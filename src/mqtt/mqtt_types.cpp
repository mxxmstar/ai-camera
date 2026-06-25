#include "mqtt/mqtt_types.h"

namespace mqtt {

// ============================================================
// Topic 构建工具实现
// ============================================================

std::string Topics::Status(const std::string& device_id) {
    return "aicamera/" + device_id + "/status";
}

std::string Topics::PropertyPost(const std::string& device_id) {
    return "aicamera/" + device_id + "/property/post";
}

std::string Topics::EventPost(const std::string& device_id) {
    return "aicamera/" + device_id + "/event/post";
}

std::string Topics::CommandDown(const std::string& device_id) {
    return "aicamera/" + device_id + "/command/down";
}

std::string Topics::CommandResp(const std::string& device_id) {
    return "aicamera/" + device_id + "/command/resp";
}

std::string Topics::OtaNotify(const std::string& device_id) {
    return "aicamera/" + device_id + "/ota/notify";
}

std::string Topics::OtaProgress(const std::string& device_id) {
    return "aicamera/" + device_id + "/ota/progress";
}

} // namespace mqtt
