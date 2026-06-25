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
    std::cout << "[SIP] SipTransport created, local port: " << local_port << std::endl;
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
    
    std::cout << "[SIP] SipTransport started, listening on port: " 
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
    
    std::cout << "[SIP] SipTransport stopped" << std::endl;
}

bool SipTransport::SendMessage(const asio::ip::udp::endpoint& remote_endpoint,
                               const std::string& message) {
    if (!running_ || !socket_.is_open()) {
        std::cerr << "[SIP] Send failed: transport not started" << std::endl;
        return false;
    }
    
    asio::error_code ec;
    size_t sent = socket_.send_to(asio::buffer(message), remote_endpoint, 0, ec);
    
    if (ec) {
        std::cerr << "[SIP] Send failed: " << ec.message() << std::endl;
        return false;
    }
    
    std::cout << "[SIP] Sent " << sent << " bytes to " 
              << remote_endpoint.address().to_string() << ":" 
              << remote_endpoint.port() << std::endl;
    
    // Print sent SIP message (for debugging)
    // std::cout << "[SIP] Sent message:\n" << message << std::endl;
    
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
        std::cerr << "[SIP] Invalid remote IP address: " << remote_ip << std::endl;
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
    
    // Asynchronous receive message
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
            // Operation cancelled (normal stop)
            return;
        }
        
        std::cerr << "[SIP] Receive error: " << error.message() << std::endl;
    } else {
        // Successfully received message
        std::string message(receive_buffer_.data(), bytes_transferred);
        
        std::cout << "[SIP] Received " << bytes_transferred << " bytes from "
                  << remote_endpoint_.address().to_string() << ":" 
                  << remote_endpoint_.port() << std::endl;
        
        // Print received SIP message (for debugging)
        // std::cout << "[SIP] Received message:\n" << message << std::endl;
        
        // Call callback function to process message
        if (receive_callback_) {
            receive_callback_(remote_endpoint_, message);
        }
    }
    
    // Continue to receive next message
    StartReceive();
}

// ============================================================================
// SipAgent 实现（SIP 代理主类）
// ============================================================================

SipAgent::SipAgent() {
    std::cout << "[SIP] SipAgent created" << std::endl;
}

SipAgent::~SipAgent() {
    Stop();
}

bool SipAgent::Init(const Gb28181Config& config) {
    config_ = config;
    local_port_ = config.local_sip_port;
    
    // Get local IP address
    // Note: Simple implementation here, should get all local IP addresses in production
    // Temporarily use "127.0.0.1", should get actual local IP in production
    local_ip_ = "127.0.0.1";  // TODO: Get actual local IP address
    
    std::cout << "[SIP] SipAgent initialized, device ID: " << config.device_id 
              << ", local port: " << local_port_ << std::endl;
    
    return true;
}

bool SipAgent::Start() {
    if (running_) {
        std::cout << "[SIP] SipAgent is already running" << std::endl;
        return true;
    }
    
    // Create transport layer
    transport_ = std::make_shared<SipTransport>(io_context_, local_port_);
    
    // Set receive callback
    transport_->SetReceiveCallback(
        [this](const asio::ip::udp::endpoint& remote_endpoint,
               const std::string& message) {
            HandleReceivedMessage(remote_endpoint, message);
        }
    );
    
    // Start transport layer
    if (!transport_->Start()) {
        std::cerr << "[SIP] Transport layer start failed" << std::endl;
        return false;
    }
    
    // Start IO thread
    io_thread_ = std::thread([this]() {
        try {
            io_context_.run();
        } catch (const std::exception& e) {
            std::cerr << "[SIP] IO thread error: " << e.what() << std::endl;
        }
    });
    
    running_ = true;
    
    // Send registration request
    if (!SendRegister()) {
        std::cerr << "[SIP] Register request send failed" << std::endl;
        // Don't return false, allow to continue running (may retry later)
    }
    
    std::cout << "[SIP] SipAgent started successfully" << std::endl;
    return true;
}

