#include "gb28181/sip_agent.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <chrono>

namespace gb28181 {

// ============================================================================
// SipTransport 实现（UDP 传输层）
// ============================================================================

SipTransport::SipTransport(asio::io_context& io_context, uint16_t local_port)
    : io_context_(io_context)
    , socket_(io_context_, asio::ip::udp::endpoint(asio::ip::udp::v4(), local_port)) {
    std::cout << "[SIP] SipTransport 创建，本地端口: " << local_port << std::endl;
}

SipTransport::~SipTransport() {
    Stop();
}

bool SipTransport::Start() {
    if (running_) {
        return true;
    }
    
    running_ = true;
    StartReceive();
    
    std::cout << "[SIP] SipTransport 启动，监听端口: " 
              << socket_.local_endpoint().port() << std::endl;
    return true;
}

void SipTransport::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (socket_.is_open()) {
        asio::error_code ec;
        socket_.close(ec);
    }
    
    std::cout << "[SIP] SipTransport 停止" << std::endl;
}

bool SipTransport::SendMessage(const asio::ip::udp::endpoint& remote_endpoint,
                               const std::string& message) {
    if (!running_ || !socket_.is_open()) {
        std::cerr << "[SIP] 发送失败：传输层未启动" << std::endl;
        return false;
    }
    
    asio::error_code ec;
    size_t sent = socket_.send_to(asio::buffer(message), remote_endpoint, 0, ec);
    
    if (ec) {
        std::cerr << "[SIP] 发送失败: " << ec.message() << std::endl;
        return false;
    }
    
    std::cout << "[SIP] 发送 " << sent << " 字节到 " 
              << remote_endpoint.address().to_string() << ":" 
              << remote_endpoint.port() << std::endl;
    
    // 打印发送的 SIP 消息（调试用）
    // std::cout << "[SIP] 发送消息:\n" << message << std::endl;
    
    return true;
}

bool SipTransport::SendMessage(const std::string& remote_ip, uint16_t remote_port,
                               const std::string& message) {
    asio::error_code ec;
    asio::ip::udp::endpoint remote_endpoint(
        asio::ip::make_address(remote_ip, ec), 
        remote_port
    );
    
    if (ec) {
        std::cerr << "[SIP] 无效的远程 IP 地址: " << remote_ip << std::endl;
        return false;
    }
    
    return SendMessage(remote_endpoint, message);
}

