/**
 * @file dns_message.h
 * @brief DNS/mDNS 报文编解码
 * 
 * 实现 DNS 消息格式的编解码，支持：
 *   - DNS 头部
 *   - 问题记录（Question）
 *   - 资源记录（Resource Record）
 *   - 常见记录类型：A、AAAA、PTR、SRV、TXT
 */

#ifndef AI_CAMERA_MDNS_DNS_MESSAGE_H
#define AI_CAMERA_MDNS_DNS_MESSAGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace mdns {

// ============================================================
// DNS 记录类型
// ============================================================

/// @brief DNS 记录类型
enum class DnsRecordType : uint16_t {
    A     = 1,    ///< IPv4 地址
    AAAA  = 28,   ///< IPv6 地址
    PTR   = 12,   ///< 指针记录
    SRV   = 33,   ///< 服务记录
    TXT   = 16,   ///< 文本记录
    ANY   = 255    ///< 任意类型
};

/// @brief DNS 记录类
enum class DnsRecordClass : uint16_t {
    INet  = 1,    ///< Internet
    ANY   = 255    ///< 任意类
};

// ============================================================
// DNS 消息头部
// ============================================================

#pragma pack(push, 1)
/// @brief DNS 消息头部（12 字节）
struct DnsHeader {
    uint16_t id;       ///< 事务 ID
    uint16_t flags;    ///< 标志位
    uint16_t qdcount;  ///< 问题数
    uint16_t ancount;  ///< 回答数
    uint16_t nscount;  ///< 权威记录数
    uint16_t arcount;  ///< 附加记录数

    /// @brief 是否为查询
    bool IsQuery() const { return (ntohs(flags) & 0x8000) == 0; }

    /// @brief 是否为响应
    bool IsResponse() const { return (ntohs(flags) & 0x8000) != 0; }

    /// @brief 设置查询/响应标志
    void SetQR(bool is_response) {
        auto f = ntohs(flags);
        if (is_response) f |= 0x8000;
        else f &= ~0x8000;
        flags = htons(f);
    }
};
#pragma pack(pop)

// ============================================================
// DNS 资源记录
// ============================================================

/// @brief DNS 资源记录基类
struct DnsResourceRecord {
    std::string name;       ///< 域名（可能是压缩格式）
    uint16_t   type;       ///< 记录类型
    uint16_t   class_;     ///< 记录类
    uint32_t   ttl;        ///< 生存时间
    std::vector<uint8_t> data;  ///< 记录数据

    virtual ~DnsResourceRecord() = default;
};

/// @brief A 记录（IPv4 地址）
struct DnsARecord : public DnsResourceRecord {
    std::string ipv4;  ///< IPv4 地址字符串
};

/// @brief PTR 记录（指针）
struct DnsPtrRecord : public DnsResourceRecord {
    std::string ptr_name;  ///< 指向的域名
};

/// @brief SRV 记录（服务）
struct DnsSrvRecord : public DnsResourceRecord {
    uint16_t priority;    ///< 优先级
    uint16_t weight;      ///< 权重
    uint16_t port;        ///< 端口
    std::string target;   ///< 目标主机名
};

/// @brief TXT 记录（文本）
struct DnsTxtRecord : public DnsResourceRecord {
    std::map<std::string, std::string> txt_data;  ///< 键值对
};

// ============================================================
// DNS 消息
// ============================================================

/// @brief DNS 问题记录
struct DnsQuestion {
    std::string name;    ///< 查询域名
    uint16_t    type;    ///< 查询类型
    uint16_t    class_;  ///< 查询类
};

/// @brief DNS 消息
struct DnsMessage {
    DnsHeader              header;
    std::vector<DnsQuestion>         questions;
    std::vector<std::shared_ptr<DnsResourceRecord>> answers;
    std::vector<std::shared_ptr<DnsResourceRecord>> authority;
    std::vector<std::shared_ptr<DnsResourceRecord>> additional;
};

// ============================================================
// 工具函数（字节序转换）
// ============================================================

/// @brief 网络字节序转主机字节序（16位）
inline uint16_t Ntohs(uint16_t net) {
    return (net >> 8) | ((net & 0xFF) << 8);
}

/// @brief 主机字节序转网络字节序（16位）
inline uint16_t Htons(uint16_t host) {
    return (host >> 8) | ((host & 0xFF) << 8);
}

/// @brief 网络字节序转主机字节序（32位）
inline uint32_t Ntohl(uint32_t net) {
    return ((uint32_t)Ntohs(net & 0xFFFF) << 16) | Ntohs(net >> 16);
}

/// @brief 主机字节序转网络字节序（32位）
inline uint32_t Htonl(uint32_t host) {
    return ((uint32_t)Htons(host & 0xFFFF) << 16) | Htons(host >> 16);
}

// ============================================================
// DNS 报文编解码器
// ============================================================

/// @brief DNS 报文编解码器
class DnsMessageCodec {
public:
    /// @brief 编码 DNS 消息为二进制报文
    /// @param msg DNS 消息
    /// @return 二进制报文
    static std::vector<uint8_t> Encode(const DnsMessage& msg);

    /// @brief 解码二进制报文为 DNS 消息
    /// @param data 二进制数据
    /// @return DNS 消息（失败返回 std::nullopt）
    static std::optional<DnsMessage> Decode(const std::vector<uint8_t>& data);

private:
    /// @brief 编码域名（支持标签压缩）
    static std::vector<uint8_t> EncodeName(const std::string& name);

    /// @brief 解码域名（支持标签压缩）
    static std::string DecodeName(const uint8_t* data, size_t& offset, size_t total_len);

    /// @brief 编码问题记录
    static std::vector<uint8_t> EncodeQuestion(const DnsQuestion& q);

    /// @brief 解码问题记录
    static DnsQuestion DecodeQuestion(const uint8_t* data, size_t& offset, size_t total_len);

    /// @brief 编码资源记录
    static std::vector<uint8_t> EncodeResourceRecord(const DnsResourceRecord& rr);

    /// @brief 解码资源记录
    static std::shared_ptr<DnsResourceRecord> DecodeResourceRecord(
        const uint8_t* data, size_t& offset, size_t total_len);
};

} // namespace mdns

#endif // AI_CAMERA_MDNS_DNS_MESSAGE_H
