/**
 * @file dns_message.cpp
 * @brief DNS/mDNS 报文编解码实现
 */

#include "mdns/dns_message.h"

#include <cstring>
#include <optional>
#include <stdexcept>

namespace mdns {

// ============================================================
// DNS 报文编解码实现
// ============================================================

std::vector<uint8_t> DnsMessageCodec::Encode(const DnsMessage& msg) {
    std::vector<uint8_t> result;
    result.reserve(512);

    // 编码头部
    DnsHeader header = msg.header;
    header.qdcount = Htons(static_cast<uint16_t>(msg.questions.size()));
    header.ancount = Htons(static_cast<uint16_t>(msg.answers.size()));
    header.nscount = Htons(static_cast<uint16_t>(msg.authority.size()));
    header.arcount = Htons(static_cast<uint16_t>(msg.additional.size()));

    uint8_t* header_bytes = reinterpret_cast<uint8_t*>(&header);
    result.insert(result.end(), header_bytes, header_bytes + sizeof(DnsHeader));

    // 编码问题记录
    for (const auto& q : msg.questions) {
        auto q_bytes = EncodeQuestion(q);
        result.insert(result.end(), q_bytes.begin(), q_bytes.end());
    }

    // 编码回答记录
    for (const auto& rr : msg.answers) {
        auto rr_bytes = EncodeResourceRecord(*rr);
        result.insert(result.end(), rr_bytes.begin(), rr_bytes.end());
    }

    // 编码权威记录
    for (const auto& rr : msg.authority) {
        auto rr_bytes = EncodeResourceRecord(*rr);
        result.insert(result.end(), rr_bytes.begin(), rr_bytes.end());
    }

    // 编码附加记录
    for (const auto& rr : msg.additional) {
        auto rr_bytes = EncodeResourceRecord(*rr);
        result.insert(result.end(), rr_bytes.begin(), rr_bytes.end());
    }

    return result;
}

std::optional<DnsMessage> DnsMessageCodec::Decode(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(DnsHeader)) {
        return std::nullopt;
    }

    DnsMessage msg;
    std::memcpy(&msg.header, data.data(), sizeof(DnsHeader));

    size_t offset = sizeof(DnsHeader);

    // 解码问题记录
    for (int i = 0; i < Ntohs(msg.header.qdcount); ++i) {
        try {
            auto q = DecodeQuestion(data.data(), offset, data.size());
            msg.questions.push_back(q);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // 解码回答记录
    for (int i = 0; i < Ntohs(msg.header.ancount); ++i) {
        try {
            auto rr = DecodeResourceRecord(data.data(), offset, data.size());
            msg.answers.push_back(rr);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // 解码权威记录
    for (int i = 0; i < Ntohs(msg.header.nscount); ++i) {
        try {
            auto rr = DecodeResourceRecord(data.data(), offset, data.size());
            msg.authority.push_back(rr);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // 解码附加记录
    for (int i = 0; i < Ntohs(msg.header.arcount); ++i) {
        try {
            auto rr = DecodeResourceRecord(data.data(), offset, data.size());
            msg.additional.push_back(rr);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    return msg;
}

std::vector<uint8_t> DnsMessageCodec::EncodeName(const std::string& name) {
    std::vector<uint8_t> result;
    
    if (name.empty() || name == ".") {
        result.push_back(0);
        return result;
    }

    std::string_view sv(name);
    if (sv.ends_with(".")) {
        sv = sv.substr(0, sv.size() - 1);
    }

    size_t start = 0;
    while (start < sv.size()) {
        size_t dot = sv.find('.', start);
        if (dot == std::string_view::npos) {
            dot = sv.size();
        }

        size_t label_len = dot - start;
        if (label_len > 63) {
            throw std::runtime_error("DNS label too long");
        }

        result.push_back(static_cast<uint8_t>(label_len));
        result.insert(result.end(), sv.begin() + start, sv.begin() + dot);

        start = dot + 1;
    }

    result.push_back(0);
    return result;
}

std::string DnsMessageCodec::DecodeName(const uint8_t* data, size_t& offset, size_t total_len) {
    std::string result;
    
    while (offset < total_len) {
        uint8_t first_byte = data[offset];

        if (first_byte == 0) {
            offset++;
            break;
        }

        if ((first_byte & 0xC0) == 0xC0) {
            if (offset + 1 >= total_len) {
                throw std::runtime_error("Invalid DNS name compression");
            }
            uint16_t pointer = ((first_byte & 0x3F) << 8) | data[offset + 1];
            offset += 2;
            
            size_t ptr_offset = pointer;
            std::string pointed_name = DecodeName(data, ptr_offset, total_len);
            if (!result.empty() && !pointed_name.empty()) {
                result += ".";
            }
            result += pointed_name;
            return result;
        }

        offset++;
        if (offset + first_byte > total_len) {
            throw std::runtime_error("Invalid DNS name length");
        }

        if (!result.empty()) {
            result += ".";
        }
        result += std::string(reinterpret_cast<const char*>(&data[offset]), first_byte);
        offset += first_byte;
    }

    return result;
}

std::vector<uint8_t> DnsMessageCodec::EncodeQuestion(const DnsQuestion& q) {
    std::vector<uint8_t> result;

    auto name_bytes = EncodeName(q.name);
    result.insert(result.end(), name_bytes.begin(), name_bytes.end());

    uint16_t type = Htons(q.type);
    uint16_t class_ = Htons(q.class_);
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&type), reinterpret_cast<uint8_t*>(&type) + 2);
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&class_), reinterpret_cast<uint8_t*>(&class_) + 2);

    return result;
}

DnsQuestion DnsMessageCodec::DecodeQuestion(const uint8_t* data, size_t& offset, size_t total_len) {
    DnsQuestion q;

    q.name = DecodeName(data, offset, total_len);

    if (offset + 4 > total_len) {
        throw std::runtime_error("Invalid DNS question");
    }

    q.type = Ntohs(*reinterpret_cast<const uint16_t*>(&data[offset]));
    offset += 2;
    q.class_ = Ntohs(*reinterpret_cast<const uint16_t*>(&data[offset]));
    offset += 2;

    return q;
}

std::vector<uint8_t> DnsMessageCodec::EncodeResourceRecord(const DnsResourceRecord& rr) {
    std::vector<uint8_t> result;

    auto name_bytes = EncodeName(rr.name);
    result.insert(result.end(), name_bytes.begin(), name_bytes.end());

    uint16_t type = Htons(rr.type);
    uint16_t class_ = Htons(rr.class_);
    uint32_t ttl = Htonl(rr.ttl);
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&type), reinterpret_cast<uint8_t*>(&type) + 2);
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&class_), reinterpret_cast<uint8_t*>(&class_) + 2);
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&ttl), reinterpret_cast<uint8_t*>(&ttl) + 4);

    uint16_t rdlength = Htons(static_cast<uint16_t>(rr.data.size()));
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&rdlength), reinterpret_cast<uint8_t*>(&rdlength) + 2);
    result.insert(result.end(), rr.data.begin(), rr.data.end());

    return result;
}

