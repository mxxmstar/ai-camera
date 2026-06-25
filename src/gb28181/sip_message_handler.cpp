#include "gb28181/sip_message_handler.h"
#include <sstream>

namespace gb28181 {

std::string SipMessageHandler::BuildKeepaliveXml(const Gb28181Config& config, int sn) {
    // TODO: Build Keepalive XML
    return "";
}

std::string SipMessageHandler::BuildCatalogResponseXml(const Gb28181Config& config,
                                                       const std::vector<Gb28181Channel>& channels,
                                                       int sn) {
    // TODO: Build Catalog response XML
    return "";
}

std::string SipMessageHandler::BuildDeviceInfoResponseXml(const Gb28181Config& config,
                                                          const Gb28181DeviceInfo& dev_info,
                                                          int sn) {
    // TODO: Build DeviceInfo response XML
    return "";
}

std::string SipMessageHandler::BuildDeviceStatusResponseXml(const Gb28181Config& config, int sn) {
    // TODO: Build DeviceStatus response XML
    return "";
}

std::string SipMessageHandler::BuildAlarmXml(const Gb28181Config& config, const Gb28181Alarm& alarm) {
    // TODO: Build Alarm XML
    return "";
}

std::string SipMessageHandler::ParseMessageCmdType(const std::string& xml_body) {
    // TODO: Parse CmdType
    return "";
}

int SipMessageHandler::ParseMessageSN(const std::string& xml_body) {
    // TODO: Parse SN
    return 0;
}

} // namespace gb28181
