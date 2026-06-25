#include "mqtt/mqtt_manager.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace mqtt {

// ============================================================
// 单例实现
// ============================================================
MqttManager& MqttManager::Instance() {
    static MqttManager instance;
    return instance;
}

MqttManager::MqttManager()
    : started_(false)
{
}

MqttManager::~MqttManager() {
    Stop();
}

// ============================================================
// SetConfig：注入配置
// ============================================================
void MqttManager::SetConfig(const MqttConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    InitTopics();
}

// ============================================================
// Start：启动 MQTT 客户端
// ============================================================
bool MqttManager::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) return true;

    client_ = std::make_shared<MqttClient>();
    client_->SetConfig(config_);

    // 设置回调
    client_->SetOnConnect([this](bool connected) {
        if (connected) {
            std::cout << "[MQTT Manager] Connected, subscribed to command/OTA topics" << std::endl;
            // 自动订阅命令下发和 OTA 通知 Topic
            client_->Subscribe(topic_command_down_, QoS::AT_LEAST_ONCE);
            client_->Subscribe(topic_ota_notify_,  QoS::AT_LEAST_ONCE);

            // 发布在线状态
            PublishStatus(true);
        } else {
            std::cout << "[MQTT Manager] Disconnected" << std::endl;
        }
        if (on_connect_) on_connect_(connected);
    });

    client_->SetOnMessage([this](const std::string& topic,
                                const std::string& payload,
                                QoS,
                                bool) {
        OnMessageInternal(topic, payload);
    });

    client_->Connect();
    started_ = true;
    return true;
}

// ============================================================
// Stop：停止并断开连接
// ============================================================
void MqttManager::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) return;

    // 发布离线状态（发送 DISCONNECT 前）
    if (client_ && client_->IsConnected()) {
        PublishStatus(false);
        // 等待一小段时间确保消息发出
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (client_) {
        client_->Disconnect();
        client_.reset();
    }
    started_ = false;
}

bool MqttManager::IsConnected() const {
    return client_ && client_->IsConnected();
}

// ============================================================
// 业务发布 API
// ============================================================
void MqttManager::PublishProperty(const std::string& json_data) {
    if (!client_) return;
    std::string payload = JsonBuilder()
        .Add("device_id", config_.device_id)
        .Add("timestamp", static_cast<int64_t>(
               std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()
               ).count()))
        .AddRaw("data", json_data)
        .Build();

    client_->Publish(topic_property_post_, payload, QoS::AT_LEAST_ONCE);
}

void MqttManager::PublishEvent(const std::string& event_type,
                                 const std::string& json_data) {
    if (!client_) return;
    std::string payload = JsonBuilder()
        .Add("device_id", config_.device_id)
        .Add("timestamp", static_cast<int64_t>(
               std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()
               ).count()))
        .Add("event_type", event_type)
        .AddRaw("data", json_data)
        .Build();

    client_->Publish(topic_event_post_, payload, QoS::AT_LEAST_ONCE);
}

void MqttManager::PublishCommandResp(const std::string& msg_id,
                                      int code,
                                      const std::string& message) {
    if (!client_) return;
    std::string payload = JsonBuilder()
        .Add("device_id", config_.device_id)
        .Add("msg_id", msg_id)
        .Add("code", code)
        .Add("message", message)
        .Build();

    client_->Publish(topic_command_resp_, payload, QoS::AT_LEAST_ONCE);
}

void MqttManager::PublishOtaProgress(int percent, const std::string& status) {
    if (!client_) return;
    std::string payload = JsonBuilder()
        .Add("device_id", config_.device_id)
        .Add("timestamp", static_cast<int64_t>(
               std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()
               ).count()))
        .Add("percent", percent)
        .Add("status", status)
        .Build();

    client_->Publish(topic_ota_progress_, payload, QoS::AT_LEAST_ONCE);
}

void MqttManager::PublishStatus(bool online) {
    if (!client_) return;
    std::string payload = JsonBuilder()
        .Add("device_id", config_.device_id)
        .Add("online", online)
        .Add("timestamp", static_cast<int64_t>(
               std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()
               ).count()))
        .Build();

    client_->Publish(topic_status_, payload, QoS::AT_LEAST_ONCE, true);
}

