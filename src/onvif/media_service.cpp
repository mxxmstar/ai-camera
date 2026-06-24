#include "onvif/media_service.h"
#include "onvif/onvif_manager.h"
#include <chrono>

namespace onvif {

// =========================================================================
// 主入口：解析 SOAP 请求并分发
// =========================================================================
std::string MediaService::HandleRequest(const std::string& soap_body) {
    SoapRequest req;
    if (!soap_helper_.ParseSoapRequest(soap_body, req)) {
        return soap_helper_.BuildFaultResponse(
            "SOAP-ENV:Sender", "Invalid SOAP request");
    }

    std::string response_xml;
    if      (req.operation == "GetProfiles") {
        response_xml = HandleGetProfiles(req);
    } else if (req.operation == "GetStreamUri") {
        response_xml = HandleGetStreamUri(req);
    } else if (req.operation == "GetVideoSources") {
        response_xml = HandleGetVideoSources(req);
    } else if (req.operation == "GetVideoSourceConfigurations") {
        response_xml = HandleGetVideoSourceConfigurations(req);
    } else if (req.operation == "GetVideoEncoderConfigurations") {
        response_xml = HandleGetVideoEncoderConfigurations(req);
    } else if (req.operation == "GetAudioSources") {
        response_xml = HandleGetAudioSources(req);
    } else if (req.operation == "GetAudioSourceConfigurations") {
        response_xml = HandleGetAudioSourceConfigurations(req);
    } else if (req.operation == "GetAudioEncoderConfigurations") {
        response_xml = HandleGetAudioEncoderConfigurations(req);
    } else if (req.operation == "GetProfile") {
        response_xml = HandleGetProfile(req);
    } else if (req.operation == "GetSnapshotUri") {
        response_xml = HandleGetSnapshotUri(req);
    } else {
        response_xml = soap_helper_.BuildFaultResponse(
            "SOAP-ENV:Sender",
            "Unsupported media operation: " + req.operation);
    }
    return response_xml;
}

// =========================================================================
// GetProfiles：返回媒体配置文件列表（Profile S）
// ONVIF Media Service Spec, Section 4.3.1
// =========================================================================
std::string MediaService::HandleGetProfiles(const SoapRequest& req) {
    std::string profiles_xml = BuildProfileToken();

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLPrinter    printer;

    // 将 profiles_xml 解析后插入
    tinyxml2::XMLDocument tmp;
    if (tmp.Parse(profiles_xml.c_str()) == tinyxml2::XML_SUCCESS) {
        tinyxml2::XMLElement* root = tmp.FirstChildElement();
        if (root) {
            tinyxml2::XMLNode* cloned = root->DeepClone(&doc);
            if (cloned) doc.InsertEndChild(cloned);
        }
    }

    tinyxml2::XMLPrinter p2;
    doc.Accept(&p2);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetProfilesResponse"),
        req.message_id,
        p2.CStr());
}