std::string SipTransport::GetLocalAddress() const {
    if (!socket_.is_open()) {
        return "";
    }
    
    asio::error_code ec;
    auto endpoint = socket_.local_endpoint(ec);
    
    if (ec) {
        return "";
    }
    
    return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

void SipTransport::StartReceive() {
    if (!running_ || !socket_.is_open()) {
        return;
    }
    
    // 异步接收消息
    socket_.async_receive_from(
        asio::buffer(receive_buffer_),
        remote_endpoint_,
        [this](const asio::error_code& error, size_t bytes_transferred) {
            HandleReceive(error, bytes_transferred);
        }
    );
}

void SipTransport::HandleReceive(const asio::error_code& error, size_t bytes_transferred) {
    if (!running_) {
        return;
    }
    
    if (error) {
        if (error == asio::error::operation_aborted) {
            // 操作被取消（正常停止）
            return;
        }
        
        std::cerr << "[SIP] 接收错误: " << error.message() << std::endl;
    } else {
        // 成功接收到消息
        std::string message(receive_buffer_.data(), bytes_transferred);
        
        std::cout << "[SIP] 收到 " << bytes_transferred << " 字节来自 "
                  << remote_endpoint_.address().to_string() << ":" 
                  << remote_endpoint_.port() << std::endl;
        
        // 打印接收到的 SIP 消息（调试用）
        // std::cout << "[SIP] 接收消息:\n" << message << std::endl;
        
        // 调用回调函数处理消息
        if (receive_callback_) {
            receive_callback_(remote_endpoint_, message);
        }
    }
    
    // 继续接收下一条消息
    StartReceive();
}

// ============================================================================
// SipAgent 实现（SIP 代理主类）
// ============================================================================

SipAgent::SipAgent() {
    std::cout << "[SIP] SipAgent 创建" << std::endl;
}

SipAgent::~SipAgent() {
    Stop();
}

bool SipAgent::Init(const Gb28181Config& config) {
    config_ = config;
    local_port_ = config.local_sip_port;
    
    // 获取本地 IP 地址
    // 注意：这里使用简单的实现，实际应该获取本机所有 IP 地址
    // 这里暂时使用 "127.0.0.1"，实际应该获取本机 IP
    local_ip_ = "127.0.0.1";  // TODO: 获取本机实际 IP 地址
    
    std::cout << "[SIP] SipAgent 初始化，设备ID: " << config.device_id 
              << ", 本地端口: " << local_port_ << std::endl;
    
    return true;
}

bool SipAgent::Start() {
    if (running_) {
        std::cout << "[SIP] SipAgent 已经在运行" << std::endl;
        return true;
    }
    
    // 创建传输层
    transport_ = std::make_shared<SipTransport>(io_context_, local_port_);
    
    // 设置接收回调
    transport_->SetReceiveCallback(
        [this](const asio::ip::udp::endpoint& remote_endpoint,
               const std::string& message) {
            HandleReceivedMessage(remote_endpoint, message);
        }
    );
    
    // 启动传输层
    if (!transport_->Start()) {
        std::cerr << "[SIP] 传输层启动失败" << std::endl;
        return false;
    }
    
    // 启动 IO 线程
    io_thread_ = std::thread([this]() {
        try {
            io_context_.run();
        } catch (const std::exception& e) {
            std::cerr << "[SIP] IO 线程错误: " << e.what() << std::endl;
        }
    });
    
    running_ = true;
    
    // 发送注册请求
    if (!SendRegister()) {
        std::cerr << "[SIP] 注册请求发送失败" << std::endl;
        // 不返回 false，允许继续运行（可能稍后重试）
    }
    
    std::cout << "[SIP] SipAgent 启动成功" << std::endl;
    return true;
}

void SipAgent::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    registered_ = false;
    
    // 取消注册（发送 REGISTER  with Expires: 0）
    if (transport_ && transport_->IsRunning()) {
        // 构造取消注册请求
        SipRequest unregister_req(SipMessage::Method::REGISTER, 
                                  "sip:" + config_.server_ip + ":" + 
                                  std::to_string(config_.server_port));
        
        unregister_req.SetCallId(SipMessage::GenerateCallId(local_ip_));
        unregister_req.SetFrom(BuildFromHeader());
        unregister_req.SetTo(BuildToHeader("sip:" + config_.device_id + "@" + config_.sip_realm));
        unregister_req.SetCSeq(std::to_string(cseq_++) + " REGISTER");
        unregister_req.SetVia(BuildViaHeader());
        unregister_req.SetContact(BuildContactHeader());
        unregister_req.SetHeader("Expires", "0");  // 0 表示取消注册
        unregister_req.SetHeader("Content-Length", "0");
        
        std::string raw_message = unregister_req.Serialize();
        transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
        
        // 等待一小段时间确保消息发送出去
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 停止定时器
    if (keepalive_timer_) {
        keepalive_timer_->cancel();
    }
    if (register_retry_timer_) {
        register_retry_timer_->cancel();
    }
    
    // 停止传输层
    if (transport_) {
        transport_->Stop();
    }
    
    // 停止 IO 上下文
    io_context_.stop();
    
    // 等待 IO 线程结束
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    std::cout << "[SIP] SipAgent 停止" << std::endl;
}

bool SipAgent::SendMessage(const std::string& to, const std::string& xml_body) {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] 发送失败：SipAgent 未启动" << std::endl;
        return false;
    }
    
    // 构造 MESSAGE 请求
    SipRequest message_req(SipMessage::Method::MESSAGE, to);
    
    // 设置必要的头字段
    message_req.SetCallId(SipMessage::GenerateCallId(local_ip_));
    message_req.SetFrom(BuildFromHeader());
    message_req.SetTo(BuildToHeader(to));
    message_req.SetCSeq(std::to_string(cseq_++) + " MESSAGE");
    message_req.SetVia(BuildViaHeader());
    message_req.SetContact(BuildContactHeader());
    message_req.SetHeader("Content-Type", "Application/MANSCDP+xml");  // GB28181 XML
    message_req.SetBody(xml_body);
    message_req.SetHeader("Content-Length", std::to_string(xml_body.length()));
    
    // 序列化并发送
    std::string raw_message = message_req.Serialize();
    
    std::cout << "[SIP] 发送 MESSAGE 到 " << to << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

