#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "onvif_types.h"
#include "soap_helper.h"

namespace onvif {

// ============================================================================
// 拉取点订阅记录
// ============================================================================
struct PullPointSubscription {
    std::string subscription_id;          // 订阅引用 ID
    std::string reference_addr;           // 订阅引用地址（URL）
    std::chrono::system_clock::time_point create_time;
    std::chrono::seconds            termination_time = std::chrono::seconds(3600); // 默认60分钟

    // 事件队列
    std::vector<std::string> pending_events;  // 待推送事件 XML 片段
    std::mutex               mtx;
    std::condition_variable  cv;
    bool                    active = true;

    // 不可拷贝（含 std::mutex）
    PullPointSubscription() = default;
    PullPointSubscription(const PullPointSubscription&) = delete;
    PullPointSubscription& operator=(const PullPointSubscription&) = delete;
};

// ============================================================================
// Events Service：处理 ONVIF 事件相关 SOAP 操作
// 对应 tev: 命名空间 (http://www.onvif.org/ver10/events/wsdl)
// ============================================================================
class EventsService {
public:
    explicit EventsService(const ServiceConfig& config);
    ~EventsService();

    // ------------------------------------------------------------------
    // 主入口：根据 SOAP 操作名分发到对应处理函数
    // 返回 SOAP 响应体（完整的 XML 字符串，含 SOAP Envelope）
    // ------------------------------------------------------------------
    std::string HandleRequest(const std::string& soap_body);

    // ------------------------------------------------------------------
    // 注入模拟事件（供外部调用，如 AI 检测到移动时调用）
    // ------------------------------------------------------------------
    void PushEvent(const std::string& event_topic,
                  const std::string& event_source,
                  const std::string& event_key,
                  const std::string& event_value);

private:
    // === ONVIF Events 标准操作 ===

    // GetEventProperties：返回设备支持的事件属性
    std::string HandleGetEventProperties(const SoapRequest& req);
    // CreatePullPointSubscription：创建拉取点订阅
    std::string HandleCreatePullPointSubscription(const SoapRequest& req);
    // PullMessages：拉取事件消息（支持超时等待）
    std::string HandlePullMessages(const SoapRequest& req);
    // Unsubscribe：取消订阅
    std::string HandleUnsubscribe(const SoapRequest& req);
    // Renew：续订订阅（可选）
    std::string HandleRenew(const SoapRequest& req);

    // ------------------------------------------------------------------
    // 内部辅助
    // ------------------------------------------------------------------
    std::string GenerateSubscriptionId();
    PullPointSubscription* FindSubscription(const std::string& subscription_id);
    void RemoveExpiredSubscriptions();
    std::string BuildPullPointReference(const std::string& sub_id) const;
    std::string BuildEventMessage(const std::string& topic,
                                  const std::string& source,
                                  const std::string& key,
                                  const std::string& value) const;

    const ServiceConfig&                               config_;
    SoapHelper                                      soap_helper_;
    std::map<std::string, std::unique_ptr<PullPointSubscription>> subscriptions_;  // id -> subscription
    std::mutex                                      subscriptions_mtx_;
};

} // namespace onvif