void SipAgent::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    registered_ = false;
    
    // Unregister (send REGISTER with Expires: 0)
    if (transport_ && transport_->IsRunning()) {
        // Construct unregister request
        SipRequest unregister_req(SipMessage::Method::REGISTER, 
                                  "sip:" + config_.server_ip + ":" + 
                                  std::to_string(config_.server_port));
        
        unregister_req.SetCallId(SipMessage::GenerateCallId(local_ip_));
        unregister_req.SetFrom(BuildFromHeader());
        unregister_req.SetTo(BuildToHeader("sip:" + config_.device_id + "@" + config_.sip_realm));
        unregister_req.SetCSeq(std::to_string(cseq_++) + " REGISTER");
        unregister_req.SetVia(BuildViaHeader());
        unregister_req.SetContact(BuildContactHeader());
        unregister_req.SetHeader("Expires", "0");  // 0 means unregister
        unregister_req.SetHeader("Content-Length", "0");
        
        std::string raw_message = unregister_req.Serialize();
        transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
        
        // Wait a short time to ensure message is sent
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Stop timers
    if (keepalive_timer_) {
        keepalive_timer_->cancel();
    }
    if (register_retry_timer_) {
        register_retry_timer_->cancel();
    }
    
    // Stop transport layer
    if (transport_) {
        transport_->Stop();
    }
    
    // Stop IO context
    io_context_.stop();
    
    // Wait for IO thread to finish
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    std::cout << "[SIP] SipAgent stopped" << std::endl;
}

bool SipAgent::SendMessage(const std::string& to, const std::string& xml_body) {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] Send failed: SipAgent not started" << std::endl;
        return false;
    }
    
    // Construct MESSAGE request
    SipRequest message_req(SipMessage::Method::MESSAGE, to);
    
    // Set necessary headers
    message_req.SetCallId(SipMessage::GenerateCallId(local_ip_));
    message_req.SetFrom(BuildFromHeader());
    message_req.SetTo(BuildToHeader(to));
    message_req.SetCSeq(std::to_string(cseq_++) + " MESSAGE");
    message_req.SetVia(BuildViaHeader());
    message_req.SetContact(BuildContactHeader());
    message_req.SetHeader("Content-Type", "Application/MANSCDP+xml");  // GB28181 XML
    message_req.SetBody(xml_body);
    message_req.SetHeader("Content-Length", std::to_string(xml_body.length()));
    
    // Serialize and send
    std::string raw_message = message_req.Serialize();
    
    std::cout << "[SIP] Sending MESSAGE to " << to << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

bool SipAgent::SendRegister() {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] Register failed: SipAgent not started" << std::endl;
        return false;
    }
    
    // Construct REGISTER request
    SipRequest register_req(
        SipMessage::Method::REGISTER,
        "sip:" + config_.server_ip + ":" + std::to_string(config_.server_port)
    );
    
    // Set necessary headers
    register_req.SetCallId(SipMessage::GenerateCallId(local_ip_));
    register_req.SetFrom(BuildFromHeader());
    register_req.SetTo(BuildToHeader("sip:" + config_.device_id + "@" + config_.sip_realm));
    register_req.SetCSeq(std::to_string(cseq_++) + " REGISTER");
    register_req.SetVia(BuildViaHeader());
    register_req.SetContact(BuildContactHeader());
    register_req.SetHeader("Expires", std::to_string(config_.expires));
    register_req.SetHeader("Content-Length", "0");
    
    // If authentication info exists, add Authorization header
    if (!auth_nonce_.empty()) {
        // Construct Authorization header
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
    
    // Serialize and send
    std::string raw_message = register_req.Serialize();
    
    std::cout << "[SIP] Sending REGISTER to " << config_.server_ip << ":" 
              << config_.server_port << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

bool SipAgent::SendInviteResponse(const std::string& call_id, const std::string& to,
                                  const std::string& sdp) {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] Send failed: SipAgent not started" << std::endl;
        return false;
    }
    
    // Construct 200 OK response
    SipResponse response(200, "OK");
    
    // Set necessary headers
    response.SetCallId(call_id);
    response.SetFrom(BuildFromHeader());
    response.SetTo(BuildToHeader(to) + ";tag=" + SipMessage::GenerateTag());
    response.SetCSeq("1 INVITE");  // TODO: Get CSeq from original request
    response.SetHeader("Content-Type", "application/sdp");
    response.SetBody(sdp);
    response.SetHeader("Content-Length", std::to_string(sdp.length()));
    
    // Serialize and send
    std::string raw_message = response.Serialize();
    
    std::cout << "[SIP] Sending INVITE 200 OK, call_id: " << call_id << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

