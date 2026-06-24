#include "gb28181/sip_message_handler.h"
#include <sstream>

namespace gb28181 {

std::string SipMessageHandler::BuildKeepaliveXml(const Gb28181Config& config, int sn) {
    // TODO: 构造 Keepalive XML
    return "";
}

std::string SipMessageHandler::BuildCatalogResponseXml(const Gb28181Config& config,
                                                       const std::vector<Gb28181Channel>& channels,
                                                       int sn) {
    // TODO: 构造 Catalog 响应 XML
    return "";
}

std::string SipMessageHandler::BuildDeviceInfoResponseXml(const Gb28181Config& config,
                                                          const Gb28181DeviceInfo& dev_info,
                                                          int sn) {
    // TODO: 构造 DeviceInfo 响应 XML
    return "";
}

std::string SipMessageHandler::BuildDeviceStatusResponseXml(const Gb28181Config& config, int sn) {
    // TODO: 构造 DeviceStatus 响应 XML
    return "";
}

std::string SipMessageHandler::BuildAlarmXml(const Gb28181Config& config, const Gb28181Alarm& alarm) {
    // TODO: 构造 Alarm XML
    return "";
}

std::string SipMessageHandler::ParseMessageCmdType(const std::string& xml_body) {
    // TODO: 解析 CmdType
    return "";
}

int SipMessageHandler::ParseMessageSN(const std::string& xml_body) {
    // TODO: 解析 SN
    return 0;
}

} // namespace gb28181
