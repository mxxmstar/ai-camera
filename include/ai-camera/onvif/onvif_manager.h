#pragma once

#include <memory>
#include <string>
#include "onvif_types.h"

// 前置声明
namespace http {
    class Router;
}

namespace onvif {

    class WSDiscovery;
    class DeviceService;
    class MediaService;
    class EventsService;

// ============================================================================
// OnvifManager：ONVIF 模块主管理器（单例模式，与 RtspManager 一致）
// ============================================================================
class OnvifManager {
public:
    static OnvifManager& Instance();

    ~OnvifManager();

    // =================================================================
    // 配置
    // =================================================================
    void SetConfig(const ServiceConfig& config);
    void SetDeviceInfo(const DeviceInfo& dev_info);

    // =================================================================
    // 注册 ONVIF SOAP 路由到 HTTP Server 的 Router
    // 在 main.cpp 中调用：
    //   OnvifManager::Instance().RegisterRoutes(router);
    // =================================================================
    void RegisterRoutes(http::Router& router);

    // =================================================================
    // 启动/停止 ONVIF 服务
    // Start() 启动 WS-Discovery UDP 多播监听
    // =================================================================
    bool Start();
    void Stop();

    // =================================================================
    // 访问子服务（供外部注入事件等）
    // =================================================================
    EventsService* GetEventsService() const { return events_service_.get(); }

private:
    OnvifManager();

    OnvifManager(const OnvifManager&)            = delete;
    OnvifManager& operator=(const OnvifManager&) = delete;

    // -----------------------------------------------------------------
    // SOAP 请求处理入口（注册到 Router 的回调）
    // -----------------------------------------------------------------
    std::string HandleDeviceService(const std::string& soap_body);
    std::string HandleMediaService(const std::string& soap_body);
    std::string HandleEventsService(const std::string& soap_body);

    ServiceConfig              config_;
    DeviceInfo                dev_info_;

    std::unique_ptr<WSDiscovery>    ws_discovery_;
    std::unique_ptr<DeviceService>  device_service_;
    std::unique_ptr<MediaService>   media_service_;
    std::unique_ptr<EventsService>  events_service_;

    bool                               started_ = false;
};

} // namespace onvif