bool SipAgent::SendByeResponse(const std::string& call_id, const std::string& to) {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] Send failed: SipAgent not started" << std::endl;
        return false;
    }
    
    // Construct 200 OK response
    SipResponse response(200, "OK");
    
    // Set necessary headers
    response.SetCallId(call_id);
    response.SetFrom(BuildFromHeader());
    response.SetTo(BuildToHeader(to));
    response.SetCSeq("1 BYE");  // TODO: Get CSeq from original request
    response.SetHeader("Content-Length", "0");
    
    // Serialize and send
    std::string raw_message = response.Serialize();
    
    std::cout << "[SIP] Sending BYE 200 OK, call_id: " << call_id << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

bool SipAgent::SendMessageResponse(const std::string& call_id, const std::string& to) {
    if (!running_ || !transport_) {
        std::cerr << "[SIP] Send failed: SipAgent not started" << std::endl;
        return false;
    }
    
    // Construct 200 OK response
    SipResponse response(200, "OK");
    
    // Set necessary headers
    response.SetCallId(call_id);
    response.SetFrom(BuildFromHeader());
    response.SetTo(BuildToHeader(to));
    response.SetCSeq("1 MESSAGE");  // TODO: Get CSeq from original request
    response.SetHeader("Content-Length", "0");
    
    // Serialize and send
    std::string raw_message = response.Serialize();
    
    std::cout << "[SIP] Sending MESSAGE 200 OK, call_id: " << call_id << std::endl;
    
    return transport_->SendMessage(config_.server_ip, config_.server_port, raw_message);
}

// ============================================================================
// 消息处理相关函数
// ============================================================================

