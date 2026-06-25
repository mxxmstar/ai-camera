#pragma once

#include <memory>
#include <mutex>
#include <string>
#include "gb28181_config.h"
#include "gb28181_types.h"
#include "sip_agent.h"

namespace gb28181 {

// ============================================================================
// Gb28181Manager：GB28181 模块全局单例管理器
//
// 职责：
// 1. 初始化/销毁 GB28181 模块（封装 SipAgent 的生命周期）
// 2. 响应平台发来的 SIP 请求（Catalog、DeviceInfo、Invite 等）
// 3. 向上层提供简单的接口（启动/停止、查询状态）
//
// 使用单例模式，整个进程只有一个实例。
// ============================================================================
class Gb28181Manager {
public:
    // 获取全局唯一实例
    static Gb28181Manager& Instance();

    // 禁止拷贝和赋值
    Gb28181Manager(const Gb28181Manager&) = delete;
    Gb28181Manager& operator=(const Gb28181Manager&) = delete;

    ~Gb28181Manager();

    // ========================================================================
    // 模块生命周期
    // ========================================================================

    // 初始化模块（读取配置，注册 SIP 回调）
    // 必须在调用 Start() 之前调用
    bool Init(const Gb28181Config& config);

    // 启动模块（开始 SIP 注册，监听平台请求）
    bool Start();

    // 停止模块（发送取消注册，释放资源）
    void Stop();

    // 是否已启动
    bool IsRunning() const;

    // ========================================================================
    // 平台交互接口
    // ========================================================================

    // 主动上报 Catalog（设备目录）
    bool ReportCatalog(const std::vector<Gb28181Channel>& channels);

    // 主动上报 DeviceInfo（设备信息）
    bool ReportDeviceInfo(const Gb28181DeviceInfo& info);

    // 主动上报告警
    bool ReportAlarm(const Gb28181Alarm& alarm);

    // 发送心跳保活（通常由 SipAgent 内部定时器自动完成，
    // 此接口允许上层手动触发）
    bool SendKeepalive();

    // ========================================================================
    // 查询接口
    // ========================================================================

    // 获取当前配置
    Gb28181Config GetConfig() const;

    // SIP 是否已注册到平台
    bool IsRegistered() const;

private:
    Gb28181Manager();  // 私有构造，强制使用 Instance()

    // ========================================================================
    // SipAgent 回调处理
    // ========================================================================

    // 收到 INVITE（实时点播请求）
    void OnInvite(const std::string& from, const std::string& to,
                  const std::string& call_id, const std::string& sdp);

    // 收到 BYE（结束会话）
    void OnBye(const std::string& call_id);

    // 收到 MESSAGE（XML 报文，如 Catalog 查询）
    void OnMessage(const std::string& from, const std::string& xml_body);

    // 注册成功
    void OnRegisterSuccess();

    // 注册失败
    void OnRegisterFailed(int status_code, const std::string& reason);

    // 收到 SUBSCRIBE（订阅请求）
    void OnSubscribe(const std::string& from, const std::string& call_id,
                     const std::string& event_type);

    // ========================================================================
    // XML 报文构造辅助函数
    // ========================================================================

    // 构造 Catalog 响应 XML
    std::string BuildCatalogResponse(const std::string& sn,
                                     const std::vector<Gb28181Channel>& channels);

    // 构造 DeviceInfo 响应 XML
    std::string BuildDeviceInfoResponse(const std::string& sn,
                                        const Gb28181DeviceInfo& info);

    // 构造心跳响应 XML
    std::string BuildKeepaliveResponse(const std::string& sn);

    // ========================================================================
    // 成员变量
    // ========================================================================

    Gb28181Config          config_;         // 配置
    std::unique_ptr<SipAgent> sip_agent_;   // SIP 代理
    mutable std::mutex     mutex_;          // 保护状态的互斥锁（mutable 允许在 const 函数中使用）
    bool                   initialized_ = false;
    bool                   running_     = false;
    bool                   registered_  = false;
};

} // namespace gb28181
