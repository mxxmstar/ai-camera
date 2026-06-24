#include "onvif/events_service.h"
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <sstream>

namespace onvif {

// =========================================================================
// 构造
// =========================================================================
EventsService::EventsService(const ServiceConfig& config)
    : config_(config) {}

EventsService::~EventsService() {
    // 通知所有等待中的 PullMessages 线程退出
    {
        std::lock_guard<std::mutex> lk(subscriptions_mtx_);
        for (auto& pair : subscriptions_) {
            if (pair.second) {
                pair.second->active = false;
                pair.second->cv.notify_all();
            }
        }
    }
}

// =========================================================================
// 主入口：解析 SOAP 请求并分发
// =========================================================================
std::string EventsService::HandleRequest(const std::string& soap_body) {
    SoapRequest req;
    if (!soap_helper_.ParseSoapRequest(soap_body, req)) {
        return soap_helper_.BuildFaultResponse(
            "SOAP-ENV:Sender", "Invalid SOAP request");
    }

    std::string response_xml;
    if      (req.operation == "GetEventProperties") {
        response_xml = HandleGetEventProperties(req);
    } else if (req.operation == "CreatePullPointSubscription") {
        response_xml = HandleCreatePullPointSubscription(req);
    } else if (req.operation == "PullMessages") {
        response_xml = HandlePullMessages(req);
    } else if (req.operation == "Unsubscribe") {
        response_xml = HandleUnsubscribe(req);
    } else if (req.operation == "Renew") {
        response_xml = HandleRenew(req);
    } else {
        response_xml = soap_helper_.BuildFaultResponse(
            "SOAP-ENV:Sender",
            "Unsupported events operation: " + req.operation);
    }
    return response_xml;
}

// =========================================================================
// 注入模拟事件（供外部调用，如 AI 检测到移动时调用）
// =========================================================================
void EventsService::PushEvent(const std::string& event_topic,
                              const std::string& event_source,
                              const std::string& event_key,
                              const std::string& event_value) {
    std::string event_xml = BuildEventMessage(event_topic, event_source,
                                            event_key, event_value);

    std::lock_guard<std::mutex> lk(subscriptions_mtx_);
    RemoveExpiredSubscriptions();

    for (auto& pair : subscriptions_) {
        if (!pair.second || !pair.second->active) continue;
        {
            std::lock_guard<std::mutex> lk2(pair.second->mtx);
            pair.second->pending_events.push_back(event_xml);
        }
        pair.second->cv.notify_one();
    }
}

// =========================================================================
// GetEventProperties：返回设备支持的事件属性
// ONVIF Events Service Spec, Section 3.2
// =========================================================================
std::string EventsService::HandleGetEventProperties(const SoapRequest& req) {
    (void)req;
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tev:GetEventPropertiesResponse");
    doc.InsertEndChild(resp);

    // 支持的事件主题（简化：仅 MotionAlarm）
    tinyxml2::XMLElement* topics = doc.NewElement("tev:TopicSet");
    resp->InsertEndChild(topics);

    tinyxml2::XMLElement* rule = doc.NewElement("tns1:RuleEngine");
    topics->InsertEndChild(rule);

    tinyxml2::XMLElement* cell = doc.NewElement("tns1:CellMotionDetector");
    cell->SetAttribute("TopicNamespace", "http://www.onvif.org/ver10/topics");
    cell->SetAttribute("Topic", "RuleEngine/CellMotionDetector/Motion");
    rule->InsertEndChild(cell);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tev + std::string("/GetEventPropertiesResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// CreatePullPointSubscription：创建拉取点订阅
// ONVIF Events Service Spec, Section 3.3
// =========================================================================
std::string EventsService::HandleCreatePullPointSubscription(const SoapRequest& req) {
    // 可选参数：InitialTerminationTime（订阅时长）
    int64_t termination_sec = 3600;  // 默认 1 小时

    if (req.doc) {
        tinyxml2::XMLElement* env  = req.doc->FirstChildElement("Envelope");
        if (!env) env = req.doc->FirstChildElement("SOAP-ENV:Envelope");
        if (env) {
            tinyxml2::XMLElement* body = env->FirstChildElement("Body");
            if (body) {
                tinyxml2::XMLElement* op = body->FirstChildElement("CreatePullPointSubscription");
                if (op) {
                    tinyxml2::XMLElement* tt = op->FirstChildElement("InitialTerminationTime");
                    if (tt && tt->GetText()) {
                        // 简化解析：支持 "PT60S" 格式（ISO 8601 Duration）
                        const char* tstr = tt->GetText();
                        // 简单解析：只处理 PT<秒>S 格式
                        const char* p = std::strstr(tstr, "PT");
                        if (p) {
                            p += 2;
                            char* endp = nullptr;
                            int64_t val = std::strtoll(p, &endp, 10);
                            if (endp && *endp == 'S') {
                                termination_sec = val;
                            } else if (endp && *endp == 'M') {
                                termination_sec = val * 60;
                            } else if (endp && *endp == 'H') {
                                termination_sec = val * 3600;
                            }
                        }
                    }
                }
            }
        }
    }

    std::string sub_id = GenerateSubscriptionId();
    std::string ref_addr = BuildPullPointReference(sub_id);

    {
        std::lock_guard<std::mutex> lk(subscriptions_mtx_);
        RemoveExpiredSubscriptions();
        auto sub = std::make_unique<PullPointSubscription>();
        sub->subscription_id  = sub_id;
        sub->reference_addr   = ref_addr;
        sub->create_time      = std::chrono::system_clock::now();
        sub->termination_time = std::chrono::seconds(termination_sec);
        sub->active           = true;
        subscriptions_[sub_id] = std::move(sub);
    }

    // 构造响应
    auto now_tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm term_tm;
#ifdef _WIN32
    gmtime_s(&term_tm, &now_tt);
#else
    gmtime_r(&now_tt, &term_tm);
#endif
    // 简化：终止时间 = 当前时间 + termination_sec
    now_tt += termination_sec;
#ifdef _WIN32
    gmtime_s(&term_tm, &now_tt);
#else
    gmtime_r(&now_tt, &term_tm);
#endif
    char term_buf[64];
    std::strftime(term_buf, sizeof(term_buf),
                  "%Y-%m-%dT%H:%M:%SZ", &term_tm);

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tev:CreatePullPointSubscriptionResponse");
    doc.InsertEndChild(resp);

    // SubscriptionReference
    tinyxml2::XMLElement* subref = doc.NewElement("tev:SubscriptionReference");
    resp->InsertEndChild(subref);

    tinyxml2::XMLElement* addr = doc.NewElement("wsa:Address");
    addr->SetText(ref_addr.c_str());
    subref->InsertEndChild(addr);

    // TerminationTime
    tinyxml2::XMLElement* tt = doc.NewElement("tev:TerminationTime");
    tt->SetText(term_buf);
    resp->InsertEndChild(tt);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tev + std::string("/CreatePullPointSubscriptionResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// PullMessages：拉取事件消息（支持超时等待）
// ONVIF Events Service Spec, Section 3.4
// =========================================================================
std::string EventsService::HandlePullMessages(const SoapRequest& req) {
    // 提取 SubscriptionReference（从 SOAP Header 的 wsa:To 或 Body 中）
    // 简化：使用请求的 wsa:MessageID 关联订阅
    std::string sub_id;
    if (req.doc) {
        tinyxml2::XMLElement* env = req.doc->FirstChildElement("Envelope");
        if (!env) env = req.doc->FirstChildElement("SOAP-ENV:Envelope");
        if (env) {
            tinyxml2::XMLElement* hdr = env->FirstChildElement("Header");
            if (!hdr) hdr = env->FirstChildElement("SOAP-ENV:Header");
            if (hdr) {
                tinyxml2::XMLElement* to = hdr->FirstChildElement("To");
                if (!to) to = hdr->FirstChildElement("wsa:To");
                if (to && to->GetText()) {
                    // 从 URL 中提取 subscription ID
                    const char* t = to->GetText();
                    const char* p = std::strrchr(t, '/');
                    if (p) sub_id = std::string(p + 1);
                }
            }
        }
    }

    // 提取 Timeout 和 MessageLimit
    int timeout_sec  = 10;   // 默认 10 秒
    int message_limit = 100;  // 默认 100 条

    if (req.doc) {
        tinyxml2::XMLElement* env = req.doc->FirstChildElement("Envelope");
        if (!env) env = req.doc->FirstChildElement("SOAP-ENV:Envelope");
        if (env) {
            tinyxml2::XMLElement* body = env->FirstChildElement("Body");
            if (body) {
                tinyxml2::XMLElement* op = body->FirstChildElement("PullMessages");
                if (op) {
                    tinyxml2::XMLElement* to = op->FirstChildElement("Timeout");
                    if (to && to->GetText()) {
                        const char* tstr = to->GetText();
                        const char* p = std::strstr(tstr, "PT");
                        if (p) {
                            p += 2;
                            char* endp = nullptr;
                            int64_t val = std::strtoll(p, &endp, 10);
                            if (endp && *endp == 'S') timeout_sec = static_cast<int>(val);
                        }
                    }
                    tinyxml2::XMLElement* ml = op->FirstChildElement("MessageLimit");
                    if (ml && ml->GetText()) {
                        message_limit = std::atoi(ml->GetText());
                    }
                }
            }
        }
    }

    if (sub_id.empty()) {
        // 查找第一个有效订阅
        std::lock_guard<std::mutex> lk(subscriptions_mtx_);
        for (const auto& pair : subscriptions_) {
            if (pair.second && pair.second->active) {
                sub_id = pair.first;
                break;
            }
        }
    }

    if (sub_id.empty()) {
        return soap_helper_.BuildFaultResponse(
            "SOAP-ENV:Sender", "No active subscription found");
    }

    PullPointSubscription* sub = FindSubscription(sub_id);
    if (!sub) {
        return soap_helper_.BuildFaultResponse(
            "SOAP-ENV:Sender", "Subscription not found: " + sub_id);
    }

    // 等待事件（带超时）
    std::vector<std::string> popped;
    {
        std::unique_lock<std::mutex> lk(sub->mtx);
        auto deadline = std::chrono::steady_clock::now() +
                       std::chrono::seconds(timeout_sec);

        // 等待直到有事件到达或超时
        sub->cv.wait_until(lk, deadline, [&]() {
            return !sub->pending_events.empty() || !sub->active;
        });

        // 弹出事件（最多 message_limit 条）
        while (!sub->pending_events.empty() && (int)popped.size() < message_limit) {
            popped.push_back(sub->pending_events.front());
            sub->pending_events.erase(sub->pending_events.begin());
        }
    }

    // 构造响应
    auto now_tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm now_tm;
#ifdef _WIN32
    gmtime_s(&now_tm, &now_tt);
#else
    gmtime_r(&now_tt, &now_tm);
#endif
    char ts_buf[64];
    std::strftime(ts_buf, sizeof(ts_buf),
                  "%Y-%m-%dT%H:%M:%SZ", &now_tm);

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* resp = doc.NewElement("tev:PullMessagesResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLElement* curr = doc.NewElement("tev:CurrentTime");
    curr->SetText(ts_buf);
    resp->InsertEndChild(curr);

    // TerminationTime（简化：使用订阅的终止时间）
    auto term_tt = std::chrono::system_clock::to_time_t(sub->create_time + sub->termination_time);
    std::tm term_tm;
#ifdef _WIN32
    gmtime_s(&term_tm, &term_tt);
#else
    gmtime_r(&term_tt, &term_tm);
#endif
    char term_buf[64];
    std::strftime(term_buf, sizeof(term_buf),
                  "%Y-%m-%dT%H:%M:%SZ", &term_tm);

    tinyxml2::XMLElement* tt = doc.NewElement("tev:TerminationTime");
    tt->SetText(term_buf);
    resp->InsertEndChild(tt);

    // NotificationMessage 列表
    for (const auto& event_xml : popped) {
        tinyxml2::XMLDocument tmp;
        if (tmp.Parse(event_xml.c_str()) == tinyxml2::XML_SUCCESS) {
            tinyxml2::XMLNode* root = tmp.FirstChild();
            if (root) {
                tinyxml2::XMLNode* cloned = root->DeepClone(&doc);
                if (cloned) resp->InsertEndChild(cloned);
            }
        }
    }

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tev + std::string("/PullMessagesResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// Unsubscribe：取消订阅
// =========================================================================
std::string EventsService::HandleUnsubscribe(const SoapRequest& req) {
    std::string sub_id;
    if (req.doc) {
        tinyxml2::XMLElement* env = req.doc->FirstChildElement("Envelope");
        if (!env) env = req.doc->FirstChildElement("SOAP-ENV:Envelope");
        if (env) {
            tinyxml2::XMLElement* body = env->FirstChildElement("Body");
            if (body) {
                tinyxml2::XMLElement* op = body->FirstChildElement("Unsubscribe");
                if (op) {
                    tinyxml2::XMLElement* ref = op->FirstChildElement("SubscriptionReference");
                    if (ref) {
                        tinyxml2::XMLElement* addr = ref->FirstChildElement("Address");
                        if (!addr) addr = ref->FirstChildElement("wsa:Address");
                        if (addr && addr->GetText()) {
                            const char* t = addr->GetText();
                            const char* p = std::strrchr(t, '/');
                            if (p) sub_id = std::string(p + 1);
                        }
                    }
                }
            }
        }
    }

    if (!sub_id.empty()) {
        std::lock_guard<std::mutex> lk(subscriptions_mtx_);
        auto it = subscriptions_.find(sub_id);
        if (it != subscriptions_.end() && it->second) {
            it->second->active = false;
            it->second->cv.notify_all();
            subscriptions_.erase(it);
        }
    }

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* resp = doc.NewElement("tev:UnsubscribeResponse");
    doc.InsertEndChild(resp);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);

    return soap_helper_.BuildSoapResponse(
        XmlNs::tev + std::string("/UnsubscribeResponse"),
        req.message_id,
        printer.CStr());
}

// =========================================================================
// Renew：续订订阅（可选实现）
// =========================================================================
std::string EventsService::HandleRenew(const SoapRequest& req) {
    (void)req;
    return soap_helper_.BuildFaultResponse(
        "SOAP-ENV:Sender", "Renew not yet implemented");
}

// =========================================================================
// 生成订阅 ID（UUID）
// =========================================================================
std::string EventsService::GenerateSubscriptionId() {
    return SoapHelper::GenerateUuid();
}

// =========================================================================
// 查找订阅
// =========================================================================
PullPointSubscription* EventsService::FindSubscription(const std::string& subscription_id) {
    std::lock_guard<std::mutex> lk(subscriptions_mtx_);
    auto it = subscriptions_.find(subscription_id);
    if (it != subscriptions_.end() && it->second && it->second->active) {
        return it->second.get();
    }
    return nullptr;
}

// =========================================================================
// 移除过期订阅
// =========================================================================
void EventsService::RemoveExpiredSubscriptions() {
    auto now = std::chrono::system_clock::now();
    for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ) {
        if (!it->second ||
            !it->second->active ||
            (now > it->second->create_time + it->second->termination_time)) {
            it = subscriptions_.erase(it);
        } else {
            ++it;
        }
    }
}

// =========================================================================
// 构造拉取点引用 URL
// =========================================================================
std::string EventsService::BuildPullPointReference(const std::string& sub_id) const {
    return config_.EventsServiceURL() + "/subscription/" + sub_id;
}

// =========================================================================
// 构造事件消息 XML 片段
// =========================================================================
std::string EventsService::BuildEventMessage(const std::string& topic,
                                             const std::string& source,
                                             const std::string& key,
                                             const std::string& value) const {
    auto now_tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm_now;
#ifdef _WIN32
    gmtime_s(&tm_now, &now_tt);
#else
    gmtime_r(&now_tt, &tm_now);
#endif
    char ts[64];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm_now);

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* msg = doc.NewElement("tev:NotificationMessage");
    doc.InsertEndChild(msg);

    // Topic
    tinyxml2::XMLElement* top = doc.NewElement("tev:Topic");
    top->SetAttribute("Dialect", "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet1");
    top->SetText(topic.c_str());
    msg->InsertEndChild(top);

    // Source
    if (!source.empty()) {
        tinyxml2::XMLElement* src = doc.NewElement("tev:Source");
        tinyxml2::XMLElement* s = doc.NewElement("tev:SourceItem");
        s->SetAttribute("Name", "VideoSource");
        s->SetAttribute("Value", source.c_str());
        src->InsertEndChild(s);
        msg->InsertEndChild(src);
    }

    // Data
    tinyxml2::XMLElement* data = doc.NewElement("tev:Data");
    msg->InsertEndChild(data);

    tinyxml2::XMLElement* item = doc.NewElement("tt:Message");
    data->InsertEndChild(item);

    tinyxml2::XMLElement* prop = doc.NewElement("tt:Property");
    item->InsertEndChild(prop);

    tinyxml2::XMLElement* k = doc.NewElement(key.c_str());
    k->SetText(value.c_str());
    prop->InsertEndChild(k);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

} // namespace onvif
