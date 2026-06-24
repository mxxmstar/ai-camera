#pragma once

#include <memory>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <asio.hpp>
#include "gb28181_config.h"
#include "gb28181_types.h"
#include "sip_message.h"

namespace gb28181 {

// ============================================================================
// SipAgent：轻量级 SIP 代理，使用 asio 实现 GB28181 SIP 信令
// 
// 功能：
// 1. 发送 SIP REGISTER 请求（支持认证）
// 2. 发送 SIP MESSAGE 请求（用于传输 GB28181 XML 报文）
// 3. 接收和处理 SIP 请求（INVITE、MESSAGE、BYE 等）
// 4. 发送 SIP 响应
// 5. 维持注册保活
// ============================================================================

// SIP 事件回调接口（供上层模块实现）
struct SipEventCallbacks {
    // 收到 INVITE（实时点播请求）
    // from: 发起方 URI
    // to: 接收方 URI
    // call_id: 呼叫 ID
    // sdp: SDP 描述
    std::function<void(const std::string& from, const std::string& to,
                       const std::string& call_id, const std::string& sdp)>
        on_invite;
    
    // 收到 BYE（结束会话请求）
    // call_id: 呼叫 ID
    std::function<void(const std::string& call_id)> on_bye;
    
    // 收到 MESSAGE（XML 报文请求，如 Catalog、DeviceInfo 等）
    // from: 发送方 URI
    // xml_body: XML 消息体
    std::function<void(const std::string& from, const std::string& xml_body)>
        on_message;
    
    // 注册成功
    std::function<void()> on_register_success;
    
    // 注册失败
    std::function<void(int status_code, const std::string& reason)> on_register_failed;
    
    // 收到 SUBSCRIBE（订阅请求）
    // from: 发起方 URI
    // call_id: 呼叫 ID
    // event_type: 事件类型（如 "Catalog"）
    std::function<void(const std::string& from, const std::string& call_id,
                       const std::string& event_type)> on_subscribe;
};

// SIP 传输层，使用 asio UDP 套接字
class SipTransport : public std::enable_shared_from_this<SipTransport> {
public:
    // 接收消息的回调函数类型
    // remote_endpoint: 发送方端点
    // message: 接收到的 SIP 消息
    using ReceiveCallback = std::function<void(
        const asio::ip::udp::endpoint& remote_endpoint,
        const std::string& message
    )>;

    SipTransport(asio::io_context& io_context, uint16_t local_port);
    ~SipTransport();

    // 启动传输层（开始接收消息）
    bool Start();
    
    // 停止传输层
    void Stop();

    // 发送 SIP 消息到指定端点
    bool SendMessage(const asio::ip::udp::endpoint& remote_endpoint,
                     const std::string& message);
    
    // 发送 SIP 消息到指定地址（字符串形式）
    bool SendMessage(const std::string& remote_ip, uint16_t remote_port,
                     const std::string& message);

    // 设置接收回调
    void SetReceiveCallback(ReceiveCallback callback) {
        receive_callback_ = callback;
    }

    // 获取本地端点
    asio::ip::udp::endpoint GetLocalEndpoint() const {
        return socket_.local_endpoint();
    }

    // 获取本地地址字符串
    std::string GetLocalAddress() const;

    // 是否正在运行
    bool IsRunning() const { return running_; }

private:
    // 异步接收消息
    void StartReceive();

    // 接收完成回调
    void HandleReceive(const asio::error_code& error, size_t bytes_transferred);

    asio::io_context& io_context_;
    asio::ip::udp::socket socket_;
    std::array<char, 65535> receive_buffer_;  // SIP 消息最大 64KB
    asio::ip::udp::endpoint remote_endpoint_;
    ReceiveCallback receive_callback_;
    std::atomic<bool> running_{false};
};

// ============================================================================
// SipAgent：SIP 代理主类
// ============================================================================
class SipAgent {
public:
    SipAgent();
    ~SipAgent();

    // 初始化 SIP 代理
    // config: GB28181 配置
    // 返回: 是否初始化成功
    bool Init(const Gb28181Config& config);

