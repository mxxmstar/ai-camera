#include "gb28181/sip_message.h"
#include <sstream>
#include <iostream>
#include <random>
#include <ctime>
#include <iomanip>

// MD5 计算需要包含 Windows 或 OpenSSL 的头文件
// 这里使用 Windows CryptoAPI 或简单的 MD5 实现
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <openssl/md5.h>
#endif

namespace gb28181 {

// ============================================================================
// 辅助函数：计算 MD5 哈希
// ============================================================================
static std::string MD5(const std::string& input) {
    std::string result;
    
#ifdef _WIN32
    // 使用 Windows CryptoAPI 计算 MD5
    HCRYPTPROV hProv = NULL;
    HCRYPTHASH hHash = NULL;
    DWORD dwDataLen = 0;
    
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            const BYTE* pbData = (const BYTE*)input.c_str();
            DWORD dwData = (DWORD)input.length();
            
            if (CryptHashData(hHash, pbData, dwData, 0)) {
                dwDataLen = 16;  // MD5 输出 16 字节
                BYTE bHash[16];
                if (CryptGetHashParam(hHash, HP_HASHVAL, bHash, &dwDataLen, 0)) {
                    // 转换为十六进制字符串
                    std::ostringstream oss;
                    for (DWORD i = 0; i < dwDataLen; i++) {
                        oss << std::hex << std::setw(2) << std::setfill('0') << (int)bHash[i];
                    }
                    result = oss.str();
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
#else
    // 使用 OpenSSL 计算 MD5
    unsigned char md5_result[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)input.c_str(), input.length(), md5_result);
    
    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)md5_result[i];
    }
    result = oss.str();
#endif
    
    return result;
}

// ============================================================================
// SipMessage 实现
// ============================================================================

void SipMessage::SetHeader(const std::string& name, const std::string& value) {
    // SIP 头字段名不区分大小写，统一转换为小写存储
    std::string lower_name;
    for (char c : name) {
        lower_name += std::tolower(c);
    }
    headers_[lower_name] = value;
}

std::string SipMessage::GetHeader(const std::string& name) const {
    std::string lower_name;
    for (char c : name) {
        lower_name += std::tolower(c);
    }
    auto it = headers_.find(lower_name);
    if (it != headers_.end()) {
        return it->second;
    }
    return "";
}

bool SipMessage::HasHeader(const std::string& name) const {
    std::string lower_name;
    for (char c : name) {
        lower_name += std::tolower(c);
    }
    return headers_.find(lower_name) != headers_.end();
}

void SipMessage::RemoveHeader(const std::string& name) {
    std::string lower_name;
    for (char c : name) {
        lower_name += std::tolower(c);
    }
    headers_.erase(lower_name);
}