// =========================================================================
// GetStreamUri：返回 RTSP 流地址（对接现有 RtspManager）
// ONVIF Media Service Spec, Section 4.3.3
// =========================================================================
std::string MediaService::HandleGetStreamUri(const SoapRequest& req) {
    // 请求参数（可选）：Stream（RTP-Unicast / RTP-Multicast）
    // 此处忽略，始终返回单播 RTSP URI
    (void)req;

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("trt:GetStreamUriResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLElement* uri = doc.NewElement("trt:Uri");
    uri->SetText(config_.StreamUri().c_str());
    resp->InsertEndChild(uri);

    // 流类型：RTP-Unicast
    tinyxml2::XMLElement* stream = doc.NewElement("trt:Stream");
    stream->SetText("RTP-Unicast");
    resp->InsertEndChild(stream);

    // 传输协议：RTSP
    tinyxml2::XMLElement* tp = doc.NewElement("trt:Transport");
    tp->SetAttribute("Protocol", "RTSP");
    tp->SetAttribute("Tunnel", "None");
    resp->InsertEndChild(tp);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetStreamUriResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetVideoSources：返回视频源配置
// =========================================================================
std::string MediaService::HandleGetVideoSources(const SoapRequest& req) {
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("trt:GetVideoSourcesResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLElement* src = doc.NewElement("trt:VideoSources");
    src->SetAttribute("token", "VideoSource_1");
    src->SetAttribute("Resolution", "1920x1080");
    src->SetAttribute("Framerate", "30");
    resp->InsertEndChild(src);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetVideoSourcesResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetVideoSourceConfigurations：返回视频源配置列表
// =========================================================================
std::string MediaService::HandleGetVideoSourceConfigurations(const SoapRequest& req) {
    std::string cfg_xml = BuildVideoSourceConfiguration();

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLDocument tmp;
    if (tmp.Parse(cfg_xml.c_str()) == tinyxml2::XML_SUCCESS) {
        tinyxml2::XMLElement* root = tmp.FirstChildElement();
        if (root) {
            tinyxml2::XMLNode* cloned = root->DeepClone(&doc);
            if (cloned) doc.InsertEndChild(cloned);
        }
    }

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetVideoSourceConfigurationsResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetVideoEncoderConfigurations：返回视频编码器配置
// =========================================================================
std::string MediaService::HandleGetVideoEncoderConfigurations(const SoapRequest& req) {
    std::string cfg_xml = BuildVideoEncoderConfiguration();

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLDocument tmp;
    if (tmp.Parse(cfg_xml.c_str()) == tinyxml2::XML_SUCCESS) {
        tinyxml2::XMLElement* root = tmp.FirstChildElement();
        if (root) {
            tinyxml2::XMLNode* cloned = root->DeepClone(&doc);
            if (cloned) doc.InsertEndChild(cloned);
        }
    }

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetVideoEncoderConfigurationsResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetAudioSources：返回音频源配置（暂返回空列表）
// =========================================================================
std::string MediaService::HandleGetAudioSources(const SoapRequest& /*req*/) {
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("trt:GetAudioSourcesResponse");
    // 无音频源：返回空响应
    doc.InsertEndChild(resp);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetAudioSourcesResponse"),
        "",  // message_id 在实际请求中填充
        printer.CStr());
}

// =========================================================================
// GetAudioSourceConfigurations：返回音频源配置列表（暂返回空）
// =========================================================================
std::string MediaService::HandleGetAudioSourceConfigurations(const SoapRequest& req) {
    (void)req;
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("trt:GetAudioSourceConfigurationsResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetAudioSourceConfigurationsResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetAudioEncoderConfigurations：返回音频编码器配置（暂返回空）
// =========================================================================
std::string MediaService::HandleGetAudioEncoderConfigurations(const SoapRequest& req) {
    (void)req;
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("trt:GetAudioEncoderConfigurationsResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetAudioEncoderConfigurationsResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetProfile：返回指定 Token 的 Profile 详情
// =========================================================================
std::string MediaService::HandleGetProfile(const SoapRequest& req) {
    std::string token = SoapHelper::GetElementText(req.doc, "ProfileToken");
    if (token.empty()) token = "Profile_1";

    std::string profile_xml = BuildProfileToken();

    // 替换 token 为请求中的值（简化处理：直接返回默认 Profile）
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLDocument tmp;
    if (tmp.Parse(profile_xml.c_str()) == tinyxml2::XML_SUCCESS) {
        tinyxml2::XMLElement* root = tmp.FirstChildElement();
        if (root) {
            tinyxml2::XMLNode* cloned = root->DeepClone(&doc);
            if (cloned) doc.InsertEndChild(cloned);
        }
    }

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::trt + std::string("/GetProfileResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// GetSnapshotUri：返回快照 URI（可选实现，暂不支持）
// =========================================================================
std::string MediaService::HandleGetSnapshotUri(const SoapRequest& req) {
    (void)req;
    return soap_helper_.BuildFaultResponse(
        "SOAP-ENV:Sender",
        "GetSnapshotUri not supported");
}

// =========================================================================
// 构造 Profile XML 片段（含 VideoSource + VideoEncoder 配置）
// =========================================================================
std::string MediaService::BuildProfileToken() const {
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* profile = doc.NewElement("trt:Profiles");
    profile->SetAttribute("token", "Profile_1");
    profile->SetAttribute("fixed", "true");
    doc.InsertEndChild(profile);

    // --- VideoSourceConfiguration ---
    {
        tinyxml2::XMLElement* vsc = doc.NewElement("tt:VideoSourceConfiguration");
        vsc->SetAttribute("token", "VideoSourceToken_1");
        profile->InsertEndChild(vsc);

        tinyxml2::XMLElement* src = doc.NewElement("tt:Source");
        src->SetAttribute("token", "VideoSource_1");
        vsc->InsertEndChild(src);

        tinyxml2::XMLElement* bounds = doc.NewElement("tt:Bounds");
        bounds->SetAttribute("width",  "1920");
        bounds->SetAttribute("height", "1080");
        bounds->SetAttribute("x", "0");
        bounds->SetAttribute("y", "0");
        vsc->InsertEndChild(bounds);
    }

    // --- VideoEncoderConfiguration ---
    {
        tinyxml2::XMLElement* vec = doc.NewElement("tt:VideoEncoderConfiguration");
        vec->SetAttribute("token", "VideoEncoderToken_1");
        profile->InsertEndChild(vec);

        tinyxml2::XMLElement* enc = doc.NewElement("tt:Encoding");
        enc->SetText("H.264");
        vec->InsertEndChild(enc);

        tinyxml2::XMLElement* res = doc.NewElement("tt:Resolution");
        {
            tinyxml2::XMLElement* w = doc.NewElement("tt:Width");
            w->SetText("1920");
            res->InsertEndChild(w);
            tinyxml2::XMLElement* h = doc.NewElement("tt:Height");
            h->SetText("1080");
            res->InsertEndChild(h);
        }
        vec->InsertEndChild(res);

        tinyxml2::XMLElement* qr = doc.NewElement("tt:Quality");
        qr->SetText("75");
        vec->InsertEndChild(qr);

        tinyxml2::XMLElement* fr = doc.NewElement("tt:RateControl");
        {
            tinyxml2::XMLElement* fps = doc.NewElement("tt:FrameRateLimit");
            fps->SetText("30");
            fr->InsertEndChild(fps);
            tinyxml2::XMLElement* bps = doc.NewElement("tt:BitrateLimit");
            bps->SetText("4096000");  // 4 Mbps
            fr->InsertEndChild(bps);
        }
        vec->InsertEndChild(fr);

        tinyxml2::XMLElement* h264 = doc.NewElement("tt:H264");
        {
            tinyxml2::XMLElement* prof = doc.NewElement("tt:Profile");
            prof->SetText("Main");
            h264->InsertEndChild(prof);
        }
        vec->InsertEndChild(h264);
    }

    // --- AudioEncoderConfiguration（可选，暂不添加）---

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

// =========================================================================
// 构造 VideoSourceConfiguration XML 片段
// =========================================================================
std::string MediaService::BuildVideoSourceConfiguration() const {
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* cfg = doc.NewElement("tt:VideoSourceConfiguration");
    cfg->SetAttribute("token", "VideoSourceToken_1");
    doc.InsertEndChild(cfg);

    tinyxml2::XMLElement* src = doc.NewElement("tt:Source");
    src->SetAttribute("token", "VideoSource_1");
    cfg->InsertEndChild(src);

    tinyxml2::XMLElement* bounds = doc.NewElement("tt:Bounds");
    bounds->SetAttribute("width",  "1920");
    bounds->SetAttribute("height", "1080");
    bounds->SetAttribute("x", "0");
    bounds->SetAttribute("y", "0");
    cfg->InsertEndChild(bounds);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

// =========================================================================
// 构造 VideoEncoderConfiguration XML 片段
// =========================================================================
std::string MediaService::BuildVideoEncoderConfiguration() const {
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* cfg = doc.NewElement("tt:VideoEncoderConfiguration");
    cfg->SetAttribute("token", "VideoEncoderToken_1");
    doc.InsertEndChild(cfg);

    tinyxml2::XMLElement* enc = doc.NewElement("tt:Encoding");
    enc->SetText("H.264");
    cfg->InsertEndChild(enc);

    tinyxml2::XMLElement* res = doc.NewElement("tt:Resolution");
    {
        tinyxml2::XMLElement* w = doc.NewElement("tt:Width");
        w->SetText("1920");
        res->InsertEndChild(w);
        tinyxml2::XMLElement* h = doc.NewElement("tt:Height");
        h->SetText("1080");
        res->InsertEndChild(h);
    }
    cfg->InsertEndChild(res);

    tinyxml2::XMLElement* qr = doc.NewElement("tt:Quality");
    qr->SetText("75");
    cfg->InsertEndChild(qr);

    tinyxml2::XMLElement* fr = doc.NewElement("tt:RateControl");
    {
        tinyxml2::XMLElement* fps = doc.NewElement("tt:FrameRateLimit");
        fps->SetText("30");
        fr->InsertEndChild(fps);
        tinyxml2::XMLElement* bps = doc.NewElement("tt:BitrateLimit");
        bps->SetText("4096000");
        fr->InsertEndChild(bps);
    }
    cfg->InsertEndChild(fr);

    tinyxml2::XMLElement* h264 = doc.NewElement("tt:H264");
    {
        tinyxml2::XMLElement* prof = doc.NewElement("tt:Profile");
        prof->SetText("Main");
        h264->InsertEndChild(prof);
    }
    cfg->InsertEndChild(h264);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

// =========================================================================
// 构造 AudioSourceConfiguration XML 片段（暂返回空）
// =========================================================================
std::string MediaService::BuildAudioSourceConfiguration() const {
    return "";
}

// =========================================================================
// 构造 AudioEncoderConfiguration XML 片段（暂返回空）
// =========================================================================
std::string MediaService::BuildAudioEncoderConfiguration() const {
    return "";
}

} // namespace onvif
