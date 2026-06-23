#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace ws {
namespace crypto {

/// @brief SHA-1 摘要算法（RFC 3174）
///
/// WebSocket 握手阶段需要计算 Sec-WebSocket-Accept，其算法为：
///   SHA-1(Sec-WebSocket-Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")
/// SHA-1 输出固定 20 字节（160 位）摘要。
///
/// 本实现为纯 C++ 无依赖版本，仅用于 WebSocket 握手，
/// 不建议用于其它安全敏感场景。
class Sha1 {
public:
    /// 摘要输出长度（字节）
    static constexpr std::size_t kDigestSize = 20;

    Sha1();
    ~Sha1();

    /// 不可拷贝（内部状态不便于复制）
    Sha1(const Sha1&) = delete;
    Sha1& operator=(const Sha1&) = delete;

    /// @brief 输入一段数据（可多次调用）
    /// @param data 数据首地址，可为 nullptr（当 size==0 时）
    /// @param size 数据字节数
    void Update(const void* data, std::size_t size);

    /// @brief 便捷重载：输入字符串
    void Update(const std::string& s) { Update(s.data(), s.size()); }

    /// @brief 结束并输出摘要
    /// @return 20 字节摘要
    std::array<uint8_t, kDigestSize> Final();

    /// @brief 一次性计算 SHA-1（便捷接口）
    /// @param data 输入数据
    /// @param size 长度
    /// @return 20 字节摘要
    static std::array<uint8_t, kDigestSize> Hash(const void* data, std::size_t size);

    /// @brief 一次性计算 SHA-1（字符串版本）
    static std::array<uint8_t, kDigestSize> Hash(const std::string& s);

private:
    void ProcessBlock(const uint8_t* block);

    uint32_t h_[5];          ///< 哈希中间状态
    uint64_t total_bits_;    ///< 已处理的总比特数
    uint8_t  buffer_[64];    ///< 输入缓冲区（不足 64 字节暂存）
    std::size_t buffer_len_; ///< 缓冲区已用长度
};

/// @brief Base64 编码（RFC 4648）
///
/// WebSocket 握手中需将 20 字节的 SHA-1 摘要进行 Base64 编码，
/// 得到 28 字符的 Sec-WebSocket-Accept 值。
///
/// @param data 输入数据首地址
/// @param size 输入字节数
/// @return Base64 编码字符串（含 '=' 填充）
std::string Base64Encode(const void* data, std::size_t size);

/// @brief Base64 编码（便捷重载，接受字节容器）
std::string Base64Encode(const std::vector<uint8_t>& data);

/// @brief Base64 编码（便捷重载，接受定长数组）
template <std::size_t N>
std::string Base64Encode(const std::array<uint8_t, N>& data) {
    return Base64Encode(data.data(), data.size());
}

} // namespace crypto
} // namespace ws
