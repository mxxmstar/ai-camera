#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include "onvif_types.h"

namespace asio {
    class io_context;
    namespace ip {
        class udp;
    }
}

namespace onvif {

// ============================================================================
// WS-Discovery：UDP 多播设备发现
// 监听 239.255.255.250:3702，响应 Probe 请求
// ============================================================================
class WSDiscovery {
public:
    explicit WSDiscovery(const ServiceConfig& config);
    ~WSDiscovery();

    // ------------------------------------------------------------------
    // 启动/停止 UDP 多播监听
    // ------------------------------------------------------------------
    bool Start();
    void Stop();

    // ------------------------------------------------------------------
    // 设置设备 Scopes（用于 ProbeMatches 响应）
    // ------------------------------------------------------------------
    void SetScopes(const std::vector<std::string>& scopes);
    void SetScopes(const std::string& scopes);  // 分号分隔

    // ------------------------------------------------------------------
    // 设置设备类型（Types 字段，用于 ProbeMatches）
    // ------------------------------------------------------------------
    void SetTypes(const std::string& types);  // 如 "tdn:NetworkVideoTransmitter"

private:
    // ------------------------------------------------------------------
    // 内部：启动异步接收（循环调用）
    // ------------------------------------------------------------------
    void DoReceive();

    // ------------------------------------------------------------------
    // 内部：处理接收到的 Probe 报文
    // ------------------------------------------------------------------
    void HandleProbe(const char* data, size_t len);

    // ------------------------------------------------------------------
    // 内部：构造 ProbeMatch SOAP 报文
    // ------------------------------------------------------------------
    std::string BuildProbeMatch(const std::string& relates_to,
                                const std::string& endpoint_ref,
                                const std::string& xaddrs) const;

    const ServiceConfig&            config_;
    std::vector<std::string>       scopes_;
    std::string                     types_;       // ONVIF 设备类型
    std::string                     device_id_;   // 设备唯一 ID（用于 EndpointReference）

    std::thread                     worker_thread_;
    std::atomic<bool>              running_{false};

    // ASIO 相关（在 worker_thread_ 中使用）
    std::unique_ptr<asio::io_context>  io_ctx_;
    void*                           socket_raw_ = nullptr;  // 实际类型为 asio::ip::udp::socket*
    std::vector<char>               recv_buf_;
    // 发送端地址（在 DoReceive  handler 中使用）
    void*                           sender_ep_raw_ = nullptr;
};

} // namespace onvif
