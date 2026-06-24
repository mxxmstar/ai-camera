#include "onvif/soap_helper.h"
#include "onvif/onvif_types.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace onvif {

static const char* SOAP_ENV_PREFIX  = "SOAP-ENV";
static const char* WSA_PREFIX       = "wsa";
static const char* TDS_PREFIX       = "tds";
static const char* TRT_PREFIX       = "trt";
static const char* TEV_PREFIX       = "tev";

// =========================================================================
// 解析 SOAP 请求
// =========================================================================
bool SoapHelper::ParseSoapRequest(const std::string& soap_body, SoapRequest& req) const {
    req.doc = new tinyxml2::XMLDocument();
    if (req.doc->Parse(soap_body.c_str()) != tinyxml2::XML_SUCCESS) {
        delete req.doc;
        req.doc = nullptr;
        return false;
    }

    // 提取 wsa:Action
    req.action = ExtractAction(req.doc);
    // 提取 wsa:MessageID
    req.message_id = ExtractMessageId(req.doc);
    // 从 Action 提取操作名
    req.operation = ExtractOperationName(req.action);

    // 提取 SOAP Body 内部 XML（第一个子元素及其内容）
    tinyxml2::XMLElement* env = req.doc->FirstChildElement("Envelope");
    if (!env) env = req.doc->FirstChildElement("SOAP-ENV:Envelope");
    if (env) {
        tinyxml2::XMLElement* body = env->FirstChildElement("Body");
        if (!body) body = env->FirstChildElement("SOAP-ENV:Body");
        if (body) {
            tinyxml2::XMLElement* op = body->FirstChildElement();
            if (op) {
                // 序列化 Body 内第一个元素（即实际操作请求）
                tinyxml2::XMLPrinter printer;
                op->Accept(&printer);
                req.body_xml = printer.CStr();
            }
        }
    }

    return true;
}