std::shared_ptr<DnsResourceRecord> DnsMessageCodec::DecodeResourceRecord(
    const uint8_t* data, size_t& offset, size_t total_len) {
    
    if (offset >= total_len) {
        throw std::runtime_error("Unexpected end of data");
    }

    std::string name = DecodeName(data, offset, total_len);

    if (offset + 10 > total_len) {
        throw std::runtime_error("Invalid DNS resource record");
    }

    uint16_t type = Ntohs(*reinterpret_cast<const uint16_t*>(&data[offset]));
    offset += 2;
    uint16_t class_ = Ntohs(*reinterpret_cast<const uint16_t*>(&data[offset]));
    offset += 2;
    uint32_t ttl = Ntohl(*reinterpret_cast<const uint32_t*>(&data[offset]));
    offset += 4;
    uint16_t rdlength = Ntohs(*reinterpret_cast<const uint16_t*>(&data[offset]));
    offset += 2;

    if (offset + rdlength > total_len) {
        throw std::runtime_error("Invalid DNS resource record data length");
    }

    std::shared_ptr<DnsResourceRecord> rr;

    switch (static_cast<DnsRecordType>(type)) {
        case DnsRecordType::A: {
            auto a_rr = std::make_shared<DnsARecord>();
            if (rdlength == 4) {
                char ipv4[16];
                std::snprintf(ipv4, sizeof(ipv4), "%d.%d.%d.%d",
                    data[offset], data[offset + 1], data[offset + 2], data[offset + 3]);
                a_rr->ipv4 = ipv4;
            }
            rr = a_rr;
            break;
        }
        case DnsRecordType::PTR: {
            auto ptr_rr = std::make_shared<DnsPtrRecord>();
            size_t ptr_offset = offset;
            ptr_rr->ptr_name = DecodeName(data, ptr_offset, total_len);
            rr = ptr_rr;
            break;
        }
        case DnsRecordType::SRV: {
            auto srv_rr = std::make_shared<DnsSrvRecord>();
            if (rdlength >= 6) {
                srv_rr->priority = Ntohs(*reinterpret_cast<const uint16_t*>(&data[offset]));
                srv_rr->weight = Ntohs(*reinterpret_cast<const uint16_t*>(&data[offset + 2]));
                srv_rr->port = Ntohs(*reinterpret_cast<const uint16_t*>(&data[offset + 4]));
                size_t target_offset = offset + 6;
                srv_rr->target = DecodeName(data, target_offset, total_len);
            }
            rr = srv_rr;
            break;
        }
        case DnsRecordType::TXT: {
            auto txt_rr = std::make_shared<DnsTxtRecord>();
            size_t txt_offset = offset;
            while (txt_offset < offset + rdlength) {
                uint8_t len = data[txt_offset++];
                if (txt_offset + len > offset + rdlength) {
                    break;
                }
                std::string txt_str(reinterpret_cast<const char*>(&data[txt_offset]), len);
                txt_offset += len;
                
                size_t eq_pos = txt_str.find('=');
                if (eq_pos != std::string::npos) {
                    txt_rr->txt_data[txt_str.substr(0, eq_pos)] = txt_str.substr(eq_pos + 1);
                }
            }
            rr = txt_rr;
            break;
        }
        default: {
            auto generic_rr = std::make_shared<DnsResourceRecord>();
            generic_rr->name = name;
            generic_rr->type = type;
            generic_rr->class_ = class_;
            generic_rr->ttl = ttl;
            generic_rr->data.assign(data + offset, data + offset + rdlength);
            rr = generic_rr;
            break;
        }
    }

    offset += rdlength;
    return rr;
}

} // namespace mdns
