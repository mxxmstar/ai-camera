#pragma once

#include "serial_config.h"
#include "serial_protocol.h"

#include <asio.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <array>

namespace serial {

/// @brief 串口接收回调
/// @param port_name  串口名称（如 "sensor1"）
/// @param frame      解析出的完整帧数据
using RecvHandler = std::function<void(const std::string& port_name,
                                       const std::string& frame)>;

/// @brief 串口错误回调
/// @param port_name  串口名称
/// @param error_msg  错误描述
using ErrorHandler = std::function<void(const std::string& port_name,
                                        const std::string& error_msg)>;

/// @brief 串口底层封装（基于 asio::serial_port 异步 I/O）
///
/// 功能：
///   - 异步读写，不阻塞调用线程
///   - 内部维护写队列，支持多线程安全调用 Send()
///   - 集成协议解析（通过 SerialProtocol 策略）
///   - 读缓冲区固定 512 字节
///
/// 线程安全：Send() 线程安全；其他成员函数需在 IO 线程中调用。
class SerialPort {
public:
    /// @param io_ctx  ASIO IO 上下文（由 SerialManager 提供）
    /// @param name    串口逻辑名称（用于回调标识）
    /// @param config  串口配置
    SerialPort(asio::io_context& io_ctx,
               const std::string& name,
               const SerialConfig& config);

    ~SerialPort();

    /// 打开串口并启动异步读取
    bool Open();

    /// 关闭串口（停止读写）
    void Close();

    /// 是否已打开
    bool IsOpen() const;

    /// 发送数据（线程安全，立即返回，实际发送在 IO 线程中异步完成）
    /// @return true 表示数据已加入发送队列
    bool Send(const std::string& data);

    // ========================================================================
    // 回调注册
    // ========================================================================

    void SetRecvHandler(RecvHandler h) { recv_handler_ = std::move(h); }
    void SetErrorHandler(ErrorHandler h) { error_handler_ = std::move(h); }

    // ========================================================================
    // 查询接口
    // ========================================================================

    SerialConfig GetConfig() const { return config_; }
    std::string  GetName() const { return name_; }

private:
    /// 启动异步读取
    void DoRead();

    /// 异步读取完成回调
    void OnRead(const asio::error_code& ec, size_t bytes_transferred);

    /// 启动异步写入（仅当 writing_ == false 时调用）
    void DoWrite();

    /// 异步写入完成回调
    void OnWrite(const asio::error_code& ec, size_t bytes_transferred);

    /// 配置 asio::serial_port 参数（波特率、数据位、校验、停止位）
    bool ApplyConfig();

    asio::io_context&           io_ctx_;
    std::unique_ptr<asio::serial_port> port_;
    std::string                 name_;
    SerialConfig                config_;
    std::unique_ptr<SerialProtocol> protocol_;

    /// 读缓冲区
    static constexpr size_t READ_BUF_SIZE = 512;
    std::array<char, READ_BUF_SIZE> read_buf_;

    /// 写队列（多线程安全）
    std::queue<std::string> write_queue_;
    bool                    writing_;
    std::mutex              write_mutex_;

    /// 回调
    RecvHandler  recv_handler_;
    ErrorHandler error_handler_;
};

} // namespace serial
