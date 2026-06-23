#ifndef SERVER_WS_WS_SERVER_HPP
#define SERVER_WS_WS_SERVER_HPP

#include "server/ws/ws_session.hpp"

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ws {

/// @brief WebSocket 服务器
///
/// 职责：
///   - 监听 TCP 端口，接受新连接
///   - 为每个连接创建 WsSession 并完成握手
///   - 管理所有活动会话的生命周期
///   - 提供 Broadcast 接口向所有连接广播消息
///
/// 用法示例：
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
///   ws::WsServer server(io_context, "0.0.0.0", 9002);
///
///   server.SetOnOpen([](std::shared_ptr<ws::WsSession> s) {
///       std::cout << "client connected: " << s->GetRemoteAddress() << "\n";
///   });
///   server.SetOnMessage([](std::shared_ptr<ws::WsSession> s, const ws::Message& m) {
///       // 回显
///       s->SendText(m.AsString());
///   });
///   server.Start();
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class WsServer {
public:
    /// @brief 构造服务器
    /// @param io_context 外部提供的 io_context（若为 nullptr 则内部创建并自管理线程）
    /// @param address     监听地址，如 "0.0.0.0"
    /// @param port        监听端口
    WsServer(asio::io_context* io_context,
             const std::string& address,
             unsigned short port);

    ~WsServer();

    /// 开始接受连接（非阻塞）
    void Start();

    /// 停止服务器，关闭所有连接
    void Stop();

    /// 是否正在运行
    bool IsRunning() const { return running_; }

    // ---- 回调设置（转发给每个新建会话）----
    void SetOnOpen(WsSession::OpenHandler h)     { on_open_     = std::move(h); }
    void SetOnMessage(WsSession::MessageHandler h){ on_message_  = std::move(h); }
    void SetOnPing(WsSession::PingHandler h)     { on_ping_     = std::move(h); }
    void SetOnClose(WsSession::CloseHandler h)   { on_close_    = std::move(h); }
    void SetSessionOptions(const SessionOptions& options) {
        session_options_ = options;
        if (session_options_.heartbeat_timeout < session_options_.heartbeat_interval) {
            session_options_.heartbeat_timeout = session_options_.heartbeat_interval;
        }
    }

    const SessionOptions& GetSessionOptions() const {
        return session_options_;
    }

    void SetSupportedSubProtocols(std::vector<std::string> protocols) {
        supported_subprotocols_ = std::move(protocols);
    }

    const std::vector<std::string>& GetSupportedSubProtocols() const {
        return supported_subprotocols_;
    }

    /// @brief 向所有活动会话广播文本消息
    void BroadcastText(const std::string& text);

    /// @brief 向所有活动会话广播二进制消息
    void BroadcastBinary(const std::vector<uint8_t>& data);

    /// 获取当前活动会话数量
    std::size_t GetSessionCount() const;

private:
    /// 开始一次异步 accept
    void DoAccept();

    /// 注册一个新会话
    void AddSession(std::shared_ptr<WsSession> session);

    /// 移除一个已关闭的会话（由 WsSession 调用）
    /// 声明为 friend 以便会话访问
    friend class WsSession;
    void RemoveSession(const std::string& id);

    // ---- 会话管理 ----
    // 注：会话表在 io_context 线程访问，故用互斥保护（广播可能从其它线程调用）
    mutable std::mutex sessions_mutex_;
    std::map<std::string, std::shared_ptr<WsSession>> sessions_;

    // ---- 网络相关 ----
    // 注意：成员声明顺序即初始化顺序，须保证 io_context_ 在 acceptor_ 之前
    bool                              owns_io_context_ = false;
    std::unique_ptr<asio::io_context> internal_io_;
    asio::io_context&                 io_context_;   ///< 引用，指向内部或外部 io_context
    asio::ip::tcp::acceptor           acceptor_;
    std::unique_ptr<asio::executor_work_guard<
        asio::io_context::executor_type>> work_guard_;
    std::thread                       io_thread_;
    std::atomic<bool>                 running_{false};

    // ---- 默认回调（复制到每个新会话）----
    WsSession::OpenHandler    on_open_;
    WsSession::MessageHandler on_message_;
    WsSession::PingHandler    on_ping_;
    WsSession::CloseHandler   on_close_;
    SessionOptions            session_options_;
    std::vector<std::string>  supported_subprotocols_;
};

} // namespace ws

#endif // SERVER_WS_WS_SERVER_HPP