bool SipAgent::SendRegister() {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] 注册失败：SipAgent 未启动" << std::endl;
        return false;
    }
    
    // 构造 REGISTER 请求
    SipRequest register_req(
        SipMessage::Method::REGISTER,
        "sip:" + config_.server_ip + ":" + std::to_string(config_.server_port)
    );
    
    // 设置必要的头字段
    register_req.SetCallId(SipMessage::GenerateCallId(local_ip_));
    register_req.SetFrom(BuildFromHeader());
    register_req.SetTo(BuildToHeader("sip:" + config_.device_id + "@" + config_.sip_realm));
    register_req.SetCSeq(std::to_string(cseq_++) + " REGISTER");
    register_req.SetVia(BuildViaHeader());
    register_req.SetContact(BuildContactHeader());
    register_req.SetHeader("Expires", std::to_string(config_.expires));
    register_req.SetHeader("Content-Length", "0");
    
    // 如果有认证信息，添加 Authorization 头
    if (!auth_nonce_.empty()) {
        // 构造 Authorization 头
        std::string uri = "sip:" + config_.server_ip + ":" + std::to_string(config_.server_port);
        std::string response = CalculateSipDigestResponse(
            config_.username,
            config_.password,
            auth_realm_,
            auth_nonce_,
            "REGISTER",
            uri,
            std::to_string(auth_nc_),
            SipMessage::GenerateBranch(),  // cnonce
            auth_qop_
        );
        
        std::ostringstream auth_header;
        auth_header << "Digest username=\"" << config_.username << "\", "
                    << "realm=\"" << auth_realm_ << "\", "
                    << "nonce=\"" << auth_nonce_ << "\", "
                    << "uri=\"" << uri << "\", "
                    << "response=\"" << response << "\"";
        
        if (!auth_qop_.empty()) {
            auth_header << ", qop=" << auth_qop_
                        << ", nc=" << std::setw(8) << std::setfill('0') << auth_nc_
                        << ", cnonce=\"" << SipMessage::GenerateBranch() << "\"";
            auth_nc_++;
        }
        
        register_req.SetHeader("Authorization", auth_header.str());
    }
    
    // 序列化并发送
    std::string raw_message = register_req.Serialize();
    
    std::cout << "[SIP] 发送 REGISTER 到 " << config_.server_ip << ":" 
              << config_.server_port << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

bool SipAgent::SendInviteResponse(const std::string& call_id, const std::string& to,
                                  const std::string& sdp) {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] 发送失败：SipAgent 未启动" << std::endl;
        return false;
    }
    
    // 构造 200 OK 响应
    SipResponse response(200, "OK");
    
    // 设置必要的头字段
    response.SetCallId(call_id);
    response.SetFrom(BuildFromHeader());
    response.SetTo(BuildToHeader(to) + ";tag=" + SipMessage::GenerateTag());
    response.SetCSeq("1 INVITE");  // TODO: 从原始请求获取 CSeq
    response.SetHeader("Content-Type", "application/sdp");
    response.SetBody(sdp);
    response.SetHeader("Content-Length", std::to_string(sdp.length()));
    
    // 序列化并发送
    std::string raw_message = response.Serialize();
    
    std::cout << "[SIP] 发送 INVITE 200 OK，call_id: " << call_id << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

