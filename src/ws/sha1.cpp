#include "ws/sha1.h"

#include <cstring>

namespace ws {
namespace crypto {

// ============================================================
// Sha1 实现
// ============================================================

// SHA-1 初始哈希值（RFC 3174 第 6.1 节）
Sha1::Sha1()
    : h_{0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u},
      total_bits_(0),
      buffer_len_(0) {
}

Sha1::~Sha1() = default;

void Sha1::Update(const void* data, std::size_t size) {
    if (size == 0) return;
    total_bits_ += static_cast<uint64_t>(size) * 8u;

    const uint8_t* p = static_cast<const uint8_t*>(data);

    // 如果缓冲区有残留数据，先尝试填满 64 字节再处理
    if (buffer_len_ > 0) {
        std::size_t need = 64 - buffer_len_;
        std::size_t take = (size < need) ? size : need;
        std::memcpy(buffer_ + buffer_len_, p, take);
        buffer_len_ += take;
        p += take;
        size -= take;

        if (buffer_len_ == 64) {
            ProcessBlock(buffer_);
            buffer_len_ = 0;
        }
    }

    // 处理完整的 64 字节块
    while (size >= 64) {
        ProcessBlock(p);
        p += 64;
        size -= 64;
    }

    // 剩余不足 64 字节存入缓冲区
    if (size > 0) {
        std::memcpy(buffer_, p, size);
        buffer_len_ = size;
    }
}

std::array<uint8_t, Sha1::kDigestSize> Sha1::Final() {
    // 保存原始消息的总比特长度（填充前）
    uint64_t original_bit_len = total_bits_;

    // 填充：先追加 0x80
    uint8_t pad = 0x80;
    Update(&pad, 1);

    // 追加 0x00，直到 buffer_len_ % 64 == 56（为最后的 8 字节长度留空间）
    uint8_t zero = 0x00;
    while (buffer_len_ % 64 != 56) {
        Update(&zero, 1);
    }

    // 追加原始消息的总比特长度（大端 64 位）
    uint8_t len_bytes[8];
    uint64_t bit_len = original_bit_len;
    for (int i = 7; i >= 0; --i) {
        len_bytes[i] = static_cast<uint8_t>(bit_len & 0xFFu);
        bit_len >>= 8;
    }
    // 此时 buffer_len_ 必为 56 的倍数（单块场景为 56），直接追加 8 字节凑满 64
    std::memcpy(buffer_ + buffer_len_, len_bytes, 8);
    buffer_len_ += 8;
    ProcessBlock(buffer_);
    buffer_len_ = 0;

    // 输出大端摘要
    std::array<uint8_t, kDigestSize> digest{};
    for (int i = 0; i < 5; ++i) {
        digest[i * 4 + 0] = static_cast<uint8_t>((h_[i] >> 24) & 0xFFu);
        digest[i * 4 + 1] = static_cast<uint8_t>((h_[i] >> 16) & 0xFFu);
        digest[i * 4 + 2] = static_cast<uint8_t>((h_[i] >> 8) & 0xFFu);
        digest[i * 4 + 3] = static_cast<uint8_t>(h_[i] & 0xFFu);
    }
    return digest;
}

std::array<uint8_t, Sha1::kDigestSize> Sha1::Hash(const void* data, std::size_t size) {
    Sha1 sha;
    sha.Update(data, size);
    return sha.Final();
}

std::array<uint8_t, Sha1::kDigestSize> Sha1::Hash(const std::string& s) {
    return Hash(s.data(), s.size());
}

/// SHA-1 循环左移
static inline uint32_t LeftRotate(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32u - bits));
}

void Sha1::ProcessBlock(const uint8_t* block) {
    // 将 64 字节拆分为 16 个大端 32 位字
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8)  |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }
    // 扩展为 80 个字
    for (int i = 16; i < 80; ++i) {
        w[i] = LeftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t temp = LeftRotate(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = LeftRotate(b, 30);
        b = a;
        a = temp;
    }

    h_[0] += a;
    h_[1] += b;
    h_[2] += c;
    h_[3] += d;
    h_[4] += e;
}

// ============================================================
// Base64 实现
// ============================================================

std::string Base64Encode(const void* data, std::size_t size) {
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const uint8_t* p = static_cast<const uint8_t*>(data);
    std::string out;
    out.reserve(((size + 2) / 3) * 4);

    std::size_t i = 0;
    for (; i + 2 < size; i += 3) {
        uint32_t n = (static_cast<uint32_t>(p[i]) << 16) |
                     (static_cast<uint32_t>(p[i + 1]) << 8) |
                     (static_cast<uint32_t>(p[i + 2]));
        out.push_back(kTable[(n >> 18) & 0x3F]);
        out.push_back(kTable[(n >> 12) & 0x3F]);
        out.push_back(kTable[(n >> 6) & 0x3F]);
        out.push_back(kTable[n & 0x3F]);
    }

    // 处理剩余 1 或 2 字节
    std::size_t rem = size - i;
    if (rem == 1) {
        uint32_t n = static_cast<uint32_t>(p[i]) << 16;
        out.push_back(kTable[(n >> 18) & 0x3F]);
        out.push_back(kTable[(n >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        uint32_t n = (static_cast<uint32_t>(p[i]) << 16) |
                     (static_cast<uint32_t>(p[i + 1]) << 8);
        out.push_back(kTable[(n >> 18) & 0x3F]);
        out.push_back(kTable[(n >> 12) & 0x3F]);
        out.push_back(kTable[(n >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

std::string Base64Encode(const std::vector<uint8_t>& data) {
    return Base64Encode(data.data(), data.size());
}

} // namespace crypto
} // namespace ws
