#ifndef AI_CAMERA_MQTT_MQTT_MANAGER_H
#define AI_CAMERA_MQTT_MQTT_MANAGER_H

#include "mqtt_client.h"
#include "mqtt_json.h"
#include "mqtt_types.h"

#include <memory>
#include <mutex>
#include <string>

namespace mqtt {

/// @brief MQTT 模块单例管理器
///
/// 职责：
///   - 配置注入与生命周期管理
///   - 业务 API 封装（属性上报/事件推送/命令响应/OTA 进度）
///   - 消息路由（根据 Topic 分发到对应回调）
///   - 自动重连与状态管理
///
/// 用法示例：
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
///   // 初始化（在 main.cpp 中）
///   MqttManager::Instance().SetConfig({
///       "192.168.1.100", 1883,
///       "AICAM-001", "user", "pass"
///   });
///   MqttManager::Instance().SetOnCommand([](...) { ... });
///   MqttManager::Instance().Start();
///
///   // 上报属性
///   MqttManager::Instance().PublishProperty(
///       JsonBuilder()
///           .Add("firmware_version", "v1.2.3")
///           .Add("ip", "192.168.1.200")
///           .Build()
///   );
///
///   // 上报 AI 事件
///   MqttManager::Instance().PublishEvent(
///       "person_detect",
///       JsonBuilder()
///           .Add("confidence", 0.95)
///           .Add("bbox", "[100,200,300,400]")
///           .Build()
///   );
///
///   // 停止（在 signal_handler 中）
///   MqttManager::Instance().Stop();
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class MqttManager {
public:
    static MqttManager& Instance();

    ~MqttManager();

    /// 设置连接配置（Start() 前调用）
    void SetConfig(const MqttConfig& config);

    /// 启动 MQTT 客户端（异步，立即返回）
    /// @return 是否成功启动连接流程
    bool Start();

    /// 停止并断开连接（发送 DISCONNECT，避免触发 LWT）
    void Stop();

    /// 是否已连接
    bool IsConnected() const;

    // ============================================================
    // 业务发布 API
    // ============================================================

    /// 上报设备属性（QoS 1）
    void PublishProperty(const std::string& json_data);

    /// 上报 AI 事件/告警（QoS 1）
    /// @param event_type 事件类型（如 "person_detect", "face_recognize", "intrusion"）
    void PublishEvent(const std::string& event_type,
                      const std::string& json_data);

    /// 上报命令执行响应（QoS 1）
    void PublishCommandResp(const std::string& msg_id,
                            int code,
                            const std::string& message);

    /// 上报 OTA 升级进度（QoS 1）
    /// @param percent 进度 0-100
    /// @param status  状态描述（如 "downloading", "installing", "done", "error"）
    void PublishOtaProgress(int percent, const std::string& status);

    /// 发布在线状态（由内部自动管理，也可手动调用）
    void PublishStatus(bool online);

    // ============================================================
    // 回调注册
    // ============================================================

    void SetOnCommand(CommandHandler h)  { on_command_  = std::move(h); }
    void SetOnOtaNotify(OtaHandler h)    { on_ota_     = std::move(h); }
    void SetOnConnect(ConnectHandler h)   { on_connect_ = std::move(h); }

private:
    MqttManager();
    MqttManager(const MqttManager&)            = delete;
    MqttManager& operator=(const MqttManager&) = delete;

    /// 初始化 Topic 列表（根据 device_id 构建）
    void InitTopics();

    /// 内部消息处理（由 MqttClient 的 OnMessage 触发）
    void OnMessageInternal(const std::string& topic,
                           const std::string& payload);

    /// 解析命令消息（简单 JSON 解析）
    void ParseCommand(const std::string& payload);

    /// 解析 OTA 通知（简单 JSON 解析）
    void ParseOtaNotify(const std::string& payload);

    MqttConfig                  config_;
    std::shared_ptr<MqttClient> client_;
    bool                        started_ = false;

    // Topic 缓存
    std::string                 topic_status_;
    std::string                 topic_property_post_;
    std::string                 topic_event_post_;
    std::string                 topic_command_down_;
    std::string                 topic_command_resp_;
    std::string                 topic_ota_notify_;
    std::string                 topic_ota_progress_;

    // 回调
    CommandHandler              on_command_;
    OtaHandler                  on_ota_;
    ConnectHandler              on_connect_;
    std::mutex                  mutex_;
};

} // namespace mqtt

#endif // AI_CAMERA_MQTT_MQTT_MANAGER_H
