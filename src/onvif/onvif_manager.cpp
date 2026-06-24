#include "onvif/onvif_manager.h"
#include "onvif/device_service.h"
#include "onvif/media_service.h"
#include "onvif/events_service.h"
#include "onvif/wsdiscovery.h"
#include "server/http/router.hpp"

#include <iostream>

namespace onvif {

// =========================================================================
// 单例实现
// =========================================================================
OnvifManager& OnvifManager::Instance() {
    static OnvifManager instance;
    return instance;
}

// =========================================================================
// 构造 / 析构
// =========================================================================
OnvifManager::OnvifManager()
    : config_({
          "127.0.0.1",  // device_ip
          8080,          // http_port
          8554,          // rtsp_port
          "live"          // rtsp_path
      }),
      dev_info_({
          "ai-camera",    // manufacturer
          "AI-Cam-1.0", // model
          "1.0",          // firmware
          "123456",       // serial
          "AICAM-HW-001" // hardware_id
      })
{
}

OnvifManager::~OnvifManager() {
    Stop();
}

// =========================================================================
// 配置
// =========================================================================
void OnvifManager::SetConfig(const onvif::ServiceConfig& config) {
    config_ = config;
}

void OnvifManager::SetDeviceInfo(const onvif::DeviceInfo& dev_info) {
    dev_info_ = dev_info;
}

// =========================================================================
// 注册 ONVIF SOAP 路由到 HTTP Server 的 Router
// =========================================================================
void OnvifManager::RegisterRoutes(http::Router& router) {
    std::cout << "[ONVIF] Registering SOAP routes..." << std::endl;

    // POST /onvif/device_service → Device Service
    router.post("/onvif/device_service",
        [this](const http::Request& req) -> http::Response {
            std::string soap_resp = HandleDeviceService(req.body);
            return http::Response()
                .status(http::StatusCode::OK)
                .header("Content-Type", "application/soap+xml; charset=utf-8")
                .body(soap_resp);
        });

    // POST /onvif/media_service → Media Service
    router.post("/onvif/media_service",
        [this](const http::Request& req) -> http::Response {
            std::string soap_resp = HandleMediaService(req.body);
            return http::Response()
                .status(http::StatusCode::OK)
                .header("Content-Type", "application/soap+xml; charset=utf-8")
                .body(soap_resp);
        });

    // POST /onvif/events_service → Events Service
    router.post("/onvif/events_service",
        [this](const http::Request& req) -> http::Response {
            std::string soap_resp = HandleEventsService(req.body);
            return http::Response()
                .status(http::StatusCode::OK)
                .header("Content-Type", "application/soap+xml; charset=utf-8")
                .body(soap_resp);
        });

    std::cout << "[ONVIF] Routes registered: "
              << "/onvif/device_service, "
              << "/onvif/media_service, "
              << "/onvif/events_service" << std::endl;
}

// =========================================================================
// 启动 / 停止 ONVIF 服务
// =========================================================================
bool OnvifManager::Start() {
    if (started_) {
        std::cout << "[ONVIF] Already started." << std::endl;
        return true;
    }

    // 创建子服务实例
    ws_discovery_   = std::make_unique<onvif::WSDiscovery>(config_);
    device_service_ = std::make_unique<onvif::DeviceService>(config_, dev_info_);
    media_service_  = std::make_unique<onvif::MediaService>(config_);
    events_service_ = std::make_unique<onvif::EventsService>(config_);

    // 配置 WS-Discovery
    ws_discovery_->SetTypes(onvif::DiscoveryScopes::NetworkVideoTransmitter);
    {
        std::vector<std::string> scopes = {
            onvif::DiscoveryScopes::Device,
            onvif::DiscoveryScopes::NetworkVideoTransmitter,
            "onvif://www.onvif.org/Profile/" + std::string(onvif::OnvifProfiles::ProfileS),
            "onvif://www.onvif.org/name/ai-camera",
        };
        ws_discovery_->SetScopes(scopes);
    }

    // 启动 WS-Discovery UDP 多播监听
    if (!ws_discovery_->Start()) {
        std::cerr << "[ONVIF] Failed to start WS-Discovery!" << std::endl;
        return false;
    }

    started_ = true;
    std::cout << "[ONVIF] Started. Device service URL: "
              << config_.DeviceServiceURL() << std::endl;
    return true;
}

void OnvifManager::Stop() {
    if (!started_) return;

    if (ws_discovery_) {
        ws_discovery_->Stop();
    }

    events_service_.reset();
    media_service_.reset();
    device_service_.reset();
    ws_discovery_.reset();

    started_ = false;
    std::cout << "[ONVIF] Stopped." << std::endl;
}

// =========================================================================
// SOAP 请求处理入口（注册到 Router 的回调）
// =========================================================================
std::string OnvifManager::HandleDeviceService(const std::string& soap_body) {
    if (!device_service_) {
        onvif::SoapHelper sh;
        return sh.BuildFaultResponse("SOAP-ENV:Server", "DeviceService not initialized");
    }
    return device_service_->HandleRequest(soap_body);
}

std::string OnvifManager::HandleMediaService(const std::string& soap_body) {
    if (!media_service_) {
        onvif::SoapHelper sh;
        return sh.BuildFaultResponse("SOAP-ENV:Server", "MediaService not initialized");
    }
    return media_service_->HandleRequest(soap_body);
}

std::string OnvifManager::HandleEventsService(const std::string& soap_body) {
    if (!events_service_) {
        onvif::SoapHelper sh;
        return sh.BuildFaultResponse("SOAP-ENV:Server", "EventsService not initialized");
    }
    return events_service_->HandleRequest(soap_body);
}

} // namespace onvif
