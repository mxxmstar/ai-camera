#pragma once

#include <cstdint>
#include <string>

namespace gb28181 {

// ============================================================================
// Gb28181Config：GB28181 模块配置结构体
// ============================================================================
struct Gb28181Config {
    // ---- 设备标识 ----
    std::string device_id;      // 20 位国标编码，如 "34020000001320000001"

    // ---- SIP 服务器（平台）信息 ----
    std::string server_ip;      // 平台 SIP 服务器 IP
    uint16_t    server_port = 5060;  // 平台 SIP 端口

    // ---- 本地 SIP 信息 ----
    uint16_t    local_sip_port = 5060;  // 本地 SIP 监听端口

    // ---- SIP 认证 ----
    std::string sip_realm;      // SIP 域，如 "3402000000"
    std::string username;       // SIP 用户名（通常为 device_id）
    std::string password;       // SIP 密码

    // ---- 媒体参数 ----
    std::string video_file;     // H.264 文件路径（用于推流）
    uint32_t    frame_rate = 25;  // 帧率

    // ---- 注册与心跳 ----
    uint32_t    expires = 3600;            // 注册有效期（秒）
    uint32_t    keepalive_interval = 30;   // 心跳间隔（秒）
    uint32_t    keepalive_retry_count = 3; // 心跳失败重试次数

    // 辅助方法：构造 SIP URI
    std::string SipUri() const {
        return "sip:" + device_id + "@" + sip_realm;
    }
};

} // namespace gb28181
