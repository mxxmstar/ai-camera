#pragma once

#include <string>
#include <vector>

namespace onvif {

// ============================================================================
// ONVIF XML 命名空间常量
// ============================================================================
struct XmlNs {
    // ONVIF 核心命名空间
    static constexpr const char* tds  = "http://www.onvif.org/ver10/device/wsdl";   // Device
    static constexpr const char* trt  = "http://www.onvif.org/ver10/media/wsdl";     // Media
    static constexpr const char* tev  = "http://www.onvif.org/ver10/events/wsdl";   // Events
    static constexpr const char* timg = "http://www.onvif.org/ver20/imaging/wsdl";   // Imaging
    static constexpr const char* tptz = "http://www.onvif.org/ver20/ptz/wsdl";    // PTZ
    static constexpr const char* tdn  = "http://www.onvif.org/ver10/deviceIO/wsdl"; // DeviceIO
    static constexpr const char* tls  = "http://www.onvif.org/ver10/display/wsdl";  // Display
    static constexpr const char* trc  = "http://www.onvif.org/ver10/recording/wsdl";
    static constexpr const char* tse  = "http://www.onvif.org/ver10/search/wsdl";
    static constexpr const char* trp  = "http://www.onvif.org/ver10/replay/wsdl";
    static constexpr const char* tav  = "http://www.onvif.org/ver10/receiver/wsdl";
    static constexpr const char* tpl  = "http://www.onvif.org/ver10/pacs";

    // 模式/类型命名空间
    static constexpr const char* tts  = "http://www.onvif.org/ver10/schema";        // Types/Schema
    static constexpr const char* tdg  = "http://www.onvif.org/ver10/deviceIO/xsd";

    // WS-Addressing
    static constexpr const char* wsa  = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    // WS-Discovery
    static constexpr const char* wsdd = "http://schemas.xmlsoap.org/ws/2005/04/discovery";
    // SOAP 1.2
    static constexpr const char* env  = "http://www.w3.org/2003/05/soap-envelope";
    // XML Schema
    static constexpr const char* xsd  = "http://www.w3.org/2001/XMLSchema";
    static constexpr const char* xsi  = "http://www.w3.org/2001/XMLSchema-instance";
    // WS-Security (暂未实现)
    static constexpr const char* wsse = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd";
};

// ============================================================================
// 设备信息（硬编码或可配置）
// ============================================================================
struct DeviceInfo {
    std::string manufacturer = "ai-camera";
    std::string model        = "AI-Cam-1.0";
    std::string firmware     = "1.0";
    std::string serial       = "123456";
    std::string hardware_id  = "AICAM-HW-001";
};

// ============================================================================
// 服务配置（设备 IP/端口/路径等）
// ============================================================================
struct ServiceConfig {
    std::string device_ip   = "127.0.0.1";
    uint16_t    http_port  = 8080;
    uint16_t    rtsp_port  = 8554;
    std::string rtsp_path  = "live";

    // 服务端点 URL（自动拼接）
    std::string DeviceServiceURL() const {
        return "http://" + device_ip + ":" + std::to_string(http_port) + "/onvif/device_service";
    }
    std::string MediaServiceURL() const {
        return "http://" + device_ip + ":" + std::to_string(http_port) + "/onvif/media_service";
    }
    std::string EventsServiceURL() const {
        return "http://" + device_ip + ":" + std::to_string(http_port) + "/onvif/events_service";
    }
    std::string StreamUri() const {
        return "rtsp://" + device_ip + ":" + std::to_string(rtsp_port) + "/" + rtsp_path;
    }
};

// ============================================================================
// WS-Discovery Scopes（设备类型标识）
// ============================================================================
struct DiscoveryScopes {
    // ONVIF 设备类型 URN
    static constexpr const char* NetworkVideoTransmitter =
        "onvif://www.onvif.org/type/video_encoder";
    static constexpr const char* Device =
        "onvif://www.onvif.org/type/device";
    static constexpr const char* MediaService =
        "onvif://www.onvif.org/type/media_service";

    // 名称空间
    static constexpr const char* NameSpace =
        "onvif://www.onvif.org/type";
};

// ============================================================================
// ONVIF Profile 常量（媒体配置文件）
// ============================================================================
struct OnvifProfiles {
    static constexpr const char* ProfileS  = "onvif://www.onvif.org/profile/s";  // Streaming
    static constexpr const char* ProfileC  = "onvif://www.onvif.org/profile/c";  // Control
    static constexpr const char* ProfileG  = "onvif://www.onvif.org/profile/g";  // Gaming/Analytics
    static constexpr const char* ProfileQ  = "onvif://www.onvif.org/profile/q";  // QoS
    static constexpr const char* ProfileA  = "onvif://www.onvif.org/profile/a";  // Advanced
};

} // namespace onvif