std::string SipMessage::Serialize() const {
    std::ostringstream oss;
    
    if (type_ == Type::REQUEST) {
        // 请求行：METHOD SP Request-URI SP SIP/2.0 CRLF
        oss << MethodToString(method_) << " " << request_uri_ << " SIP/2.0\r\n";
    } else {
        // 状态行：SIP/2.0 SP Status-Code SP Reason-Phrase CRLF
        oss << "SIP/2.0 " << status_code_ << " " << reason_phrase_ << "\r\n";
    }
    
    // 添加必要的头字段（如果未设置）
    // Content-Length 自动计算
    if (!HasHeader("Content-Length")) {
        oss << "Content-Length: " << body_.length() << "\r\n";
    }
    
    // 序列化所有头字段
    for (const auto& header : headers_) {
        // 头字段名使用原始大小写（这里简单处理，统一首字母大写）
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    // 空行分隔头字段和消息体
    oss << "\r\n";
    
    // 消息体
    if (!body_.empty()) {
        oss << body_;
    }
    
    return oss.str();
}

// 从字符串解析 SIP 消息
SipMessage* SipMessage::Parse(const std::string& raw_message) {
    std::istringstream iss(raw_message);
    std::string line;
    
    // 读取第一行（请求行或状态行）
    if (!std::getline(iss, line) || line.empty()) {
        std::cerr << "[SIP] Parse failed: empty message" << std::endl;
        return nullptr;
    }
    
    // 移除末尾的 \r（如果有）
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    SipMessage* msg = nullptr;
    
    // 判断是请求还是响应
    if (line.find("SIP/2.0") == 0) {
        // 响应：SIP/2.0 状态码 原因短语
        msg = new SipResponse();
        msg->type_ = Type::RESPONSE;
        
        // 解析状态码和原因短语
        std::istringstream line_ss(line.substr(8));  // 跳过 "SIP/2.0 "
        int status_code;
        line_ss >> status_code;
        msg->status_code_ = static_cast<uint16_t>(status_code);
        
        // 读取原因短语（剩余部分）
        std::string reason;
        std::getline(line_ss, reason);
        // 跳过前导空格
        size_t start = reason.find_first_not_of(' ');
        if (start != std::string::npos) {
            reason = reason.substr(start);
        }
        msg->reason_phrase_ = reason;
    } else {
        // 请求：METHOD Request-URI SIP/2.0
        msg = new SipRequest();
        msg->type_ = Type::REQUEST;
        
        std::istringstream line_ss(line);
        std::string method_str;
        line_ss >> method_str;
        msg->method_ = StringToMethod(method_str);
        
        std::string uri;
        line_ss >> uri;
        msg->request_uri_ = uri;
        
        // 验证 SIP 版本
        std::string sip_version;
        line_ss >> sip_version;
        if (sip_version != "SIP/2.0") {
            std::cerr << "[SIP] Unsupported SIP version: " << sip_version << std::endl;
            delete msg;
            return nullptr;
        }
    }
    
    // 解析头字段
    while (std::getline(iss, line)) {
        // 移除末尾的 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // 空行表示头字段结束
        if (line.empty()) {
            break;
        }
        
        // 解析头字段：Name: Value
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // 移除值的前导空格
            size_t start = value.find_first_not_of(' ');
            if (start != std::string::npos) {
                value = value.substr(start);
            }
            
            msg->SetHeader(name, value);
        }
    }
    
    // 读取消息体（如果有 Content-Length）
    std::string content_length_str = msg->GetHeader("Content-Length");
    if (!content_length_str.empty()) {
        size_t content_length = std::stoul(content_length_str);
        if (content_length > 0) {
            // 读取剩余内容作为消息体
            std::string rest;
            while (std::getline(iss, line)) {
                rest += line + "\n";
            }
            // 也可能消息体已经在第一次空行后就开始了
            // 这里简化处理，直接读取剩余所有内容
            auto remaining = raw_message.find("\r\n\r\n");
            if (remaining != std::string::npos) {
                msg->body_ = raw_message.substr(remaining + 4);
            }
        }
    }
    
    return msg;
}

std::string SipMessage::MethodToString(Method method) {
    switch (method) {
        case Method::REGISTER:   return "REGISTER";
        case Method::INVITE:     return "INVITE";
        case Method::ACK:        return "ACK";
        case Method::BYE:        return "BYE";
        case Method::CANCEL:     return "CANCEL";
        case Method::MESSAGE:    return "MESSAGE";
        case Method::OPTIONS:    return "OPTIONS";
        case Method::SUBSCRIBE:  return "SUBSCRIBE";
        case Method::NOTIFY:     return "NOTIFY";
        default:                 return "UNKNOWN";
    }
}

SipMessage::Method SipMessage::StringToMethod(const std::string& method_str) {
    if (method_str == "REGISTER")   return Method::REGISTER;
    if (method_str == "INVITE")     return Method::INVITE;
    if (method_str == "ACK")        return Method::ACK;
    if (method_str == "BYE")        return Method::BYE;
    if (method_str == "CANCEL")     return Method::CANCEL;
    if (method_str == "MESSAGE")    return Method::MESSAGE;
    if (method_str == "OPTIONS")    return Method::OPTIONS;
    if (method_str == "SUBSCRIBE")  return Method::SUBSCRIBE;
    if (method_str == "NOTIFY")     return Method::NOTIFY;
    return Method::UNKNOWN;
}

std::string SipMessage::GenerateCallId(const std::string& local_ip) {
    // Call-ID 格式：随机字符串@本地IP
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    std::ostringstream oss;
    oss << dis(gen) << "@" << local_ip;
    return oss.str();
}