void SipAgent::HandleReceivedMessage(const asio::ip::udp::endpoint& remote_endpoint,
                                     const std::string& raw_message) {
    // Parse SIP message
    SipMessage* message = SipMessage::Parse(raw_message);
    
    if (!message) {
        std::cerr << "[SIP] Message parse failed" << std::endl;
        return;
    }
    
    // Dispatch processing based on message type
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
    
    std::cout << "[SIP] Received request: " << SipMessage::MethodToString(method)
              << ", Call-ID: " << call_id << std::endl;
    
    switch (method) {
        case SipMessage::Method::INVITE: {
            // Handle INVITE (real-time playback request)
            std::string from = message->GetFrom();
            std::string to = message->GetTo();
            std::string sdp = message->GetBody();
            
            std::cout << "[SIP] Received INVITE, from: " << from << ", to: " << to << std::endl;
            
            // Trigger callback
            if (callbacks_.on_invite) {
                callbacks_.on_invite(from, to, call_id, sdp);
            }
            
            // Send 200 OK response (with SDP)
            // Note: Should be called by upper layer via SendInviteResponse
            // Here we send a simple 200 OK first
            SendInviteResponse(call_id, from, "v=0\r\n...");  // TODO: Construct complete SDP
            break;
        }
        
        case SipMessage::Method::BYE: {
            // Handle BYE (session termination request)
            std::cout << "[SIP] Received BYE, Call-ID: " << call_id << std::endl;
            
            // Trigger callback
            if (callbacks_.on_bye) {
                callbacks_.on_bye(call_id);
            }
            
            // Send 200 OK response
            SendByeResponse(call_id, message->GetFrom());
            break;
        }
        
        case SipMessage::Method::MESSAGE: {
            // Handle MESSAGE (XML message request)
            std::string from = message->GetFrom();
            std::string xml_body = message->GetBody();
            
            std::cout << "[SIP] Received MESSAGE, from: " << from << std::endl;
            
            // Trigger callback
            if (callbacks_.on_message) {
                callbacks_.on_message(from, xml_body);
            }
            
            // Send 200 OK response
            SendMessageResponse(call_id, from);
            break;
        }
        
        case SipMessage::Method::SUBSCRIBE: {
            // Handle SUBSCRIBE (subscription request)
            std::string from = message->GetFrom();
            std::string event_type = message->GetHeader("Event");
            
            std::cout << "[SIP] Received SUBSCRIBE, from: " << from 
                      << ", event: " << event_type << std::endl;
            
            // Trigger callback
            if (callbacks_.on_subscribe) {
                callbacks_.on_subscribe(from, call_id, event_type);
            }
            
            // Send 200 OK response
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
            // Handle ACK (acknowledgment)
            std::cout << "[SIP] Received ACK, Call-ID: " << call_id << std::endl;
            break;
        }
        
        case SipMessage::Method::CANCEL: {
            // Handle CANCEL (cancel)
            std::cout << "[SIP] Received CANCEL, Call-ID: " << call_id << std::endl;
            
            // Send 200 OK response
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
            std::cout << "[SIP] Unsupported method: " << SipMessage::MethodToString(method) << std::endl;
            
            // Send 501 Not Implemented response
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
    
    std::cout << "[SIP] Received response: " << status_code << " " << reason
              << ", Call-ID: " << call_id << std::endl;
    
    if (status_code == 200) {
        // Success response
        std::cout << "[SIP] Registration successful" << std::endl;
        registered_ = true;
        
        // Start keepalive timer
        keepalive_timer_ = std::make_shared<asio::steady_timer>(io_context_);
        KeepaliveTimerCallback(asio::error_code());
        
        // Trigger callback
        if (callbacks_.on_register_success) {
            callbacks_.on_register_success();
        }
    } else if (status_code == 401) {
        // Unauthorized, need authentication
        std::cout << "[SIP] Authentication required (401 Unauthorized)" << std::endl;
        
        // Handle authentication challenge
        HandleAuthentication(remote_endpoint, message, "REGISTER");
    } else {
        // Other errors
        std::cerr << "[SIP] Registration failed: " << status_code << " " << reason << std::endl;
        
        // Trigger callback
        if (callbacks_.on_register_failed) {
            callbacks_.on_register_failed(status_code, reason);
        }
        
        // Start registration retry timer
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
    // Get WWW-Authenticate header
    std::string www_auth = response->GetHeader("WWW-Authenticate");
    if (www_auth.empty()) {
        std::cerr << "[SIP] 401 response missing WWW-Authenticate header" << std::endl;
        return;
    }
    
    std::cout << "[SIP] WWW-Authenticate: " << www_auth << std::endl;
    
    // Parse authentication info
    SipAuthInfo auth_info = ParseWwwAuthenticate(www_auth);
    
    auth_realm_ = auth_info.realm;
    auth_nonce_ = auth_info.nonce;
    auth_opaque_ = auth_info.opaque;
    auth_qop_ = auth_info.qop;
    auth_nc_ = 1;
    
    std::cout << "[SIP] Auth info - realm: " << auth_realm_ 
              << ", nonce: " << auth_nonce_ << std::endl;
    
    // Resend REGISTER request with authentication
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
    
    std::cout << "[SIP] Sending keepalive REGISTER" << std::endl;
    
    // Send keepalive REGISTER
    SendRegister();
    
    // Reset timer
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
    
    std::cout << "[SIP] Retrying registration" << std::endl;
    
    // Resend registration request
    SendRegister();
}

} // namespace gb28181
