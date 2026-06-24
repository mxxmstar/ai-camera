#include "onvif/device_service.h"
#include "onvif/onvif_manager.h"   // for OnvifManager::Instance() to get config
#include <sstream>
#include <iomanip>
#include <chrono>

namespace onvif {

// =========================================================================
// 主入口：解析 SOAP 请求并分发
// =========================================================================
std::string DeviceService::HandleRequest(const std::string& soap_body) {
    SoapRequest req;
    if (!soap_helper_.ParseSoapRequest(soap_body, req)) {
        return soap_helper_.BuildFaultResponse(
            "SOAP-ENV:Sender", "Invalid SOAP request");
    }

    std::string response_xml;
    if (req.operation == "GetCapabilities") {
        response_xml = HandleGetCapabilities(req);
    } else if (req.operation == "GetDeviceInformation") {
        response_xml = HandleGetDeviceInformation(req);
    } else if (req.operation == "GetSystemDateAndTime") {
        response_xml = HandleGetSystemDateAndTime(req);
    } else if (req.operation == "GetServices") {
        response_xml = HandleGetServices(req);
    } else if (req.operation == "GetScopes") {
        response_xml = HandleGetScopes(req);
    } else if (req.operation == "GetHostname") {
        response_xml = HandleGetHostname(req);
    } else {
        response_xml = soap_helper_.BuildFaultResponse(
            "SOAP-ENV:Sender",
            "Unsupported operation: " + req.operation);
    }

    // response_xml 已经是完整的 SOAP 响应（含 Envelope）
    return response_xml;
}

// =========================================================================
// GetCapabilities：返回设备能力描述
// ONVIF 2.4 Core Spec, Section 7.1.1
// =========================================================================
std::string DeviceService::HandleGetCapabilities(const SoapRequest& req) {
    std::string body_xml = BuildCapabilitiesResponse(req.message_id);
    return soap_helper_.BuildSoapResponse(
        XmlNs::tds + std::string("/GetCapabilitiesResponse"),
        req.message_id,
        body_xml);
}

std::string DeviceService::BuildCapabilitiesResponse(const std::string& /*token*/) const {
    // @note: 此处仅返回核心服务端点；完整实现可按 Category 过滤
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tds:GetCapabilitiesResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLElement* caps = doc.NewElement("tds:Capabilities");
    resp->InsertEndChild(caps);

    // --- Device 能力 ---
    tinyxml2::XMLElement* dev = doc.NewElement("tds:Device");
    dev->SetAttribute("XAddr", config_.DeviceServiceURL().c_str());
    // 支持的基础操作
    tinyxml2::XMLElement* sys = doc.NewElement("tds:System");
    sys->SetText("true");
    dev->InsertEndChild(sys);
    tinyxml2::XMLElement* io = doc.NewElement("tds:IO");
    io->SetText("false");
    dev->InsertEndChild(io);
    tinyxml2::XMLElement* sec = doc.NewElement("tds:Security");
    sec->SetText("false");   // 暂不实现 WS-Security
    dev->InsertEndChild(sec);
    caps->InsertEndChild(dev);

    // --- Media 能力 ---
    tinyxml2::XMLElement* media = doc.NewElement("tds:Media");
    media->SetAttribute("XAddr", config_.MediaServiceURL().c_str());
    tinyxml2::XMLElement* stream = doc.NewElement("tds:Streaming");
    stream->SetText("true");
    media->InsertEndChild(stream);
    caps->InsertEndChild(media);

    // --- Events 能力 ---
    tinyxml2::XMLElement* events = doc.NewElement("tds:Events");
    events->SetAttribute("XAddr", config_.EventsServiceURL().c_str());
    tinyxml2::XMLElement* wss = doc.NewElement("tds:WSSubscription");
    wss->SetText("true");
    events->InsertEndChild(wss);
    tinyxml2::XMLElement* pull = doc.NewElement("tds:Pull");
    pull->SetText("true");
    events->InsertEndChild(pull);
    caps->InsertEndChild(events);

    // --- PTZ / Imaging 等暂不支持 ---
    // <tds:PTZ XAddr="..."/>  ← 不输出表示不支持

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

// =========================================================================
// GetDeviceInformation：返回设备制造商/型号/固件版本
// ONVIF 2.4 Core Spec, Section 7.1.2
// =========================================================================
std::string DeviceService::HandleGetDeviceInformation(const SoapRequest& req) {
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tds:GetDeviceInformationResponse");
    doc.InsertEndChild(resp);

    auto add_text = [&](const char* name, const std::string& val) {
        tinyxml2::XMLElement* e = doc.NewElement(name);
        e->SetText(val.c_str());
        resp->InsertEndChild(e);
    };
    add_text("tds:Manufacturer",    dev_info_.manufacturer);
    add_text("tds:Model",           dev_info_.model);
    add_text("tds:FirmwareVersion", dev_info_.firmware);
    add_text("tds:SerialNumber",    dev_info_.serial);
    add_text("tds:HardwareId",      dev_info_.hardware_id);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tds + std::string("/GetDeviceInformationResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetSystemDateAndTime：返回设备系统时间
// ONVIF 2.4 Core Spec, Section 7.1.3
// =========================================================================
std::string DeviceService::HandleGetSystemDateAndTime(const SoapRequest& req) {
    // 获取当前 UTC 和本地时间
    auto now     = std::chrono::system_clock::now();
    auto now_tt = std::chrono::system_clock::to_time_t(now);

    // 分解为 UTC tm
    std::tm utc_tm;
#ifdef _WIN32
    gmtime_s(&utc_tm, &now_tt);
#else
    gmtime_r(&now_tt, &utc_tm);
#endif

    // 时区：使用本地时区（Windows 用 _get_timezone）
    int tz_offset_min = 0;  // 相对于 UTC 的分钟数（东为正）
#if defined(_WIN32)
    long tz_sec = 0;
    _get_timezone(&tz_sec);   // Windows: 返回 UTC - 本地（秒），与 POSIX 相反
    tz_offset_min = -static_cast<int>(tz_sec) / 60;
#else
    std::tm local_tm;
    localtime_r(&now_tt, &local_tm);
    tz_offset_min = (local_tm.tm_hour - utc_tm.tm_hour) * 60 +
                    (local_tm.tm_min  - utc_tm.tm_min);
    // 处理跨日
    if (tz_offset_min > 720)  tz_offset_min -= 1440;
    if (tz_offset_min < -720) tz_offset_min += 1440;
#endif

    auto format_tz = [](int offset_min) -> std::string {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%c%02d:%02d",
                      (offset_min >= 0 ? '+' : '-'),
                      std::abs(offset_min) / 60,
                      std::abs(offset_min) % 60);
        return buf;
    };

    auto format_dt = [](const std::tm& t) -> std::string {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec);
        return buf;
    };

    std::string utc_str     = format_dt(utc_tm);
    std::string local_str  = format_dt(utc_tm);  // 近似；精确需本地 tm
    std::string tz_str     = format_tz(tz_offset_min);

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tds:GetSystemDateAndTimeResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLElement* dt = doc.NewElement("tds:SystemDateAndTime");
    resp->InsertEndChild(dt);

    tinyxml2::XMLElement* ttype = doc.NewElement("tds:DateTimeType");
    ttype->SetText("Manual");   // 或 "NTP"
    dt->InsertEndChild(ttype);

    tinyxml2::XMLElement* dst = doc.NewElement("tds:DaylightSavings");
    dst->SetText("false");
    dt->InsertEndChild(dst);

    // --- UTC 时间 ---
    tinyxml2::XMLElement* utc = doc.NewElement("tds:UTCDateTime");
    dt->InsertEndChild(utc);
    tinyxml2::XMLElement* t1 = doc.NewElement("tt:Time");
    t1->SetAttribute("Hour",   utc_tm.tm_hour);
    t1->SetAttribute("Minute", utc_tm.tm_min);
    t1->SetAttribute("Second", utc_tm.tm_sec);
    utc->InsertEndChild(t1);
    tinyxml2::XMLElement* d1 = doc.NewElement("tt:Date");
    d1->SetAttribute("Year",  utc_tm.tm_year + 1900);
    d1->SetAttribute("Month", utc_tm.tm_mon + 1);
    d1->SetAttribute("Day",   utc_tm.tm_mday);
    utc->InsertEndChild(d1);

    // --- 本地时间 + 时区 ---
    tinyxml2::XMLElement* local = doc.NewElement("tds:LocalDateTime");
    dt->InsertEndChild(local);
    tinyxml2::XMLElement* t2 = doc.NewElement("tt:Time");
    t2->SetAttribute("Hour",   utc_tm.tm_hour);
    t2->SetAttribute("Minute", utc_tm.tm_min);
    t2->SetAttribute("Second", utc_tm.tm_sec);
    local->InsertEndChild(t2);
    tinyxml2::XMLElement* d2 = doc.NewElement("tt:Date");
    d2->SetAttribute("Year",  utc_tm.tm_year + 1900);
    d2->SetAttribute("Month", utc_tm.tm_mon + 1);
    d2->SetAttribute("Day",   utc_tm.tm_mday);
    local->InsertEndChild(d2);
    tinyxml2::XMLElement* tz = doc.NewElement("tt:TimeZone");
    tz->SetText(tz_str.c_str());
    local->InsertEndChild(tz);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tds + std::string("/GetSystemDateAndTimeResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetServices：返回各服务的端点 URL 和能力
// ONVIF 2.4 Core Spec, Section 7.1.4
// =========================================================================
std::string DeviceService::HandleGetServices(const SoapRequest& req) {
    // 可选参数：IncludeCapability（bool）
    bool include_cap = false;
    if (req.doc) {
        tinyxml2::XMLElement* env  = req.doc->FirstChildElement("Envelope");
        if (!env) env = req.doc->FirstChildElement("SOAP-ENV:Envelope");
        if (env) {
            tinyxml2::XMLElement* body = env->FirstChildElement("Body");
            if (body) {
                tinyxml2::XMLElement* op = body->FirstChildElement("GetServices");
                if (op) {
                    tinyxml2::XMLElement* cap = op->FirstChildElement("IncludeCapability");
                    if (cap && cap->GetText()) {
                        include_cap = (std::string(cap->GetText()) == "true");
                    }
                }
            }
        }
    }

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tds:GetServicesResponse");
    doc.InsertEndChild(resp);

    // --- Device Service ---
    {
        tinyxml2::XMLElement* svc = doc.NewElement("tds:Service");
        resp->InsertEndChild(svc);

        tinyxml2::XMLElement* ns = doc.NewElement("tds:Namespace");
        ns->SetText(XmlNs::tds);
        svc->InsertEndChild(ns);

        tinyxml2::XMLElement* xaddr = doc.NewElement("tds:XAddr");
        xaddr->SetText(config_.DeviceServiceURL().c_str());
        svc->InsertEndChild(xaddr);

        tinyxml2::XMLElement* ver = doc.NewElement("tds:Version");
        ver->SetAttribute("Major", 1);
        ver->SetAttribute("Minor", 0);
        svc->InsertEndChild(ver);

        if (include_cap) {
            // 简化的能力 XML（与 GetCapabilities 对齐）
            tinyxml2::XMLElement* cap = doc.NewElement("tds:Capabilities");
            tinyxml2::XMLElement* d = doc.NewElement("tds:Device");
            d->SetAttribute("XAddr", config_.DeviceServiceURL().c_str());
            cap->InsertEndChild(d);
            svc->InsertEndChild(cap);
        }
    }

    // --- Media Service ---
    {
        tinyxml2::XMLElement* svc = doc.NewElement("tds:Service");
        resp->InsertEndChild(svc);

        tinyxml2::XMLElement* ns = doc.NewElement("tds:Namespace");
        ns->SetText(XmlNs::trt);
        svc->InsertEndChild(ns);

        tinyxml2::XMLElement* xaddr = doc.NewElement("tds:XAddr");
        xaddr->SetText(config_.MediaServiceURL().c_str());
        svc->InsertEndChild(xaddr);

        tinyxml2::XMLElement* ver = doc.NewElement("tds:Version");
        ver->SetAttribute("Major", 1);
        ver->SetAttribute("Minor", 0);
        svc->InsertEndChild(ver);
    }

    // --- Events Service ---
    {
        tinyxml2::XMLElement* svc = doc.NewElement("tds:Service");
        resp->InsertEndChild(svc);

        tinyxml2::XMLElement* ns = doc.NewElement("tds:Namespace");
        ns->SetText(XmlNs::tev);
        svc->InsertEndChild(ns);

        tinyxml2::XMLElement* xaddr = doc.NewElement("tds:XAddr");
        xaddr->SetText(config_.EventsServiceURL().c_str());
        svc->InsertEndChild(xaddr);

        tinyxml2::XMLElement* ver = doc.NewElement("tds:Version");
        ver->SetAttribute("Major", 1);
        ver->SetAttribute("Minor", 0);
        svc->InsertEndChild(ver);
    }

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tds + std::string("/GetServicesResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetScopes：返回设备 Scopes
// ONVIF 2.4 Core Spec, Section 7.1.5
// =========================================================================
std::string DeviceService::HandleGetScopes(const SoapRequest& req) {
    // 固定 Scopes：设备类型 + 名称
    std::vector<std::string> scopes = {
        DiscoveryScopes::Device,
        DiscoveryScopes::NetworkVideoTransmitter,
        "onvif://www.onvif.org/Profile/" + std::string(OnvifProfiles::ProfileS),
        "onvif://www.onvif.org/name/ai-camera",
        "onvif://www.onvif.org/location/unknown",
    };

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tds:GetScopesResponse");
    doc.InsertEndChild(resp);

    for (const auto& scope : scopes) {
        tinyxml2::XMLElement* item = doc.NewElement("tds:Scopes");
        resp->InsertEndChild(item);

        tinyxml2::XMLElement* scope_field = doc.NewElement("tds:Scope");
        scope_field->SetText(scope.c_str());
        item->InsertEndChild(scope_field);

        tinyxml2::XMLElement* level = doc.NewElement("tds:Level");
        // Fixed = 启动时固定；Configurable = 可修改
        level->SetText("Fixed");
        item->InsertEndChild(level);
    }

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tds + std::string("/GetScopesResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetHostname：返回设备主机名配置
// ONVIF 2.4 Core Spec, Section 7.1.6
// =========================================================================
std::string DeviceService::HandleGetHostname(const SoapRequest& req) {
    // 使用设备 IP 作为近似主机名（真实实现应从系统获取）
    std::string hostname = config_.device_ip;

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tds:GetHostnameResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLElement* hostname_cfg = doc.NewElement("tds:HostnameInformation");
    resp->InsertEndChild(hostname_cfg);

    tinyxml2::XMLElement* name = doc.NewElement("tds:Name");
    name->SetText(hostname.c_str());
    hostname_cfg->InsertEndChild(name);

    tinyxml2::XMLElement* from_dhcp = doc.NewElement("tds:FromDHCP");
    from_dhcp->SetText("false");
    hostname_cfg->InsertEndChild(from_dhcp);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tds + std::string("/GetHostnameResponse"),
        req.message_id,
        printer.CStr());
}

} // namespace onvif