std::string SipMessage::GenerateBranch() {
    // Branch 格式：z9hG4bK + 随机字符串
    // z9hG4bK 是 RFC 3261 规定的魔法 Cookie，表示支持 RFC 3261
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    std::ostringstream oss;
    oss << "z9hG4bK" << dis(gen);
    return oss.str();
}

std::string SipMessage::GenerateTag() {
    // Tag 格式：随机字符串
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000000, 99999999);
    return std::to_string(dis(gen));
}

// ============================================================================
// SipRequest 实现
// ============================================================================

SipRequest::SipRequest() {
    type_ = Type::REQUEST;
}

SipRequest::SipRequest(Method method, const std::string& request_uri) {
    type_ = Type::REQUEST;
    method_ = method;
    request_uri_ = request_uri;
}

std::string SipRequest::Serialize() const {
    return SipMessage::Serialize();
}

// ============================================================================
// SipResponse 实现
// ============================================================================

SipResponse::SipResponse() {
    type_ = Type::RESPONSE;
}

SipResponse::SipResponse(uint16_t status_code, const std::string& reason_phrase) {
    type_ = Type::RESPONSE;
    status_code_ = status_code;
    reason_phrase_ = reason_phrase;
}

std::string SipResponse::Serialize() const {
    return SipMessage::Serialize();
}

// ============================================================================
// SIP 认证相关函数实现
// ============================================================================

SipAuthInfo ParseWwwAuthenticate(const std::string& header_value) {
    SipAuthInfo auth_info;
    
    // 解析格式：Digest realm="...", nonce="...", ...
    size_t space_pos = header_value.find(' ');
    if (space_pos != std::string::npos) {
        auth_info.scheme = header_value.substr(0, space_pos);
        std::string params = header_value.substr(space_pos + 1);
        
        // 简单解析参数（实际应该使用更严格的解析）
        size_t pos = 0;
        while (pos < params.length()) {
            size_t eq_pos = params.find('=', pos);
            if (eq_pos == std::string::npos) break;
            
            std::string key = params.substr(pos, eq_pos - pos);
            // 移除首尾空格
            key.erase(0, key.find_first_not_of(' '));
            key.erase(key.find_last_not_of(' ') + 1);
            
            size_t quote_start = params.find('"', eq_pos + 1);
            size_t quote_end = params.find('"', quote_start + 1);
            
            if (quote_start != std::string::npos && quote_end != std::string::npos) {
                std::string value = params.substr(quote_start + 1, quote_end - quote_start - 1);
                
                if (key == "realm") {
                    auth_info.realm = value;
                } else if (key == "nonce") {
                    auth_info.nonce = value;
                } else if (key == "opaque") {
                    auth_info.opaque = value;
                } else if (key == "algorithm") {
                    auth_info.algorithm = value;
                } else if (key == "qop") {
                    auth_info.qop = value;
                } else if (key == "stale") {
                    auth_info.stale = value;
                }
                
                pos = quote_end + 1;
                // 跳过逗号和空格
                pos = params.find(',', pos);
                if (pos == std::string::npos) break;
                pos++;  // 跳过逗号
            } else {
                break;
            }
        }
    }
    
    return auth_info;
}

std::string CalculateSipDigestResponse(
    const std::string& username,
    const std::string& password,
    const std::string& realm,
    const std::string& nonce,
    const std::string& method,
    const std::string& uri,
    const std::string& nc,
    const std::string& cnonce,
    const std::string& qop
) {
    // SIP Digest 认证响应计算
    // response = MD5(HA1:nonce:HA2)  （如果 qop 为空）
    // response = MD5(HA1:nonce:nc:cnonce:qop:HA2)  （如果 qop 不为空）
    
    // HA1 = MD5(username:realm:password)
    std::string ha1 = MD5(username + ":" + realm + ":" + password);
    
    // HA2 = MD5(method:uri)
    std::string ha2 = MD5(method + ":" + uri);
    
    // 计算 response
    std::string response_input;
    if (!qop.empty()) {
        response_input = ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2;
    } else {
        response_input = ha1 + ":" + nonce + ":" + ha2;
    }
    
    return MD5(response_input);
}

} // namespace gb28181
