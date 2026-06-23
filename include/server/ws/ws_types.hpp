#ifndef SERVER_WS_WS_TYPES_HPP
#define SERVER_WS_WS_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ws {

/// @brief WebSocket 帧操作码（RFC 6455 第 5.2 节）
enum class OpCode : uint8_t {
    Continuation = 0x0, ///< 分片帧的后续帧
    Text          = 0x1, ///< 文本帧（UTF-8）
    Binary        = 0x2, ///< 二进制帧
    Close         = 0x8, ///< 关闭帧
    Ping          = 0x9, ///< 心跳请求
    Pong          = 0xA, ///< 心跳响应
};

/// @brief 关闭状态码（RFC 6455 第 7.4 节）
enum class CloseCode : uint16_t {
    NormalClosure          = 1000, ///< 正常关闭
    GoingAway              = 1001, ///< 端点离开（如页面关闭）
    ProtocolError          = 1002, ///< 协议错误
    UnsupportedData        = 1003, ///< 不支持的数据类型
    NoStatusReceived       = 1005, ///< 未收到状态码（内部使用，不可发送）
    AbnormalClosure        = 1006, ///< 异常关闭（内部使用，不可发送）
    InvalidFramePayloadData= 1007, ///< 帧载荷数据无效
    PolicyViolation        = 1008, ///< 策略违规
    MessageTooBig          = 1009, ///< 消息过大
    MandatoryExtension     = 1010, ///< 缺少必要扩展
    InternalServerError    = 1011, ///< 服务器内部错误
};

/// @brief 判断操作码是否为控制帧（Close/Ping/Pong）
/// 控制帧操作码最高位为 1（0x8 ~ 0xF），且载荷不超过 125 字节。
inline bool IsControlFrame(OpCode op) {
    return static_cast<uint8_t>(op) >= 0x8;
}

/// @brief 判断操作码是否合法（RFC 6455 第 5.2 节）
inline bool IsValidOpCode(uint8_t code) {
    switch (code) {
        case 0x0: case 0x1: case 0x2:
        case 0x8: case 0x9: case 0xA:
            return true;
        default:
            return false;
    }
}

/// @brief 一条已重组完成的 WebSocket 消息
struct Message {
    OpCode               opcode = OpCode::Text; ///< 消息类型
    std::vector<uint8_t> payload;               ///< 消息载荷

    bool IsText()   const { return opcode == OpCode::Text; }
    bool IsBinary() const { return opcode == OpCode::Binary; }

    /// 以字符串形式获取载荷（仅在 Text 类型时有意义）
    std::string AsString() const {
        return std::string(payload.begin(), payload.end());
    }
};

/// @brief 单个 WebSocket 会话的运行参数
struct SessionOptions {
    std::size_t max_handshake_bytes = 16 * 1024;
    std::size_t max_frame_buffer_bytes = 2 * 1024 * 1024;
    std::size_t max_message_bytes = 1 * 1024 * 1024;
    std::chrono::steady_clock::duration heartbeat_interval =
        std::chrono::seconds(15);
    std::chrono::steady_clock::duration heartbeat_timeout =
        std::chrono::seconds(45);
};

} // namespace ws

#endif // SERVER_WS_WS_TYPES_HPP
