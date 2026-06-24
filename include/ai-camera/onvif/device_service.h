#pragma once

#include <string>
#include "onvif_types.h"
#include "soap_helper.h"

namespace onvif {

// ============================================================================
// Device Service：处理 ONVIF 设备管理 SOAP 操作
// 对应 tds: 命名空间 (http://www.onvif.org/ver10/device/wsdl)
// ============================================================================
class DeviceService {
public:
    explicit DeviceService(const ServiceConfig& config,
                          const DeviceInfo&   dev_info)
        : config_(config), dev_info_(dev_info) {}

    ~DeviceService() = default;

    // ------------------------------------------------------------------
    // 主入口：根据 SOAP 操作名分发到对应处理函数
    // 返回 SOAP 响应体（完整的 XML 字符串，含 SOAP Envelope）
    // ------------------------------------------------------------------
    std::string HandleRequest(const std::string& soap_body);

private:
    // === ONVIF Device 标准操作 ===

    // GetCapabilities：返回设备能力描述（媒体/事件/设备等服务端点）
    std::string HandleGetCapabilities(const SoapRequest& req);
    // GetDeviceInformation：返回设备制造商/型号/固件版本等
    std::string HandleGetDeviceInformation(const SoapRequest& req);
    // GetSystemDateAndTime：返回设备系统时间（UTC + Local）
    std::string HandleGetSystemDateAndTime(const SoapRequest& req);
    // GetServices：返回各服务的端点 URL 和能力
    std::string HandleGetServices(const SoapRequest& req);
    // GetScopes：返回设备 Scopes（类型/名称等）
    std::string HandleGetScopes(const SoapRequest& req);
    // GetHostname：返回设备主机名配置
    std::string HandleGetHostname(const SoapRequest& req);

    // ------------------------------------------------------------------
    // 构造能力响应 XML 片段（插入 SOAP Body）
    // ------------------------------------------------------------------
    std::string BuildCapabilitiesResponse(const std::string& token) const;

    const ServiceConfig& config_;
    const DeviceInfo&   dev_info_;
    SoapHelper          soap_helper_;
};

} // namespace onvif
