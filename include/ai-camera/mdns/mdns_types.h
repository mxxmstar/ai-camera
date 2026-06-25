/**
 * @file mdns_types.h
 * @brief mDNS 模块类型定义和常量
 * 
 * 功能：
 *   - 定义服务类型枚举
 *   - 定义协议类型枚举
 *   - 定义服务信息结构体
 *   - 定义回调签名
 * 
 * 用法：
 *   - 包含此头文件以使用 mDNS 类型
 *   - 通常不需要直接使用此文件，通过 mdns_service.h 使用
 */

#ifndef AI_CAMERA_MDNS_MDNS_TYPES_H
#define AI_CAMERA_MDNS_MDNS_TYPES_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace mdns {

// ============================================================
// 服务类型（常见的摄像头服务）
// ============================================================

/// @brief 服务类型枚举
enum class ServiceType {
    RTSP,       ///< RTSP 流媒体服务（_rtsp._tcp）
    HTTP,       ///< HTTP API 服务（_http._tcp）
    HTTPS,      ///< HTTPS API 服务（_https._tcp）
    MQTT,       ///< MQTT 服务（_mqtt._tcp）
    ONVIF,      ///< ONVIF 服务（_onvif._tcp）
    Custom      ///< 自定义服务类型
};

/// @brief 协议类型枚举
enum class Protocol {
    IPv4,       ///< 仅 IPv4
    IPv6,       ///< 仅 IPv6
    Both        ///< IPv4 + IPv6
};

// ============================================================
// DNS-SD 类型字符串映射
// ============================================================

/// @brief 将 ServiceType 转换为 DNS-SD 类型字符串
/// @param type 服务类型枚举
/// @return DNS-SD 类型字符串（如 "_rtsp._tcp"）
inline std::string ServiceTypeToString(ServiceType type) {
    switch (type) {
        case ServiceType::RTSP:   return "_rtsp._tcp";
        case ServiceType::HTTP:   return "_http._tcp";
        case ServiceType::HTTPS:  return "_https._tcp";
        case ServiceType::MQTT:   return "_mqtt._tcp";
        case ServiceType::ONVIF:  return "_onvif._tcp";
        case ServiceType::Custom: return "";  // 需要手动设置 type_str
        default:                  return "";
    }
}

/// @brief 将 DNS-SD 类型字符串转换为 ServiceType
/// @param type_str DNS-SD 类型字符串
/// @return 服务类型枚举
inline ServiceType StringToServiceType(const std::string& type_str) {
    if (type_str == "_rtsp._tcp")   return ServiceType::RTSP;
    if (type_str == "_http._tcp")   return ServiceType::HTTP;
    if (type_str == "_https._tcp")  return ServiceType::HTTPS;
    if (type_str == "_mqtt._tcp")   return ServiceType::MQTT;
    if (type_str == "_onvif._tcp")  return ServiceType::ONVIF;
    return ServiceType::Custom;
}

// ============================================================
// 服务信息结构体
// ============================================================

/// @brief 服务信息（用于注册和发现）
struct ServiceInfo {
    std::string name;       ///< 服务实例名称（如 "AI-Camera-001"）
    ServiceType type;        ///< 服务类型
    std::string type_str;    ///< 自定义服务类型字符串（当 type=Custom 时使用）
    uint16_t    port;       ///< 服务端口
    std::string host;        ///< 主机名（如 "camera.local."）
    std::string ipv4;       ///< IPv4 地址
    std::string ipv6;       ///< IPv6 地址
    std::map<std::string, std::string> txt_records;  ///< TXT 记录（键值对）

    /// @brief 获取完整的服务类型字符串
    /// @return 完整的 DNS-SD 类型字符串
    std::string GetFullTypeString() const {
        if (type == ServiceType::Custom && !type_str.empty()) {
            return type_str;
        }
        return ServiceTypeToString(type);
    }

    /// @brief 设置 TXT 记录
    /// @param key 键
    /// @param value 值
    void SetTxtRecord(const std::string& key, const std::string& value) {
        txt_records[key] = value;
    }

    /// @brief 获取 TXT 记录
    /// @param key 键
    /// @return 值（如果不存在返回空字符串）
    std::string GetTxtRecord(const std::string& key) const {
        auto it = txt_records.find(key);
        if (it != txt_records.end()) {
            return it->second;
        }
        return "";
    }
};

// ============================================================
// 回调签名定义
// ============================================================

/// @brief 发现服务时的回调
/// @param service 服务信息
using ServiceFoundHandler = std::function<void(const ServiceInfo& service)>;

/// @brief 服务消失时的回调
/// @param service_name 服务实例名称
using ServiceLostHandler = std::function<void(const std::string& service_name)>;

/// @brief 服务解析完成后的回调
/// @param service 解析后的服务信息（包含 IP 地址）
using ServiceResolvedHandler = std::function<void(const ServiceInfo& service)>;

/// @brief 注册完成回调
/// @param success 是否成功
/// @param service_name 服务实例名称
using RegisterCompleteHandler = std::function<void(bool success, const std::string& service_name)>;

// ============================================================
// 错误码定义
// ============================================================

/// @brief mDNS 错误码
enum class MdnsErrorCode {
    Success,                ///< 成功
    InitFailed,            ///< 初始化失败
    RegistrationFailed,    ///< 注册失败
    BrowseFailed,          ///< 浏览失败
    ResolveFailed,         ///< 解析失败
    Timeout,               ///< 超时
    NotInitialized,        ///< 未初始化
    AlreadyInitialized     ///< 已初始化
};

// ============================================================
// 常量定义
// ============================================================

constexpr const char* MDNS_MULTICAST_ADDRESS = "224.0.0.251";  ///< mDNS 多播地址（IPv4）
constexpr const char* MDNS_MULTICAST_ADDRESS_IPV6 = "ff02::fb"; ///< mDNS 多播地址（IPv6）
constexpr uint16_t MDNS_PORT = 5353;                          ///< mDNS 端口
constexpr const char* MDNS_DOMAIN = "local.";                 ///< mDNS 域名后缀

} // namespace mdns

#endif // AI_CAMERA_MDNS_MDNS_TYPES_H