bool SipAgent::SendByeResponse(const std::string& call_id, const std::string& to) {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] 发送失败：SipAgent 未启动" << std::endl;
        return false;
    }
    
    // 构造 200 OK 响应
    SipResponse response(200, "OK");
    
    // 设置必要的头字段
    response.SetCallId(call_id);
    response.SetFrom(BuildFromHeader());
    response.SetTo(BuildToHeader(to));
    response.SetCSeq("1 BYE");  // TODO: 从原始请求获取 CSeq
    response.SetHeader("Content-Length", "0");
    
    // 序列化并发送
    std::string raw_message = response.Serialize();
    
    std::cout << "[SIP] 发送 BYE 200 OK，call_id: " << call_id << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

bool SipAgent::SendMessageResponse(const std::string& call_id, const std::string& to) {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] 发送失败：SipAgent 未启动" << std::endl;
        return false;
    }
    
    // 构造 200 OK 响应
    SipResponse response(200, "OK");
    
    // 设置必要的头字段
    response.SetCallId(call_id);
    response.SetFrom(BuildFromHeader());
    response.SetTo(BuildToHeader(to));
    response.SetCSeq("1 MESSAGE");  // TODO: 从原始请求获取 CSeq
    response.SetHeader("Content-Length", "0");
    
    // 序列化并发送
    std::string raw_message = response.Serialize();
    
    std::cout << "[SIP] 发送 MESSAGE 200 OK，call_id: " << call_id << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

// ============================================================================
// 消息处理相关函数
// ============================================================================

void SipAgent::HandleReceivedMessage(const asio::ip::udp::endpoint& remote_endpoint,
                                     const std::string& raw_message) {
    // 解析 SIP 消息
    SipMessage* message = SipMessage::Parse(raw_message);
    
    if (!message) {
        std::cerr << "[SIP] 消息解析失败" << std::endl;
        return;
    }
    
    // 根据消息类型分发处理
    if (message->GetType() == SipMessage::Type::REQUEST) {
        HandleRequest(remote_endpoint, message);
    } else {
        HandleResponse(remote_endpoint, message);
    }
    
    delete message;
}

