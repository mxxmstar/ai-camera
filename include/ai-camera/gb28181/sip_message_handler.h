#pragma once

#include <string>
#include <vector>
#include "gb28181_config.h"
#include "gb28181_types.h"

// tinyxml2 XML 解析库
#include <tinyxml2.h>

namespace gb28181 {

// ============================================================================
// SipMessageHandler：GB28181 XML 报文构造与解析
// ============================================================================
class SipMessageHandler {
public:
    // 构造 Keepalive MESSAGE XML
    static std::string BuildKeepaliveXml(const Gb28181Config& config, int sn);

    // 构造 Catalog 响应 XML
    static std::string BuildCatalogResponseXml(const Gb28181Config& config,
                                               const std::vector<Gb28181Channel>& channels,
                                               int sn);

    // 构造 DeviceInfo 响应 XML
    static std::string BuildDeviceInfoResponseXml(const Gb28181Config& config,
                                                  const Gb28181DeviceInfo& dev_info,
                                                  int sn);

    // 构造 DeviceStatus 响应 XML
    static std::string BuildDeviceStatusResponseXml(const Gb28181Config& config, int sn);

    // 构造 Alarm MESSAGE XML
    static std::string BuildAlarmXml(const Gb28181Config& config, const Gb28181Alarm& alarm);

    // 解析 MESSAGE CmdType
    static std::string ParseMessageCmdType(const std::string& xml_body);

    // 解析 MESSAGE SN
    static int ParseMessageSN(const std::string& xml_body);
};

} // namespace gb28181