// =========================================================================
// 构造 SOAP 响应
// =========================================================================
std::string SoapHelper::BuildSoapResponse(const std::string& action,
                                           const std::string& relates_to,
                                           const std::string& body_xml) const {
    tinyxml2::XMLDocument doc;

    // <?xml version="1.0" encoding="UTF-8"?>
    doc.InsertEndChild(doc.NewDeclaration());

    // <SOAP-ENV:Envelope ...namespaces...>
    tinyxml2::XMLElement* env = doc.NewElement("SOAP-ENV:Envelope");
    env->SetAttribute("xmlns:SOAP-ENV", XmlNs::env);
    env->SetAttribute("xmlns:SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/");
    env->SetAttribute("xmlns:xsi", XmlNs::xsi);
    env->SetAttribute("xmlns:xsd", XmlNs::xsd);
    env->SetAttribute("xmlns:wsa", XmlNs::wsa);
    env->SetAttribute("xmlns:tds", XmlNs::tds);
    env->SetAttribute("xmlns:trt", XmlNs::trt);
    env->SetAttribute("xmlns:tev", XmlNs::tev);
    env->SetAttribute("xmlns:tt", XmlNs::tts);
    doc.InsertEndChild(env);

    // <SOAP-ENV:Header>
    tinyxml2::XMLElement* hdr = doc.NewElement("SOAP-ENV:Header");
    env->InsertEndChild(hdr);

    // <wsa:Action>
    tinyxml2::XMLElement* act = doc.NewElement("wsa:Action");
    act->SetAttribute("SOAP-ENV:mustUnderstand", "true");
    act->SetText(action.c_str());
    hdr->InsertEndChild(act);

    // <wsa:RelatesTo>
    if (!relates_to.empty()) {
        tinyxml2::XMLElement* rel = doc.NewElement("wsa:RelatesTo");
        rel->SetText(relates_to.c_str());
        hdr->InsertEndChild(rel);
    }

    // <SOAP-ENV:Body>
    tinyxml2::XMLElement* body = doc.NewElement("SOAP-ENV:Body");
    env->InsertEndChild(body);

    // 将 body_xml 字符串解析后插入 Body
    if (!body_xml.empty()) {
        tinyxml2::XMLDocument tmp;
        if (tmp.Parse(body_xml.c_str()) == tinyxml2::XML_SUCCESS) {
            tinyxml2::XMLNode* root = tmp.FirstChild();
            if (root) {
                tinyxml2::XMLNode* cloned = root->DeepClone(&doc);
                if (cloned) {
                    body->InsertEndChild(cloned);
                }
            }
        }
    }

    // 序列化为字符串
    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

// =========================================================================
// 构造 SOAP Fault 响应（SOAP 1.2）
// =========================================================================
std::string SoapHelper::BuildFaultResponse(const std::string& fault_code,
                                            const std::string& fault_reason,
                                            const std::string& detail_xml) const {
    tinyxml2::XMLDocument doc;

    doc.InsertEndChild(doc.NewDeclaration());

    tinyxml2::XMLElement* env = doc.NewElement("SOAP-ENV:Envelope");
    env->SetAttribute("xmlns:SOAP-ENV", XmlNs::env);
    env->SetAttribute("xmlns:wsa", XmlNs::wsa);
    doc.InsertEndChild(env);

    tinyxml2::XMLElement* body = doc.NewElement("SOAP-ENV:Body");
    env->InsertEndChild(body);

    tinyxml2::XMLElement* fault = doc.NewElement("SOAP-ENV:Fault");
    body->InsertEndChild(fault);

    // SOAP 1.2: <Code><Value>fault_code</Value></Code>
    tinyxml2::XMLElement* code = doc.NewElement("SOAP-ENV:Code");
    tinyxml2::XMLElement* value = doc.NewElement("SOAP-ENV:Value");
    value->SetText(fault_code.c_str());
    code->InsertEndChild(value);
    fault->InsertEndChild(code);

    // <Reason><Text>fault_reason</Text></Reason>
    tinyxml2::XMLElement* reason = doc.NewElement("SOAP-ENV:Reason");
    tinyxml2::XMLElement* text = doc.NewElement("SOAP-ENV:Text");
    text->SetAttribute("xml:lang", "en");
    text->SetText(fault_reason.c_str());
    reason->InsertEndChild(text);
    fault->InsertEndChild(reason);

    // <Detail>（可选）
    if (!detail_xml.empty()) {
        tinyxml2::XMLElement* detail = doc.NewElement("SOAP-ENV:Detail");
        tinyxml2::XMLDocument tmp;
        if (tmp.Parse(detail_xml.c_str()) == tinyxml2::XML_SUCCESS) {
            tinyxml2::XMLNode* root = tmp.FirstChild();
            if (root) {
                tinyxml2::XMLNode* cloned = root->DeepClone(&doc);
                if (cloned) detail->InsertEndChild(cloned);
            }
        }
        fault->InsertEndChild(detail);
    }

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

// =========================================================================
// 辅助：提取元素文本内容
// =========================================================================
std::string SoapHelper::GetElementText(tinyxml2::XMLDocument* doc,
                                        const char* element_name) {
    if (!doc || !element_name) return "";
    tinyxml2::XMLElement* elem = doc->FirstChildElement(element_name);
    if (!elem) {
        // 尝试在 Body 下查找
        tinyxml2::XMLElement* env = doc->FirstChildElement("Envelope");
        if (env) {
            tinyxml2::XMLElement* body = env->FirstChildElement("Body");
            if (body) {
                elem = body->FirstChildElement(element_name);
                if (!elem) {
                    // 尝试带命名空间前缀
                    std::string prefixed = std::string("tds:") + element_name;
                    elem = body->FirstChildElement(prefixed.c_str());
                    if (!elem) {
                        prefixed = std::string("trt:") + element_name;
                        elem = body->FirstChildElement(prefixed.c_str());
                    }
                }
            }
        }
    }
    if (elem && elem->GetText()) {
        return std::string(elem->GetText());
    }
    return "";
}

// =========================================================================
// 辅助：生成 UUID（UUID v4）
// =========================================================================
std::string SoapHelper::GenerateUuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    auto hex_digit = []() -> char {
        int d = dis(gen);
        return d < 10 ? ('0' + d) : ('a' + (d - 10));
    };

    char uuid[37];
    // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx  (version 4)
    for (int i = 0; i < 8; ++i) uuid[i] = hex_digit();
    uuid[8] = '-';
    for (int i = 9; i < 13; ++i) uuid[i] = hex_digit();
    uuid[13] = '-';
    uuid[14] = '4';  // version 4
    for (int i = 15; i < 18; ++i) uuid[i] = hex_digit();
    uuid[18] = '-';
    uuid[19] = (hex_digit() & 0x3) | 0x8;  // variant 1
    for (int i = 20; i < 23; ++i) uuid[i] = hex_digit();
    uuid[23] = '-';
    for (int i = 24; i < 36; ++i) uuid[i] = hex_digit();
    uuid[36] = '\0';

    return std::string("urn:uuid:") + uuid;
}

// =========================================================================
// 私有：从 SOAP Header 提取 wsa:Action
// =========================================================================
std::string SoapHelper::ExtractAction(tinyxml2::XMLDocument* doc) const {
    if (!doc) return "";
    tinyxml2::XMLElement* env = doc->FirstChildElement("Envelope");
    if (!env) env = doc->FirstChildElement("SOAP-ENV:Envelope");
    if (env) {
        tinyxml2::XMLElement* hdr = env->FirstChildElement("Header");
        if (!hdr) hdr = env->FirstChildElement("SOAP-ENV:Header");
        if (hdr) {
            tinyxml2::XMLElement* action = hdr->FirstChildElement("Action");
            if (!action) action = hdr->FirstChildElement("wsa:Action");
            if (action && action->GetText()) {
                return std::string(action->GetText());
            }
        }
    }
    return "";
}

// =========================================================================
// 私有：从 SOAP Header 提取 wsa:MessageID
// =========================================================================
std::string SoapHelper::ExtractMessageId(tinyxml2::XMLDocument* doc) const {
    if (!doc) return "";
    tinyxml2::XMLElement* env = doc->FirstChildElement("Envelope");
    if (!env) env = doc->FirstChildElement("SOAP-ENV:Envelope");
    if (env) {
        tinyxml2::XMLElement* hdr = env->FirstChildElement("Header");
        if (!hdr) hdr = env->FirstChildElement("SOAP-ENV:Header");
        if (hdr) {
            tinyxml2::XMLElement* msgId = hdr->FirstChildElement("MessageID");
            if (!msgId) msgId = hdr->FirstChildElement("wsa:MessageID");
            if (msgId && msgId->GetText()) {
                return std::string(msgId->GetText());
            }
        }
    }
    return "";
}

// =========================================================================
// 私有：从 Action URL 提取操作名
// 例："http://www.onvif.org/ver10/device/wsdl/GetCapabilities" -> "GetCapabilities"
// =========================================================================
std::string SoapHelper::ExtractOperationName(const std::string& action) const {
    if (action.empty()) return "";
    size_t pos = action.find_last_of('/');
    if (pos != std::string::npos && pos + 1 < action.size()) {
        return action.substr(pos + 1);
    }
    return action;
}

} // namespace onvif