void SipAgent::HandleRequest(const asio::ip::udp::endpoint& remote_endpoint,
                             SipMessage* message) {
    SipMessage::Method method = message->GetMethod();
    std::string call_id = message->GetCallId();
    
    std::cout << "[SIP] 收到请求: " << SipMessage::MethodToString(method)
              << ", Call-ID: " << call_id << std::endl;
    
    switch (method) {
        case SipMessage::Method::INVITE: {
            // 处理 INVITE（实时点播请求）
            std::string from = message->GetFrom();
            std::string to = message->GetTo();
            std::string sdp = message->GetBody();
            
            std::cout << "[SIP] 收到 INVITE，from: " << from << ", to: " << to << std::endl;
            
            // 触发回调
            if (callbacks_.on_invite) {
                callbacks_.on_invite(from, to, call_id, sdp);
            }
            
            // 发送 200 OK 响应（带 SDP）
            // 注意：这里应该由上层调用 SendInviteResponse，这里先发送一个简单的 200 OK
            SendInviteResponse(call_id, from, "v=0\r\n...");  // TODO: 构造完整的 SDP
            break;
        }
        
        case SipMessage::Method::BYE: {
            // 处理 BYE（结束会话请求）
            std::cout << "[SIP] 收到 BYE，Call-ID: " << call_id << std::endl;
            
            // 触发回调
            if (callbacks_.on_bye) {
                callbacks_.on_bye(call_id);
            }
            
            // 发送 200 OK 响应
            SendByeResponse(call_id, message->GetFrom());
            break;
        }
        
        case SipMessage::Method::MESSAGE: {
            // 处理 MESSAGE（XML 报文请求）
            std::string from = message->GetFrom();
            std::string xml_body = message->GetBody();
            
            std::cout << "[SIP] 收到 MESSAGE，from: " << from << std::endl;
            
            // 触发回调
            if (callbacks_.on_message) {
                callbacks_.on_message(from, xml_body);
            }
            
            // 发送 200 OK 响应
            SendMessageResponse(call_id, from);
            break;
        }
        
        case SipMessage::Method::SUBSCRIBE: {
            // 处理 SUBSCRIBE（订阅请求）
            std::string from = message->GetFrom();
            std::string event_type = message->GetHeader("Event");
            
            std::cout << "[SIP] 收到 SUBSCRIBE，from: " << from 
                      << ", event: " << event_type << std::endl;
            
            // 触发回调
            if (callbacks_.on_subscribe) {
                callbacks_.on_subscribe(from, call_id, event_type);
            }
            
            // 发送 200 OK 响应
            SipResponse response(200, "OK");
            response.SetCallId(call_id);
            response.SetFrom(BuildFromHeader());
            response.SetTo(BuildToHeader(from));
            response.SetCSeq(message->GetCSeq());
            response.SetHeader("Content-Length", "0");
            
            std::string raw_response = response.Serialize();
            transport_->SendMessage(remote_endpoint, raw_response);
            break;
        }
        
        case SipMessage::Method::ACK: {
            // 处理 ACK（确认）
            std::cout << "[SIP] 收到 ACK，Call-ID: " << call_id << std::endl;
            break;
        }
        
        case SipMessage::Method::CANCEL: {
            // 处理 CANCEL（取消）
            std::cout << "[SIP] 收到 CANCEL，Call-ID: " << call_id << std::endl;
            
            // 发送 200 OK 响应
            SipResponse response(200, "OK");
            response.SetCallId(call_id);
            response.SetFrom(BuildFromHeader());
            response.SetTo(BuildToHeader(message->GetFrom()));
            response.SetCSeq(message->GetCSeq());
            response.SetHeader("Content-Length", "0");
            
            std::string raw_response = response.Serialize();
            transport_->SendMessage(remote_endpoint, raw_response);
            break;
        }
        
        default: {
            std::cout << "[SIP] 未支持的方法: " << SipMessage::MethodToString(method) << std::endl;
            
            // 发送 501 Not Implemented 响应
            SipResponse response(501, "Not Implemented");
            response.SetCallId(call_id);
            response.SetFrom(BuildFromHeader());
            response.SetTo(BuildToHeader(message->GetFrom()));
            response.SetCSeq(message->GetCSeq());
            response.SetHeader("Content-Length", "0");
            
            std::string raw_response = response.Serialize();
            transport_->SendMessage(remote_endpoint, raw_response);
            break;
        }
    }
}

void SipAgent::HandleResponse(const asio::ip::udp::endpoint& remote_endpoint,
                              SipMessage* message) {
    uint16_t status_code = message->GetStatusCode();
    std::string reason = message->GetReasonPhrase();
    std::string call_id = message->GetCallId();
    
    std::cout << "[SIP] 收到响应: " << status_code << " " << reason
              << ", Call-ID: " << call_id << std::endl;
    
    if (status_code == 200) {
        // 成功响应
        std::cout << "[SIP] 注册成功" << std::endl;
        registered_ = true;
        
        // 启动保活定时器
        keepalive_timer_ = std::make_shared<asio::steady_timer>(io_context_);
        KeepaliveTimerCallback(asio::error_code());
        
        // 触发回调
        if (callbacks_.on_register_success) {
            callbacks_.on_register_success();
        }
    } else if (status_code == 401) {
        // 未授权，需要认证
        std::cout << "[SIP] 需要认证（401 Unauthorized）" << std::endl;
        
        // 处理认证挑战
        HandleAuthentication(remote_endpoint, message, "REGISTER");
    } else {
        // 其他错误
        std::cerr << "[SIP] 注册失败: " << status_code << " " << reason << std::endl;
        
        // 触发回调
        if (callbacks_.on_register_failed) {
            callbacks_.on_register_failed(status_code, reason);
        }
        
        // 启动注册重试定时器
        register_retry_timer_ = std::make_shared<asio::steady_timer>(io_context_);
        register_retry_timer_->expires_after(std::chrono::seconds(10));
        register_retry_timer_->async_wait(
            [this](const asio::error_code& error) {
                RegisterRetryTimerCallback(error);
            }
        );
    }
}

