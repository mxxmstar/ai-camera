#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace gb28181 {

// ============================================================================
// SipMessage：SIP 消息基类，用于表示 SIP 请求和响应
// ============================================================================
class SipMessage {
public:
    // SIP 消息类型
    enum class Type {
        REQUEST,   // 请求
        RESPONSE   // 响应
    };

    // SIP 方法类型
    enum class Method {
        UNKNOWN,
        REGISTER,  // 注册
        INVITE,    // 邀请（用于视频点播）
        ACK,       // 确认
        BYE,       // 结束会话
        CANCEL,    // 取消
        MESSAGE,   // 消息（用于 GB28181 XML 报文传输）
        OPTIONS,   // 选项
        SUBSCRIBE, // 订阅
        NOTIFY     // 通知
    };

    // SIP 响应状态码
    enum class StatusCode {
        TRYING              = 100,  // 正在尝试
        RINGING             = 180,  // 响铃
        OK                  = 200,  // 成功
        MULTIPLE_CHOICES    = 300,  // 多个选择
        MOVED_PERMANENTLY   = 301,  // 永久移动
        BAD_REQUEST         = 400,  // 错误请求
        UNAUTHORIZED        = 401,  // 未授权
        FORBIDDEN           = 403,  // 禁止
        NOT_FOUND           = 404,  // 未找到
        METHOD_NOT_ALLOWED  = 405,  // 方法不允许
        REQUEST_TIMEOUT     = 408,  // 请求超时
        INTERNAL_ERROR      = 500,  // 内部错误
        SERVICE_UNAVAILABLE = 503   // 服务不可用
    };

    SipMessage() = default;
    virtual ~SipMessage() = default;

    // 获取消息类型
    Type GetType() const { return type_; }

    // 获取/设置 SIP 方法
    Method GetMethod() const { return method_; }
    void SetMethod(Method method) { method_ = method; }

    // 获取/设置 SIP 请求 URI
    const std::string& GetRequestUri() const { return request_uri_; }
    void SetRequestUri(const std::string& uri) { request_uri_ = uri; }

    // 获取/设置状态码（仅响应）
    uint16_t GetStatusCode() const { return status_code_; }
    void SetStatusCode(uint16_t code) { status_code_ = code; }

    // 获取/设置原因短语（仅响应）
    const std::string& GetReasonPhrase() const { return reason_phrase_; }
    void SetReasonPhrase(const std::string& phrase) { reason_phrase_ = phrase; }

    // SIP 头字段操作
    void SetHeader(const std::string& name, const std::string& value);
    std::string GetHeader(const std::string& name) const;
    bool HasHeader(const std::string& name) const;
    void RemoveHeader(const std::string& name);

    // 获取所有头字段
    const std::map<std::string, std::string>& GetHeaders() const { return headers_; }

    // 获取/设置消息体
    const std::string& GetBody() const { return body_; }
    void SetBody(const std::string& body) { body_ = body; }

    // SIP 必要头字段的便捷访问
    std::string GetCallId() const { return GetHeader("Call-ID"); }
    void SetCallId(const std::string& call_id) { SetHeader("Call-ID", call_id); }

    std::string GetFrom() const { return GetHeader("From"); }
    void SetFrom(const std::string& from) { SetHeader("From", from); }

    std::string GetTo() const { return GetHeader("To"); }
    void SetTo(const std::string& to) { SetHeader("To", to); }

    std::string GetCSeq() const { return GetHeader("CSeq"); }
    void SetCSeq(const std::string& cseq) { SetHeader("CSeq", cseq); }

    std::string GetVia() const { return GetHeader("Via"); }
    void SetVia(const std::string& via) { SetHeader("Via", via); }

    std::string GetContact() const { return GetHeader("Contact"); }
    void SetContact(const std::string& contact) { SetHeader("Contact", contact); }

    // 获取 Content-Length
    size_t GetContentLength() const { return body_.length(); }

    // 序列化为 SIP 消息字符串
    virtual std::string Serialize() const;

    // 从字符串解析 SIP 消息（静态工厂方法）
    static SipMessage* Parse(const std::string& raw_message);

    // 辅助方法：将方法枚举转换为字符串
    static std::string MethodToString(Method method);
    static Method StringToMethod(const std::string& method_str);

    // 辅助方法：生成唯一的 Call-ID
    static std::string GenerateCallId(const std::string& local_ip);

    // 辅助方法：生成分支参数
    static std::string GenerateBranch();

    // 辅助方法：生成标签（tag）
    static std::string GenerateTag();

protected:
    Type type_ = Type::REQUEST;          // 消息类型
    Method method_ = Method::UNKNOWN;    // SIP 方法
    std::string request_uri_;             // 请求 URI（仅请求）
    uint16_t status_code_ = 0;           // 状态码（仅响应）
    std::string reason_phrase_;          // 原因短语（仅响应）
    std::map<std::string, std::string> headers_;  // SIP 头字段
    std::string body_;                    // 消息体
};

// ============================================================================
// SipRequest：SIP 请求消息
// ============================================================================
class SipRequest : public SipMessage {
public:
    SipRequest();
    explicit SipRequest(Method method, const std::string& request_uri);
    
    std::string Serialize() const override;
};

// ============================================================================
// SipResponse：SIP 响应消息
// ============================================================================
class SipResponse : public SipMessage {
public:
    SipResponse();
    SipResponse(uint16_t status_code, const std::string& reason_phrase);
    
    std::string Serialize() const override;
};

// ============================================================================
// SipAuth：SIP 认证信息（用于 WWW-Authenticate 和 Authorization 头）
// ============================================================================
struct SipAuthInfo {
    std::string scheme;      // 认证方案（通常为 "Digest"）
    std::string realm;       // 认证域
    std::string nonce;       // 服务器随机数
    std::string opaque;      // 不透明值（可选）
    std::string algorithm;    // 算法（通常为 "MD5"）
    std::string qop;         // 保护质量（可选，如 "auth"）
    std::string stale;       // 过期标志（可选）
    
    // 用于 Authorization 头的字段
    std::string username;    // 用户名
    std::string uri;         // URI
    std::string response;    // 响应摘要
    std::string nc;          // 随机数计数（可选）
    std::string cnonce;      // 客户端随机数（可选）
};

// 解析 WWW-Authenticate 头，提取认证信息
SipAuthInfo ParseWwwAuthenticate(const std::string& header_value);

// 计算 SIP Digest 认证响应
std::string CalculateSipDigestResponse(
    const std::string& username,
    const std::string& password,
    const std::string& realm,
    const std::string& nonce,
    const std::string& method,
    const std::string& uri,
    const std::string& nc = "",
    const std::string& cnonce = "",
    const std::string& qop = ""
);

} // namespace gb28181
