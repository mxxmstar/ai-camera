#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace gb28181 {

// ============================================================================
// SipMessage: Base class for SIP messages (requests and responses)
// ============================================================================
class SipMessage {
public:
    // SIP message type
    enum class Type {
        REQUEST,   // Request
        RESPONSE   // Response
    };

    // SIP method type
    enum class Method {
        UNKNOWN,
        REGISTER,  // Register
        INVITE,    // Invite (for video streaming)
        ACK,       // Acknowledge
        BYE,       // End session
        CANCEL,    // Cancel
        MESSAGE,   // Message (for GB28181 XML message transmission)
        OPTIONS,   // Options
        SUBSCRIBE, // Subscribe
        NOTIFY     // Notify
    };

    // SIP response status codes
    enum class StatusCode {
        TRYING              = 100,  // Trying
        RINGING             = 180,  // Ringing
        OK                  = 200,  // OK
        MULTIPLE_CHOICES    = 300,  // Multiple Choices
        MOVED_PERMANENTLY   = 301,  // Moved Permanently
        BAD_REQUEST         = 400,  // Bad Request
        UNAUTHORIZED        = 401,  // Unauthorized
        FORBIDDEN           = 403,  // Forbidden
        NOT_FOUND           = 404,  // Not Found
        METHOD_NOT_ALLOWED  = 405,  // Method Not Allowed
        REQUEST_TIMEOUT     = 408,  // Request Timeout
        INTERNAL_ERROR      = 500,  // Internal Server Error
        SERVICE_UNAVAILABLE = 503   // Service Unavailable
    };

    SipMessage() = default;
    virtual ~SipMessage() = default;

    // Get message type
    Type GetType() const { return type_; }

    // Get/Set SIP method
    Method GetMethod() const { return method_; }
    void SetMethod(Method method) { method_ = method; }

    // Get/Set SIP request URI
    const std::string& GetRequestUri() const { return request_uri_; }
    void SetRequestUri(const std::string& uri) { request_uri_ = uri; }

    // Get/Set status code (response only)
    uint16_t GetStatusCode() const { return status_code_; }
    void SetStatusCode(uint16_t code) { status_code_ = code; }

    // Get/Set reason phrase (response only)
    const std::string& GetReasonPhrase() const { return reason_phrase_; }
    void SetReasonPhrase(const std::string& phrase) { reason_phrase_ = phrase; }

    // SIP header operations
    void SetHeader(const std::string& name, const std::string& value);
    std::string GetHeader(const std::string& name) const;
    bool HasHeader(const std::string& name) const;
    void RemoveHeader(const std::string& name);

    // Get all headers
    const std::map<std::string, std::string>& GetHeaders() const { return headers_; }

    // Get/Set message body
    const std::string& GetBody() const { return body_; }
    void SetBody(const std::string& body) { body_ = body; }

    // Convenience methods for essential SIP headers
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

    // Get Content-Length
    size_t GetContentLength() const { return body_.length(); }

    // Serialize to SIP message string
    virtual std::string Serialize() const;

    // Parse SIP message from string (static factory method)
    static SipMessage* Parse(const std::string& raw_message);

    // Helper: Convert method enum to string
    static std::string MethodToString(Method method);
    static Method StringToMethod(const std::string& method_str);

    // Helper: Generate unique Call-ID
    static std::string GenerateCallId(const std::string& local_ip);

    // Helper: Generate branch parameter
    static std::string GenerateBranch();

    // Helper: Generate tag
    static std::string GenerateTag();

protected:
    Type type_ = Type::REQUEST;          // Message type
    Method method_ = Method::UNKNOWN;    // SIP method
    std::string request_uri_;             // Request URI (request only)
    uint16_t status_code_ = 0;           // Status code (response only)
    std::string reason_phrase_;          // Reason phrase (response only)
    std::map<std::string, std::string> headers_;  // SIP headers
    std::string body_;                    // Message body
};

// ============================================================================
// SipRequest: SIP request message
// ============================================================================
class SipRequest : public SipMessage {
public:
    SipRequest();
    explicit SipRequest(Method method, const std::string& request_uri);
    
    std::string Serialize() const override;
};

// ============================================================================
// SipResponse: SIP response message
// ============================================================================
class SipResponse : public SipMessage {
public:
    SipResponse();
    SipResponse(uint16_t status_code, const std::string& reason_phrase);
    
    std::string Serialize() const override;
};

// ============================================================================
// SipAuth: SIP authentication info (for WWW-Authenticate and Authorization headers)
// ============================================================================
struct SipAuthInfo {
    std::string scheme;      // Authentication scheme (usually "Digest")
    std::string realm;       // Authentication realm
    std::string nonce;       // Server nonce
    std::string opaque;      // Opaque value (optional)
    std::string algorithm;    // Algorithm (usually "MD5")
    std::string qop;         // Quality of protection (optional, e.g. "auth")
    std::string stale;       // Stale flag (optional)
    
    // Fields for Authorization header
    std::string username;    // Username
    std::string uri;         // URI
    std::string response;    // Response digest
    std::string nc;          // Nonce count (optional)
    std::string cnonce;      // Client nonce (optional)
};

// Parse WWW-Authenticate header and extract authentication info
SipAuthInfo ParseWwwAuthenticate(const std::string& header_value);

// Calculate SIP Digest authentication response
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
