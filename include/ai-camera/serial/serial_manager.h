#pragma once

#include "serial_port.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace serial {

/// @brief 串口管理模块单例
///
/// 管理多个串口实例，每个实例独立配置协议。
/// 拥有独立的 asio::io_context 和专用 IO 线程。
///
/// 用法示例：
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
///   // 初始化（在 main.cpp 中）
///   serial::SerialManager::Instance().Start();
///
///   // 打开串口
///   serial::SerialConfig cfg;
///   cfg.device = "COM3";
///   cfg.baud_rate = 115200;
///   cfg.protocol = "line";
///   serial::SerialManager::Instance().Open("sensor1", cfg);
///
///   // 注册接收回调
///   serial::SerialManager::Instance().RegisterRecvCallback(
///       "sensor1",
///       [](const std::string& name, const std::string& frame) {
///           LOG_INFO("Received from {}: {}", name, frame);
///       }
///   );
///
///   // 发送数据
///   serial::SerialManager::Instance().Send("sensor1", "AT\r\n");
///
///   // 停止（在 signal_handler 中）
///   serial::SerialManager::Instance().Stop();
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class SerialManager {
public:
    static SerialManager& Instance();

    ~SerialManager();

    // ========================================================================
    // 生命周期管理
    // ========================================================================

    /// 启动串口管理模块（创建 io_context 和专用 IO 线程）
    /// @return 是否成功启动
    bool Start();

    /// 停止所有串口并退出 IO 线程
    void Stop();

    /// 是否已启动
    bool IsRunning() const { return running_; }

    // ========================================================================
    // 串口管理
    // ========================================================================

    /// 打开指定名称的串口
    /// @param name 串口逻辑名称（如 "sensor1", "ptz"）
    /// @param cfg  串口配置
    /// @return     true 表示打开成功
    bool Open(const std::string& name, const SerialConfig& cfg);

    /// 关闭指定名称的串口
    void Close(const std::string& name);

    /// 向指定串口发送数据（线程安全）
    bool Send(const std::string& name, const std::string& data);

    /// 查询串口是否打开
    bool IsOpen(const std::string& name) const;

    // ========================================================================
    // 回调注册
    // ========================================================================

    /// 注册接收回调（每个串口可独立注册）
    void RegisterRecvCallback(const std::string& name, RecvHandler cb);

    /// 注册错误回调
    void RegisterErrorCallback(const std::string& name, ErrorHandler cb);

    // ========================================================================
    // 查询接口
    // ========================================================================

    /// 获取所有已注册串口名称列表
    std::vector<std::string> GetPortNames() const;

    /// 获取指定串口配置
    SerialConfig GetConfig(const std::string& name) const;

    /// 获取所有串口状态 JSON（用于 HTTP API）
    std::string GetAllStatusJson() const;

    /// 获取指定串口状态 JSON
    std::string GetPortStatusJson(const std::string& name) const;

private:
    SerialManager();
    SerialManager(const SerialManager&) = delete;
    SerialManager& operator=(const SerialManager&) = delete;

    /// IO 线程主函数
    void IoThreadFunc();

    /// 查找串口（内部使用，需持有 mutex_）
    SerialPort* FindPortUnsafe(const std::string& name) const;

    std::unordered_map<std::string, std::unique_ptr<SerialPort>> ports_;
    mutable std::mutex mutex_;

    std::unique_ptr<asio::io_context> io_ctx_;
    std::thread                       io_thread_;
    std::atomic<bool>                 running_{false};
};

} // namespace serial
