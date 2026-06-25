/**
 * @file mdns_service.cpp
 * @brief mDNS 服务主类实现
 */

#include "mdns/mdns_service.h"
#include "mdns/mdns_impl.h"

#include <memory>
#include <mutex>
#include <iostream>

namespace mdns {

// ============================================================
// 单例实例
// ============================================================

static std::shared_ptr<MdnsService> g_instance = nullptr;
static std::mutex g_instance_mutex;

/// @brief 获取全局单例实例
std::shared_ptr<MdnsService> MdnsService::Instance() {
    std::lock_guard<std::mutex> lock(g_instance_mutex);
    if (!g_instance) {
        g_instance = std::shared_ptr<MdnsService>(new MdnsService());
    }
    return g_instance;
}

// ============================================================
// 构造与析构
// ============================================================

MdnsService::MdnsService() 
    : impl_(std::make_unique<MdnsServiceImpl>(this)) {
}

MdnsService::~MdnsService() {
    Shutdown();
}

// ============================================================
// 初始化与销毁
// ============================================================

bool MdnsService::Init(const std::string& host_name, Protocol protocol) {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->Init(host_name, protocol);
}

void MdnsService::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->Shutdown();
}

bool MdnsService::IsInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->IsInitialized();
}

// ============================================================
// 服务注册
// ============================================================

bool MdnsService::RegisterService(const ServiceInfo& service) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsInitialized()) {
        return false;
    }
    return impl_->RegisterService(service);
}

bool MdnsService::UnregisterService(const std::string& service_name, ServiceType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsInitialized()) {
        return false;
    }
    return impl_->UnregisterService(service_name, type);
}

bool MdnsService::UpdateServiceTxt(const std::string& service_name,
                                    ServiceType type,
                                    const std::map<std::string, std::string>& txt_records) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsInitialized()) {
        return false;
    }
    return impl_->UpdateServiceTxt(service_name, type, txt_records);
}

// ============================================================
// 服务发现
// ============================================================

int MdnsService::StartBrowse(ServiceType type,
                              ServiceFoundHandler on_found,
                              ServiceLostHandler on_lost) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsInitialized()) {
        return -1;
    }
    return impl_->StartBrowse(type, std::move(on_found), std::move(on_lost));
}

void MdnsService::StopBrowse(int browse_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsInitialized()) {
        return;
    }
    impl_->StopBrowse(browse_id);
}

// ============================================================
// 服务解析
// ============================================================

void MdnsService::ResolveService(const std::string& service_name,
                                 ServiceType type,
                                 ServiceResolvedHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsInitialized()) {
        return;
    }
    impl_->ResolveService(service_name, type, std::move(handler));
}

// ============================================================
// 查询接口
// ============================================================

std::vector<ServiceInfo> MdnsService::GetRegisteredServices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->GetRegisteredServices();
}

std::vector<ServiceInfo> MdnsService::GetDiscoveredServices(ServiceType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->GetDiscoveredServices(type);
}

std::string MdnsService::GetHostName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->GetHostName();
}

} // namespace mdns
