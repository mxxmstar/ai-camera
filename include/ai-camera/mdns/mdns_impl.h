/**
 * @file mdns_impl.h
 * @brief mDNS 内部实现类
 * 
 * 注意：此文件为内部实现，不对外暴露
 */

#ifndef AI_CAMERA_MDNS_MDNS_IMPL_H
#define AI_CAMERA_MDNS_MDNS_IMPL_H

#include "mdns_types.h"

#include <asio.hpp>

#include <cstdint>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <functional>

namespace mdns {

// 前向声明
class MdnsService;

// ============================================================
// mDNS 实现类
// ============================================================

class MdnsServiceImpl {
public:
    /// @brief 构造函数
    explicit MdnsServiceImpl(MdnsService* parent);

    /// @brief 析构函数
    ~MdnsServiceImpl();

    /// @brief 初始化
    bool Init(const std::string& host_name, Protocol protocol);

    /// @brief 关闭
    void Shutdown();

    /// @brief 是否已初始化
    bool IsInitialized() const { return initialized_; }

    // 服务注册
    bool RegisterService(const ServiceInfo& service);
    bool UnregisterService(const std::string& service_name, ServiceType type);
    bool UpdateServiceTxt(const std::string& service_name,
                         ServiceType type,
                         const std::map<std::string, std::string>& txt_records);

    // 服务发现
    int StartBrowse(ServiceType type,
                     ServiceFoundHandler on_found,
                     ServiceLostHandler  on_lost);
    void StopBrowse(int browse_id);

    // 服务解析
    void ResolveService(const std::string& service_name,
                       ServiceType type,
                       ServiceResolvedHandler handler);

    // 查询接口
    std::vector<ServiceInfo> GetRegisteredServices() const;
    std::vector<ServiceInfo> GetDiscoveredServices(ServiceType type) const;
    std::string GetHostName() const { return host_name_; }

private:
    // 初始化多播 socket
    bool InitMulticastSocket();

    // 发送/接收数据
    void SendPacket(const std::vector<uint8_t>& packet);
    void StartReceive();
    void HandleReceivedPacket(const std::vector<uint8_t>& packet,
                             const asio::ip::udp::endpoint& sender);

    // 构建报文（简化版）
    std::vector<uint8_t> BuildRegistrationPacket(const ServiceInfo& service);
    std::vector<uint8_t> BuildQueryPacket(const std::string& service_type);

    // 成员变量
    class MdnsService* parent_;
    std::string   host_name_;
    Protocol      protocol_;

    // ASIO 相关
    asio::io_context io_context_;
    std::unique_ptr<std::thread> io_thread_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint multicast_endpoint_;

    // 接收缓冲
    std::vector<uint8_t> recv_buffer_;
    static constexpr std::size_t RECV_BUFFER_SIZE = 4096;

    // 服务管理
    std::map<std::string, ServiceInfo> registered_services_;
    std::map<ServiceType, std::vector<ServiceInfo>> discovered_services_;
    std::map<int, std::pair<ServiceFoundHandler, ServiceLostHandler>> browse_handlers_;
    int next_browse_id_;

    // 状态
    bool initialized_;
    bool running_;
};

} // namespace mdns

#endif // AI_CAMERA_MDNS_MDNS_IMPL_H
