#include "ai-camera/serial/serial_protocol.h"

#include <algorithm>
#include <sstream>

namespace serial {

// ============================================================================
// LineProtocol
// ============================================================================

LineProtocol::LineProtocol(const std::string& delimiter)
    : delimiter_(delimiter) {
}

std::vector<std::string> LineProtocol::OnData(const char* data, size_t len) {
    std::vector<std::string> frames;
    buffer_.append(data, len);

    // 按分隔符分帧
    size_t pos = 0;
    while ((pos = buffer_.find(delimiter_)) != std::string::npos) {
        // 提取一帧（不含分隔符）
        frames.push_back(buffer_.substr(0, pos));
        buffer_.erase(0, pos + delimiter_.size());
    }

    return frames;
}

std::string LineProtocol::WrapFrame(const std::string& payload) {
    return payload + delimiter_;
}

// ============================================================================
// LengthProtocol
// ============================================================================

LengthProtocol::LengthProtocol(unsigned int field_size,
                               bool         big_endian,
                               unsigned int offset,
                               unsigned int max_frame)
    : field_size_(field_size)
    , big_endian_(big_endian)
    , offset_(offset)
    , max_frame_(max_frame)
    , buffer_() {
}

std::vector<std::string> LengthProtocol::OnData(const char* data, size_t len) {
    std::vector<std::string> frames;
    buffer_.append(data, len);

    while (buffer_.size() >= offset_ + field_size_) {
        // 读取长度字段
        uint32_t payload_len = 0;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(buffer_.data()) + offset_;

        if (big_endian_) {
            for (unsigned int i = 0; i < field_size_; ++i) {
                payload_len = (payload_len << 8) | p[i];
            }
        } else {
            for (unsigned int i = 0; i < field_size_; ++i) {
                payload_len |= static_cast<uint32_t>(p[i]) << (8 * i);
            }
        }

        // 检查帧长度是否合法
        if (payload_len > max_frame_) {
            // 异常数据，清空缓冲区防止内存耗尽
            buffer_.clear();
            return frames;
        }

        // 完整帧大小 = offset + field_size + payload_len
        size_t frame_size = offset_ + field_size_ + payload_len;
        if (buffer_.size() < frame_size) {
            // 数据不完整，等待更多数据
            break;
        }

        // 提取完整帧
        frames.push_back(buffer_.substr(0, frame_size));
        buffer_.erase(0, frame_size);
    }

    return frames;
}

// ============================================================================
// RawProtocol
// ============================================================================

std::vector<std::string> RawProtocol::OnData(const char* data, size_t len) {
    // 透传模式：所有数据作为一帧返回
    return { std::string(data, len) };
}

// ============================================================================
// CreateProtocol 工厂函数
// ============================================================================

std::unique_ptr<SerialProtocol> CreateProtocol(const std::string&  protocol_name,
                                               const SerialConfig& config) {
    if (protocol_name == "line") {
        auto proto = std::make_unique<LineProtocol>();
        // 注意：LineProtocol 默认分隔符为 "\n"
        // 若需自定义分隔符，需在 SerialConfig 中添加对应字段
        return proto;
    }

    if (protocol_name == "length") {
        return std::make_unique<LengthProtocol>(
            config.length_field_size,
            config.length_big_endian,
            config.length_offset,
            config.max_frame_size
        );
    }

    if (protocol_name == "raw") {
        return std::make_unique<RawProtocol>();
    }

    // 未知协议
    return nullptr;
}

} // namespace serial
