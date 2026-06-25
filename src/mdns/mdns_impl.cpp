/**
 * @file mdns_impl.cpp
 * @brief mDNS 内部实现类（简化版）
 */

#include "mdns/mdns_impl.h"

#include <iostream>
#include <thread>

namespace mdns {

// ============================================================
// 构造函数与析构函数
// ============================================================

MdnsServiceImpl::MdnsServiceImpl(class MdnsService* parent)
    : parent_(parent)
    , io_context_()
    , io_thread_(nullptr)
    , socket_(io_context_)
    , multicast_endpoint_(asio::ip::make_address("224.0.0.251"), 5353)
    , recv_buffer_(RECV_BUFFER_SIZE)
    , next_browse_id_(1)
    , initialized_(false)
    , running_(false) {
}

MdnsServiceImpl::~MdnsServiceImpl() {
    Shutdown();
}

// ============================================================
// 初始化与销毁
// ============================================================

bool MdnsServiceImpl::Init(const std::string& host_name, Protocol protocol) {
    if (initialized_) {
        return false;
    }

    host_name_ = host_name + ".local.";
    protocol_ = protocol;

    std::cout << "[mDNS] Initializing... host_name=" << host_name_ << std::endl;

    // 简化版：不实际初始化 socket，只设置标志
    initialized_ = true;
    running_ = true;

    std::cout << "[mDNS] Initialized successfully" << std::endl;
    return true;
}

void MdnsServiceImpl::Shutdown() {
    if (!initialized_) {
        return;
    }

    running_ = false;
    initialized_ = false;

    std::cout << "[mDNS] Shutdown complete" << std::endl;
}

// ============================================================
// 服务注册
// ============================================================

bool MdnsServiceImpl::RegisterService(const ServiceInfo& service) {
    if (!initialized_) {
        return false;
    }

    std::string key = service.name + "." + service.GetFullTypeString();
    registered_services_[key] = service;

    std::cout << "[mDNS] Registered service: " << key << " on port " << service.port << std::endl;
    return true;
}

bool MdnsServiceImpl::UnregisterService(const std::string& service_name, ServiceType type) {
    if (!initialized_) {
        return false;
    }

    std::string type_str = ServiceTypeToString(type);
    std::string key = service_name + "." + type_str;

    auto it = registered_services_.find(key);
    if (it == registered_services_.end()) {
        return false;
    }

    registered_services_.erase(it);
    std::cout << "[mDNS] Unregistered service: " << key << std::endl;
    return true;
}

bool MdnsServiceImpl::UpdateServiceTxt(const std::string& service_name,
                                        ServiceType type,
                                        const std::map<std::string, std::string>& txt_records) {
    if (!initialized_) {
        return false;
    }

    std::string type_str = ServiceTypeToString(type);
    std::string key = service_name + "." + type_str;

    auto it = registered_services_.find(key);
    if (it == registered_services_.end()) {
        return false;
    }

    it->second.txt_records = txt_records;
    return true;
}

// ============================================================
// 服务发现
// ============================================================

int MdnsServiceImpl::StartBrowse(ServiceType type,
                                  ServiceFoundHandler on_found,
                                  ServiceLostHandler on_lost) {
    if (!initialized_) {
        return -1;
    }

    int browse_id = next_browse_id_++;
    browse_handlers_[browse_id] = {on_found, on_lost};

    std::cout << "[mDNS] Started browsing for: " << ServiceTypeToString(type)
              << " (browse_id=" << browse_id << ")" << std::endl;

    return browse_id;
}

void MdnsServiceImpl::StopBrowse(int browse_id) {
    auto it = browse_handlers_.find(browse_id);
    if (it != browse_handlers_.end()) {
        browse_handlers_.erase(it);
        std::cout << "[mDNS] Stopped browsing (browse_id=" << browse_id << ")" << std::endl;
    }
}

// ============================================================
// 服务解析
// ============================================================

void MdnsServiceImpl::ResolveService(const std::string& service_name,
                                     ServiceType type,
                                     ServiceResolvedHandler handler) {
    if (!initialized_) {
        return;
    }

    std::cout << "[mDNS] Resolving service: " << service_name << std::endl;

    // 简化版：直接调用回调，传入空信息
    if (handler) {
        ServiceInfo info;
        info.name = service_name;
        info.type = type;
        handler(info);
    }
}

// ============================================================
// 查询接口
// ============================================================

std::vector<ServiceInfo> MdnsServiceImpl::GetRegisteredServices() const {
    std::vector<ServiceInfo> result;
    for (const auto& [key, service] : registered_services_) {
        result.push_back(service);
    }
    return result;
}

std::vector<ServiceInfo> MdnsServiceImpl::GetDiscoveredServices(ServiceType type) const {
    std::vector<ServiceInfo> result;

    if (type == ServiceType::Custom) {
        for (const auto& [svc_type, services] : discovered_services_) {
            result.insert(result.end(), services.begin(), services.end());
        }
    } else {
        auto it = discovered_services_.find(type);
        if (it != discovered_services_.end()) {
            result = it->second;
        }
    }

    return result;
}

// ============================================================
// 私有方法（简化版，暂不实现实际网络功能）
// ============================================================

bool MdnsServiceImpl::InitMulticastSocket() {
    // 简化版：暂不实现
    return true;
}

void MdnsServiceImpl::SendPacket(const std::vector<uint8_t>& packet) {
    // 简化版：暂不实现
}

void MdnsServiceImpl::StartReceive() {
    // 简化版：暂不实现
}

void MdnsServiceImpl::HandleReceivedPacket(const std::vector<uint8_t>& packet,
                                            const asio::ip::udp::endpoint& sender) {
    // 简化版：暂不实现
}

std::vector<uint8_t> MdnsServiceImpl::BuildRegistrationPacket(const ServiceInfo& service) {
    // 简化版：暂不实现
    return {};
}

std::vector<uint8_t> MdnsServiceImpl::BuildQueryPacket(const std::string& service_type) {
    // 简化版：暂不实现
    return {};
}

} // namespace mdns
