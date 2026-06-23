#ifndef SERVER_WS_WS_FRAME_HPP
#define SERVER_WS_WS_FRAME_HPP

#include "server/ws/ws_types.hpp"

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

namespace ws {

/// @brief WebSocket 帧编解码（RFC 6455 第 5 节）
///
/// 帧格式（单位：bit）：
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
///  0                   1                   2                   3
///  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
/// +-+-+-+-+-------+-+-------------+-------------------------------+
/// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
/// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
/// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
/// | |1|2|3|       |K|             |                               |
/// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
/// |     Extended payload length continued, if payload len == 127  |
/// + - - - - - - - - - - - - - - - +-------------------------------+
/// |                               |Masking-key, if MASK set to 1  |
/// +-------------------------------+-------------------------------+
/// | Masking-key (continued)       |          Payload Data         |
/// +-------------------------------- - - - - - - - - - - - - - - - +
/// :                     Payload Data continued ...                :
/// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
/// |                     Payload Data continued ...                |
/// +---------------------------------------------------------------+
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
///
/// 说明：
/// - FIN (1 bit)：是否为消息的最后一个分片
/// - RSV1~3 (3 bits)：保留，必须为 0（除非协商了扩展）
/// - opcode (4 bits)：帧类型
/// - MASK (1 bit)：客户端→服务器的帧必须掩码；服务器→客户端不得掩码
/// - Payload len (7 bits)：
///     0~125  ：载荷长度直接用这 7 位表示
///     126    ：后接 2 字节（大端）为真实长度
///     127    ：后接 8 字节（大端）为真实长度
/// - Masking-key (32 bits)：当 MASK=1 时存在，用于异或解掩码
/// - Payload Data：实际数据
namespace frame {

/// 帧头解析的中间状态
struct Header {
    bool    fin      = false;   ///< FIN 标志
    OpCode  opcode   = OpCode::Text; ///< 操作码
    bool    masked   = false;   ///< 是否掩码
    std::size_t header_len = 0; ///< 已解析出的帧头长度
    uint64_t payload_len = 0;   ///< 载荷长度
    uint32_t mask_key = 0;      ///< 掩码密钥（当 masked=true 时有效）
};

/// @brief 计算给定帧头所需的完整帧字节数（帧头 + 载荷）
/// @return 帧总长度；若 header 不完整则返回 0
inline uint64_t FrameTotalSize(const Header& hdr) {
    return static_cast<uint64_t>(hdr.header_len) + hdr.payload_len;
}

/// @brief 尝试从缓冲区解析帧头
/// @param data  缓冲区首地址
/// @param size  缓冲区有效字节数
/// @param[out] hdr 解析出的帧头
/// @return >0：帧头已完整解析，返回帧头部分占用的字节数（不含载荷）；
///         ==0：数据不足，需要继续读取；
///         解析失败时 hdr.payload_len 会被设置为特殊标记（调用方应关闭连接）。
inline std::size_t ParseHeader(const uint8_t* data, std::size_t size, Header& hdr) {
    if (size < 2) return 0; // 至少需要 2 字节

    uint8_t b0 = data[0];
    uint8_t b1 = data[1];

    hdr.fin    = (b0 & 0x80) != 0;
    uint8_t rsv = b0 & 0x70;
    uint8_t op = b0 & 0x0F;

    // RSV 必须为 0，操作码必须合法
    if (rsv != 0 || !IsValidOpCode(op)) {
        hdr.payload_len = UINT64_MAX; // 标记为错误
        return 0;
    }
    hdr.opcode = static_cast<OpCode>(op);

    hdr.masked = (b1 & 0x80) != 0;
    uint8_t len7 = b1 & 0x7F;

    // 确定扩展长度字段的字节数
    std::size_t ext_len_bytes = 0;
    if (len7 == 126) {
        ext_len_bytes = 2;
    } else if (len7 == 127) {
        ext_len_bytes = 8;
    }

    std::size_t header_len = 2 + ext_len_bytes + (hdr.masked ? 4 : 0);
    if (size < header_len) return 0; // 数据不足
    hdr.header_len = header_len;

    // 解析载荷长度
    if (len7 <= 125) {
        hdr.payload_len = len7;
    } else if (len7 == 126) {
        hdr.payload_len = (static_cast<uint64_t>(data[2]) << 8) | data[3];
        // 2 字节长度不得小于 126（否则应使用 7 位编码）
        if (hdr.payload_len < 126) {
            hdr.payload_len = UINT64_MAX;
            return 0;
        }
    } else { // 127
        // 高 4 字节必须为 0（WebSocket 载荷长度实际上不会达到 2^64）
        for (int i = 0; i < 4; ++i) {
            if (data[2 + i] != 0) {
                hdr.payload_len = UINT64_MAX;
                return 0;
            }
        }
        hdr.payload_len = 0;
        for (int i = 4; i < 8; ++i) {
            hdr.payload_len = (hdr.payload_len << 8) | data[2 + i];
        }
        if (hdr.payload_len < 65536) {
            hdr.payload_len = UINT64_MAX;
            return 0;
        }
    }

    // 解析掩码密钥
    if (hdr.masked) {
        std::size_t off = 2 + ext_len_bytes;
        hdr.mask_key = (static_cast<uint32_t>(data[off]) << 24) |
                       (static_cast<uint32_t>(data[off + 1]) << 16) |
                       (static_cast<uint32_t>(data[off + 2]) << 8)  |
                       (static_cast<uint32_t>(data[off + 3]));
    }

    return header_len;
}

/// @brief 对载荷执行掩码/解掩码（异或操作，可逆）
/// @param payload 载荷数据（原地修改）
/// @param len     载荷长度
/// @param mask_key 4 字节掩码密钥
inline void ApplyMask(uint8_t* payload, uint64_t len, uint32_t mask_key) {
    const uint8_t mask[4] = {
        static_cast<uint8_t>((mask_key >> 24) & 0xFF),
        static_cast<uint8_t>((mask_key >> 16) & 0xFF),
        static_cast<uint8_t>((mask_key >> 8) & 0xFF),
        static_cast<uint8_t>(mask_key & 0xFF),
    };
    for (uint64_t i = 0; i < len; ++i) {
        payload[i] ^= mask[i % 4];
    }
}

/// @brief 编码一个服务器→客户端的帧（不带掩码）
/// @param fin     是否为最终分片
/// @param opcode  操作码
/// @param payload 载荷数据
/// @param len     载荷长度
/// @return 编码后的完整帧字节序列
inline std::vector<uint8_t> EncodeServerFrame(
    bool fin, OpCode opcode, const uint8_t* payload, uint64_t len)
{
    std::vector<uint8_t> frame;
    // 预估大小：2 + 8（最大扩展长度）+ len
    frame.reserve(static_cast<size_t>(2 + 8 + len));

    // 第一字节：FIN + RSV(0) + opcode
    uint8_t b0 = static_cast<uint8_t>(opcode);
    if (fin) b0 |= 0x80;
    frame.push_back(b0);

    // 第二字节：MASK=0（服务器发出的帧不得掩码）+ Payload len
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }

    // 载荷
    if (len > 0 && payload != nullptr) {
        frame.insert(frame.end(), payload, payload + len);
    }
    return frame;
}

/// @brief 生成一个随机 32 位掩码密钥（用于客户端模式，本库主要用于服务端故少用）
inline uint32_t GenerateMaskKey() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return static_cast<uint32_t>(gen());
}

} // namespace frame
} // namespace ws

#endif // SERVER_WS_WS_FRAME_HPP