// ============================================================
// 内部消息处理
// ============================================================
void MqttManager::OnMessageInternal(const std::string& topic,
                                     const std::string& payload) {
    std::cout << "[MQTT Manager] Received message: " << topic << " -> " << payload << std::endl;

    if (topic == topic_command_down_) {
        ParseCommand(payload);
    } else if (topic == topic_ota_notify_) {
        ParseOtaNotify(payload);
    } else {
        std::cout << "[MQTT Manager] Unknown topic: " << topic << std::endl;
    }
}

// ============================================================
// 解析命令消息（简单 JSON 解析，不依赖第三方库）
// ============================================================
void MqttManager::ParseCommand(const std::string& payload) {
    // 简单解析 JSON，提取 msg_id, cmd, params
    std::string msg_id;
    std::string cmd;
    std::string params = "{}";

    // 提取 JSON 字符串字段: "field":"value"
    auto ExtractString = [](const std::string& json,
                            const std::string& field) -> std::string {
        std::string key = "\"" + field + "\":\"";
        auto pos = json.find(key);
        if (pos == std::string::npos) return "";
        pos += key.size();  // 跳过 key，指向值的第一个字符
        auto end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    // 提取 JSON 对象字段: "field":{...}
    auto ExtractObject = [](const std::string& json,
                            const std::string& field) -> std::string {
        std::string key = "\"" + field + "\":";
        auto pos = json.find(key);
        if (pos == std::string::npos) return "";
        pos = json.find("{", pos);
        if (pos == std::string::npos) return "";
        int brace = 1;
        std::size_t start = pos;
        for (std::size_t i = pos + 1; i < json.size(); ++i) {
            if (json[i] == '{') ++brace;
            else if (json[i] == '}') {
                --brace;
                if (brace == 0) {
                    return json.substr(start, i - start + 1);
                }
            }
        }
        return "";
    };

    msg_id  = ExtractString(payload, "msg_id");
    cmd     = ExtractString(payload, "cmd");
    params  = ExtractObject(payload, "params");
    if (params.empty()) params = "{}";

    std::cout << "[MQTT Manager] Command: " << cmd
              << " (msg_id=" << msg_id << ")" << std::endl;

    if (on_command_) on_command_(msg_id, cmd, params);
}

// ============================================================
// 解析 OTA 通知（简单 JSON 解析）
// ============================================================
void MqttManager::ParseOtaNotify(const std::string& payload) {
    std::string version, url, md5;
    uint64_t size = 0;

    auto ExtractString = [](const std::string& json,
                            const std::string& field) -> std::string {
        std::string key = "\"" + field + "\":\"";
        auto pos = json.find(key);
        if (pos == std::string::npos) return "";
        pos += key.size();
        auto end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    auto ExtractNumber = [](const std::string& json,
                            const std::string& field) -> uint64_t {
        std::string key = "\"" + field + "\":";
        auto pos = json.find(key);
        if (pos == std::string::npos) return 0;
        pos += key.size();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
        uint64_t val = 0;
        while (pos < json.size() && std::isdigit(json[pos])) {
            val = val * 10 + (json[pos] - '0');
            ++pos;
        }
        return val;
    };

    version = ExtractString(payload, "version");
    url     = ExtractString(payload, "url");
    md5     = ExtractString(payload, "md5");
    size    = ExtractNumber(payload, "size");

    std::cout << "[MQTT Manager] OTA notify: version=" << version
              << ", url=" << url << ", size=" << size << std::endl;

    if (on_ota_) on_ota_(version, url, md5, size);
}

// ============================================================
// InitTopics：根据 device_id 构建 Topic 列表
// ============================================================
void MqttManager::InitTopics() {
    topic_status_         = "aicamera/" + config_.device_id + "/status";
    topic_property_post_  = "aicamera/" + config_.device_id + "/property/post";
    topic_event_post_     = "aicamera/" + config_.device_id + "/event/post";
    topic_command_down_   = "aicamera/" + config_.device_id + "/command/down";
    topic_command_resp_   = "aicamera/" + config_.device_id + "/command/resp";
    topic_ota_notify_     = "aicamera/" + config_.device_id + "/ota/notify";
    topic_ota_progress_   = "aicamera/" + config_.device_id + "/ota/progress";
}

} // namespace mqtt
