#pragma once

#include <string>
#include <memory>
#include "tinyxml2.h"

namespace onvif {

// ============================================================================
// 解析后的 SOAP 请求信息
// ============================================================================
struct SoapRequest {
    std::string action;        // wsa:Action（如 http://www.onvif.org/ver10/device/wsdl/GetCapabilities）
    std::string message_id;     // wsa:MessageID（urn:uuid:...）
    std::string relates_to;    // wsa:RelatesTo（响应时填请求的 MessageID）
    std::string operation;      // 操作名（如 GetCapabilities、GetProfiles）
    std::string body_xml;       // SOAP Body 内部的 XML 片段（原始）
    tinyxml2::XMLDocument*     doc = nullptr;  // 解析后的 XML 文档（调用方不负责释放）
};

// ============================================================================
// SOAP 工具类：解析请求 / 构造响应
// ============================================================================
class SoapHelper {
public:
    SoapHelper()  = default;
    ~SoapHelper() = default;

    // ------------------------------------------------------------------
    // 解析 SOAP 请求体（完整的 HTTP body 字符串）
    // 返回 true 表示解析成功，结果填入 req
    // ------------------------------------------------------------------
    bool ParseSoapRequest(const std::string& soap_body, SoapRequest& req) const;

    // ------------------------------------------------------------------
    // 构造 SOAP 响应
    //   action     : wsa:Action（响应操作名，通常为 请求Action + "Response"）
    //   relates_to : 对应请求的 wsa:MessageID
    //   body_xml   : SOAP Body 内部的 XML 片段（不含 <SOAP-ENV:Body> 标签）
    // ------------------------------------------------------------------
    std::string BuildSoapResponse(const std::string& action,
                                 const std::string& relates_to,
                                 const std::string& body_xml) const;

    // ------------------------------------------------------------------
    // 构造 SOAP Fault 响应
    //   fault_code  : SOAP 1.2 错误码，如 "SOAP-ENV:Sender"
    //   fault_reason: 人类可读的错误原因
    //   detail_xml  :（可选）详细的错误 XML 片段
    // ------------------------------------------------------------------
    std::string BuildFaultResponse(const std::string& fault_code,
                                const std::string& fault_reason,
                                const std::string& detail_xml = "") const;

    // ------------------------------------------------------------------
    // 辅助：从已解析的 XMLDocument 中提取指定路径的第一个元素的文本内容
    // 路径格式："/Envelope/Body/Operation/ElementName"
    // ------------------------------------------------------------------
    static std::string GetElementText(tinyxml2::XMLDocument* doc,
                                    const char* element_name);

    // ------------------------------------------------------------------
    // 辅助：生成 UUID（用于 MessageID / SubscriptionReference 等）
    // ------------------------------------------------------------------
    static std::string GenerateUuid();

private:
    // 从 XML 中提取 wsa:Action
    std::string ExtractAction(tinyxml2::XMLDocument* doc) const;
    // 从 XML 中提取 wsa:MessageID
    std::string ExtractMessageId(tinyxml2::XMLDocument* doc) const;
    // 从 Action URL 中提取操作名（最后一个 '/' 之后的部分）
    std::string ExtractOperationName(const std::string& action) const;
};

} // namespace onvif
