#ifndef AI_CAMERA_MQTT_MQTT_TYPES_H
#define AI_CAMERA_MQTT_MQTT_TYPES_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace mqtt {

// ============================================================
// MQTT 3.1.1 协议常量
// ============================================================

/// MQTT 协议名（CONNECT 报文中固定值）
constexpr const char* PROTOCOL_NAME = "MQTT";
/// MQTT 3.1.1 协议等级
constexpr uint8_t PROTOCOL_LEVEL_3_1_1 = 4;

// ============================================================
// 报文类型（固定头高 4 位）
// ============================================================
enum class PacketType : uint8_t {
    CONNECT     = 1,
    CONNACK    = 2,
    PUBLISH    = 3,
    PUBACK     = 4,
    PUBREC     = 5,
    PUBREL     = 6,
    PUBCOMP    = 7,
    SUBSCRIBE  = 8,
    SUBACK     = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK   = 11,
    PINGREQ    = 12,
    PINGRESP   = 13,
    DISCONNECT = 14,
};

// ============================================================
// QoS 等级
// ============================================================
enum class QoS : uint8_t {
    AT_MOST_ONCE  = 0,  ///< QoS 0：最多一次
    AT_LEAST_ONCE = 1,  ///< QoS 1：至少一次
    EXACTLY_ONCE  = 2,  ///< QoS 2：恰好一次
};

// ============================================================
// CONNACK 返回码
// ============================================================
enum class ConnectReturnCode : uint8_t {
    ACCEPTED              = 0,  ///< 连接已接受
    BAD_PROTOCOL_VERSION  = 1,  ///< 不支持的协议版本
    BAD_CLIENT_ID         = 2,  ///< 客户端 ID 不合格
    SERVER_UNAVAILABLE   = 3,  ///< 服务端不可用
    BAD_USERNAME_PASSWORD = 4,  ///< 用户名或密码格式错误
    NOT_AUTHORIZED       = 5,  ///< 未授权
};

// ============================================================
// SUBACK 返回码
// ============================================================
enum class SubAckReturnCode : uint8_t {
    QOS_0       = 0,
    QOS_1       = 1,
    QOS_2       = 2,
    FAILURE     = 0x80,  ///< 订阅失败
};

// ============================================================
// 连接配置
// ============================================================
struct MqttConfig {
    std::string broker_host   = "127.0.0.1";
    uint16_t    broker_port   = 1883;
    std::string client_id;
    std::string username;
    std::string password;
    std::string device_id     = "AICAM-001";

    /// 清理会话（Clean Session）
    bool clean_session = true;

    /// 遗嘱消息（LWT）
    std::string will_topic;
    std::string will_payload;
    QoS          will_qos    = QoS::AT_MOST_ONCE;
    bool         will_retain = false;

    /// Keep-Alive 间隔（秒）
    uint16_t keep_alive_seconds = 60;

    /// 自动重连参数
    std::chrono::milliseconds reconnect_interval = std::chrono::seconds(5);
    std::chrono::milliseconds max_reconnect_interval = std::chrono::seconds(60);
};

// ============================================================
// Topic 构建工具
// ============================================================
struct Topics {
    static std::string Status(const std::string& device_id);
    static std::string PropertyPost(const std::string& device_id);
    static std::string EventPost(const std::string& device_id);
    static std::string CommandDown(const std::string& device_id);
    static std::string CommandResp(const std::string& device_id);
    static std::string OtaNotify(const std::string& device_id);
    static std::string OtaProgress(const std::string& device_id);
};

// ============================================================
// 回调类型定义
// ============================================================

/// 连接状态变化回调（connected = true 表示已连接）
using ConnectHandler = std::function<void(bool connected)>;

/// 收到 PUBLISH 消息回调
/// @param topic   发布主题
/// @param payload 消息载荷
/// @param qos     QoS 等级
/// @param retain  是否为保留消息
using MessageHandler = std::function<void(
    const std::string& topic,
    const std::string& payload,
    QoS qos,
    bool retain
)>;

/// 命令下发回调
/// @param msg_id  消息 ID（用于响应）
/// @param cmd     命令名称
/// @param params  JSON 格式的参数
using CommandHandler = std::function<void(
    const std::string& msg_id,
    const std::string& cmd,
    const std::string& params
)>;

/// OTA 升级通知回调
/// @param version 新固件版本
/// @param url     下载地址
/// @param md5     文件 MD5 校验值
/// @param size    文件大小（字节）
using OtaHandler = std::function<void(
    const std::string& version,
    const std::string& url,
    const std::string& md5,
    uint64_t size
)>;

// ============================================================
// 错误码
// ============================================================
enum class MqttErrorCode {
    SUCCESS,
    CONNECTION_FAILED,
    PROTOCOL_ERROR,
    TIMEOUT,
    DISCONNECTED,
};

} // namespace mqtt

#endif // AI_CAMERA_MQTT_MQTT_TYPES_H
