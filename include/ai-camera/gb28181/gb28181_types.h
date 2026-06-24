#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gb28181 {

// ============================================================================
// 设备信息（用于 DeviceInfo 查询响应）
// ============================================================================
struct Gb28181DeviceInfo {
    std::string manufacturer = "ai-camera";
    std::string model        = "AI-Cam-1.0";
    std::string firmware     = "1.0.0";
    std::string serial       = "123456";
    std::string hardware_id  = "AICAM-HW-001";
    std::string device_name  = "AI Camera";
    std::string ip_addr;      // 运行时填充
    uint16_t    port    = 5060;
};

// ============================================================================
// 通道信息（用于 Catalog 查询响应）
// ============================================================================
struct Gb28181Channel {
    std::string device_id;     // 通道设备 ID（20 位）
    std::string name;          // 通道名称
    std::string manufacturer;  // 制造商
    std::string model;         // 型号
    std::string owner     = "owner";
    std::string civil_code;
    std::string address;
    std::string parental   = "0";
    std::string safety_way = "0";
    std::string register_way = "1";
    std::string cert_num;
    std::string certifiable = "0";
    std::string err_code   = "400";
    std::string end_time   = "0";
    std::string secrecy    = "0";
    std::string status     = "ON";  // ON / OFF
};

// ============================================================================
// 告警信息（用于 Alarm 主动上报）
// ============================================================================
struct Gb28181Alarm {
    std::string device_id;     // 设备 ID
    int        alarm_method = 5;  // 5=视频丢失，6=移动侦测，...
    int        alarm_type   = 1;  // 告警类型
    std::string alarm_time;      // 告警时间（YYYY-MM-DDTHH:MM:SS）
    std::string alarm_description; // 告警描述
};

// ============================================================================
// 媒体会话状态
// ============================================================================
enum class MediaSessionState {
    IDLE,       // 空闲
    INVITING,   // 邀请中
    STREAMING,  // 推流中
    STOPPING    // 停止中
};

// ============================================================================
// INVITE SDP 解析结果（平台发来的媒体参数）
// ============================================================================
struct InviteMediaInfo {
    std::string platform_ip;   // 平台接收 IP
    uint16_t    platform_port = 0;  // 平台接收 RTP 端口
    uint32_t    ssrc       = 0;  // SSRC（来自 SDP y= 字段，10 位十进制）
    uint8_t     payload_type = 96; // RTP payload type（PS = 96）
    uint32_t    rtp_clock_rate = 90000; // RTP 时钟频率（H.264 = 90kHz）
};

} // namespace gb28181