void SipAgent::HandleAuthentication(const asio::ip::udp::endpoint& remote_endpoint,
                                    SipMessage* response,
                                    const std::string& original_method) {
    // 获取 WWW-Authenticate 头
    std::string www_auth = response->GetHeader("WWW-Authenticate");
    if (www_auth.empty()) {
        std::cerr << "[SIP] 401 响应缺少 WWW-Authenticate 头" << std::endl;
        return;
    }
    
    std::cout << "[SIP] WWW-Authenticate: " << www_auth << std::endl;
    
    // 解析认证信息
    SipAuthInfo auth_info = ParseWwwAuthenticate(www_auth);
    
    auth_realm_ = auth_info.realm;
    auth_nonce_ = auth_info.nonce;
    auth_opaque_ = auth_info.opaque;
    auth_qop_ = auth_info.qop;
    auth_nc_ = 1;
    
    std::cout << "[SIP] 认证信息 - realm: " << auth_realm_ 
              << ", nonce: " << auth_nonce_ << std::endl;
    
    // 重新发送带认证的 REGISTER 请求
    SendRegister();
}

// ============================================================================
// 辅助函数
// ============================================================================

std::string SipAgent::BuildViaHeader() {
    // Via: SIP/2.0/UDP 192.168.1.100:5060;rport;branch=z9hG4bK123456
    std::ostringstream oss;
    oss << "SIP/2.0/UDP " << local_ip_ << ":" << local_port_
        << ";rport;branch=" << SipMessage::GenerateBranch();
    return oss.str();
}

std::string SipAgent::BuildFromHeader() {
    // From: <sip:34020000001320000001@3402000000>;tag=123456
    std::ostringstream oss;
    oss << "<sip:" << config_.device_id << "@" << config_.sip_realm
        << ">;tag=" << SipMessage::GenerateTag();
    return oss.str();
}

std::string SipAgent::BuildToHeader(const std::string& to_uri) {
    // To: <sip:34020000002000000001@3402000000>
    std::ostringstream oss;
    oss << "<" << to_uri << ">";
    return oss.str();
}

std::string SipAgent::BuildContactHeader() {
    // Contact: <sip:34020000001320000001@192.168.1.100:5060>
    std::ostringstream oss;
    oss << "<sip:" << config_.device_id << "@" << local_ip_ << ":" << local_port_ << ">";
    return oss.str();
}

void SipAgent::KeepaliveTimerCallback(const asio::error_code& error) {
    if (error || !running_) {
        return;
    }
    
    std::cout << "[SIP] 发送保活 REGISTER" << std::endl;
    
    // 发送保活 REGISTER
    SendRegister();
    
    // 重新设置定时器
    keepalive_timer_->expires_after(std::chrono::seconds(config_.keepalive_interval));
    keepalive_timer_->async_wait(
        [this](const asio::error_code& error) {
            KeepaliveTimerCallback(error);
        }
    );
}

void SipAgent::RegisterRetryTimerCallback(const asio::error_code& error) {
    if (error || !running_) {
        return;
    }
    
    std::cout << "[SIP] 重试注册" << std::endl;
    
    // 重新发送注册请求
    SendRegister();
}

} // namespace gb28181