    // 启动 SIP 代理（开始监听和注册）
    bool Start();

    // 停止 SIP 代理
    void Stop();

    // 是否正在运行
    bool IsRunning() const { return running_; }

    // 发送 SIP MESSAGE（XML 报文）
    // to: 目标 URI（如 sip:34020000002000000001@3402000000）
    // xml_body: XML 消息体
    // 返回: 是否发送成功
    bool SendMessage(const std::string& to, const std::string& xml_body);

    // 发送 SIP REGISTER（保活注册）
    // 返回: 是否发送成功
    bool SendRegister();

    // 发送 INVITE 响应（200 OK with SDP）
    // call_id: 呼叫 ID
    // to: 接收方 URI
    // sdp: SDP 描述
    // 返回: 是否发送成功
    bool SendInviteResponse(const std::string& call_id, const std::string& to,
                            const std::string& sdp);

    // 发送 BYE 响应（200 OK）
    // call_id: 呼叫 ID
    // to: 接收方 URI
    // 返回: 是否发送成功
    bool SendByeResponse(const std::string& call_id, const std::string& to);

    // 发送 MESSAGE 响应（200 OK）
    // call_id: 呼叫 ID
    // to: 接收方 URI
    // 返回: 是否发送成功
    bool SendMessageResponse(const std::string& call_id, const std::string& to);

    // 设置事件回调
    void SetCallbacks(const SipEventCallbacks& cb) { callbacks_ = cb; }

    // 获取当前配置
    const Gb28181Config& GetConfig() const { return config_; }

    // 是否已注册
    bool IsRegistered() const { return registered_; }

private:
    // 处理接收到的 SIP 消息
    void HandleReceivedMessage(const asio::ip::udp::endpoint& remote_endpoint,
                               const std::string& raw_message);

    // 处理 SIP 请求
    void HandleRequest(const asio::ip::udp::endpoint& remote_endpoint,
                       SipMessage* message);

    // 处理 SIP 响应
    void HandleResponse(const asio::ip::udp::endpoint& remote_endpoint,
                        SipMessage* message);

    // 处理认证挑战（401 Unauthorized）
    void HandleAuthentication(const asio::ip::udp::endpoint& remote_endpoint,
                              SipMessage* response,
                              const std::string& original_method);

    // 构造并发送带认证的请求
    void ResendWithAuth(const asio::ip::udp::endpoint& remote_endpoint,
                        const SipRequest& original_request,
                        const SipAuthInfo& auth_info);

    // 构造 SIP Via 头
    std::string BuildViaHeader();

    // 构造 SIP From 头
    std::string BuildFromHeader();

    // 构造 SIP To 头
    std::string BuildToHeader(const std::string& to_uri);

    // 构造 SIP Contact 头
    std::string BuildContactHeader();

    // 保活定时器回调
    void KeepaliveTimerCallback(const asio::error_code& error);

    // 注册重试定时器回调
    void RegisterRetryTimerCallback(const asio::error_code& error);

    Gb28181Config config_;                // GB28181 配置
    SipEventCallbacks callbacks_;         // 事件回调
    std::shared_ptr<SipTransport> transport_;  // SIP 传输层
    asio::io_context io_context_;         // asio IO 上下文
    std::thread io_thread_;               // IO 线程
    std::atomic<bool> running_{false};    // 是否正在运行
    std::atomic<bool> registered_{false}; // 是否已注册
    std::atomic<uint32_t> cseq_{1};       // CSeq 计数器

    // 认证相关
    std::string auth_realm_;              // 认证域
    std::string auth_nonce_;              // 服务器随机数
    std::string auth_opaque_;             // 不透明值
    std::string auth_qop_;                // QoP
    uint32_t auth_nc_ = 1;               // 随机数计数

    // 定时器
    std::shared_ptr<asio::steady_timer> keepalive_timer_;  // 保活定时器
    std::shared_ptr<asio::steady_timer> register_retry_timer_;  // 注册重试定时器

    // 本地地址信息
    std::string local_ip_;                // 本地 IP 地址
    uint16_t local_port_;                 // 本地端口
};

} // namespace gb28181
