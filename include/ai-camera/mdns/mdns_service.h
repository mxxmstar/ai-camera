/**
 * @file mdns_service.h
 * @brief mDNS 服务主接口
 * 
 * 功能：
 *   - 服务注册（Service Registration）
 *   - 服务发现（Service Discovery）
 *   - 服务解析（Service Resolution）
 *   - 支持多种服务类型（RTSP、HTTP、MQTT 等）
 * 
 * 线程模型：
 *   - 所有 API 可跨线程安全调用
 *   - 回调在内部 io_context 线程调用
 * 
 * 用法示例：
 *   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 *   // 1. 获取单例
 *   auto mdns = mdns::MdnsService::Instance();
 *   
 *   // 2. 初始化
 *   mdns->Init("ai-camera");
 *   
 *   // 3. 注册服务
 *   mdns::ServiceInfo service;
 *   service.name = "AI-Camera-001";
 *   service.type = mdns::ServiceType::RTSP;
 *   service.port = 554;
 *   service.txt_records = {{"path", "/live/0"}};
 *   mdns->RegisterService(service);
 *   
 *   // 4. 发现服务
 *   mdns->StartBrowse(mdns::ServiceType::RTSP,
 *       [](const mdns::ServiceInfo& svc) {
 *           std::cout << "Found: " << svc.name << std::endl;
 *       },
 *       [](const std::string& name) {
 *           std::cout << "Lost: " << name << std::endl;
 *       }
 *   );
 *   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef AI_CAMERA_MDNS_MDNS_SERVICE_H
#define AI_CAMERA_MDNS_MDNS_SERVICE_H

#include "mdns_types.h"

#include <memory>
#include <mutex>
#include <vector>

namespace mdns {

// 前向声明内部实现类
class MdnsServiceImpl;

/// @brief mDNS 服务主类（单例模式）
/// 
/// 提供 mDNS 服务注册和发现功能。
/// 使用单例模式，整个进程只有一个实例。
class MdnsService : public std::enable_shared_from_this<MdnsService> {
public:
    /// @brief 获取全局单例实例
    /// @return 单例指针
    static std::shared_ptr<MdnsService> Instance();

    /// @brief 禁止拷贝和赋值
    MdnsService(const MdnsService&) = delete;
    MdnsService& operator=(const MdnsService&) = delete;

    /// @brief 析构函数
    ~MdnsService();

    // ========================================================================
    // 初始化与销毁
    // ========================================================================

    /// @brief 初始化 mDNS 模块
    /// 
    /// 初始化 mDNS 协议栈，设置主机名。
    /// 必须在调用其他 API 之前调用。
    /// 
    /// @param host_name 主机名（如 "ai-camera"），会追加 ".local." 后缀
    /// @param protocol  协议类型（IPv4/IPv6/Both）
    /// @return 成功返回 true
    /// 
    /// 示例：
    ///   auto mdns = MdnsService::Instance();
    ///   mdns->Init("ai-camera");
    bool Init(const std::string& host_name = "ai-camera",
              Protocol protocol = Protocol::IPv4);

    /// @brief 关闭 mDNS 模块
    /// 
    /// 释放所有资源，注销已注册的服务。
    /// 调用后需要重新 Init() 才能使用。
    void Shutdown();

    /// @brief 是否已初始化
    /// @return 已初始化返回 true
    bool IsInitialized() const;

    // ========================================================================
    // 服务注册（Service Registration）
    // ========================================================================

    /// @brief 注册服务（异步，立即返回）
    /// 
    /// 将服务注册到局域网，其他设备可以通过 mDNS 发现该服务。
    /// 
    /// @param service 服务信息
    /// @return 成功返回 true
    /// 
    /// 示例：
    ///   mdns::ServiceInfo service;
    ///   service.name = "AI-Camera-001";
    ///   service.type = mdns::ServiceType::RTSP;
    ///   service.port = 554;
    ///   service.txt_records = {
    ///       {"path", "/live/0"},
    ///       {"resolution", "1920x1080"}
    ///   };
    ///   mdns->RegisterService(service);
    bool RegisterService(const ServiceInfo& service);

    /// @brief 注销服务
    /// 
    /// 从局域网中移除服务注册。
    /// 
    /// @param service_name 服务实例名称
    /// @param type 服务类型
    /// @return 成功返回 true
    bool UnregisterService(const std::string& service_name, ServiceType type);

    /// @brief 更新服务的 TXT 记录
    /// 
    /// 动态更新服务的 TXT 记录（如状态信息）。
    /// 
    /// @param service_name 服务实例名称
    /// @param type 服务类型
    /// @param txt_records 新的 TXT 记录
    /// @return 成功返回 true
    bool UpdateServiceTxt(const std::string& service_name,
                         ServiceType type,
                         const std::map<std::string, std::string>& txt_records);

    // ========================================================================
    // 服务发现（Service Discovery）
    // ========================================================================

    /// @brief 开始浏览服务（异步，通过回调通知）
    /// 
    /// 开始监听局域网中的指定类型服务。
    /// 当发现新服务或已有服务消失时，通过回调通知。
    /// 
    /// @param type     要浏览的服务类型
    /// @param on_found 发现服务时的回调
    /// @param on_lost  服务消失时的回调
    /// @return 浏览会话 ID（用于停止浏览，失败返回 -1）
    /// 
    /// 示例：
    ///   int id = mdns->StartBrowse(
    ///       mdns::ServiceType::RTSP,
    ///       [](const mdns::ServiceInfo& svc) {
    ///           LOG_INFO("Found: {}", svc.name);
    ///       },
    ///       [](const std::string& name) {
    ///           LOG_INFO("Lost: {}", name);
    ///       }
    ///   );
    int StartBrowse(ServiceType type,
                     ServiceFoundHandler on_found,
                     ServiceLostHandler  on_lost);

    /// @brief 停止浏览
    /// 
    /// 停止监听指定类型的服务。
    /// 
    /// @param browse_id 浏览会话 ID（StartBrowse 的返回值）
    void StopBrowse(int browse_id);

    // ========================================================================
    // 服务解析（Service Resolution）
    // ========================================================================

    /// @brief 解析服务（获取 IP 地址和端口）
    /// 
    /// 将服务名称解析为具体的 IP 地址和端口。
    /// 解析完成后通过回调通知。
    /// 
    /// @param service_name 服务实例名称
    /// @param type         服务类型
    /// @param handler      解析完成后的回调
    /// 
    /// 示例：
    ///   mdns->ResolveService("AI-Camera-001", mdns::ServiceType::RTSP,
    ///       [](const mdns::ServiceInfo& svc) {
    ///           LOG_INFO("Resolved: {} -> {}", svc.name, svc.ipv4);
    ///           // 现在可以连接 rtsp://<ipv4>:<port>/live/0
    ///       }
    ///   );
    void ResolveService(const std::string& service_name,
                       ServiceType type,
                       ServiceResolvedHandler handler);

    // ========================================================================
    // 查询接口
    // ========================================================================

    /// @brief 获取已注册的服务列表
    /// @return 已注册的服务信息列表
    std::vector<ServiceInfo> GetRegisteredServices() const;

    /// @brief 获取已发现的服务列表
    /// @param type 服务类型（可选，不指定则返回所有类型）
    /// @return 已发现的服务信息列表
    std::vector<ServiceInfo> GetDiscoveredServices(ServiceType type = ServiceType::Custom) const;

    /// @brief 获取主机名
    /// @return 主机名（如 "ai-camera.local."）
    std::string GetHostName() const;

private:
    /// @brief 私有构造函数（单例模式）
    MdnsService();

    /// @brief 内部实现类（Pimpl 模式）
    std::unique_ptr<MdnsServiceImpl> impl_;

    /// @brief 互斥锁（保护 impl_）
    mutable std::mutex mutex_;
};

} // namespace mdns

#endif // AI_CAMERA_MDNS_MDNS_SERVICE_H
