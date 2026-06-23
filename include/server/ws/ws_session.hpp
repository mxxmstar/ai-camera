#ifndef SERVER_WS_WS_SESSION_HPP
#define SERVER_WS_WS_SESSION_HPP

#include "server/ws/ws_frame.hpp"
#include "server/ws/ws_handshake.hpp"
#include "server/ws/ws_types.hpp"

#include <asio.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace ws {

class WsServer; // 前向声明

/// @brief WebSocket 连接会话
///
/// 生命周期：
///   1. TCP 连接建立后由 WsServer 创建 Session
///   2. 读取并完成 HTTP 握手（101 Switching Protocols）
///   3. 进入 WebSocket 帧读写循环
///   4. 收到 Close 帧或发生错误时关闭连接并回调 OnClose
///
/// 线程安全说明：
///   - 所有 asio 异步回调均在所属 io_context 线程执行；
///   - Send() 可被外部线程调用，内部通过发送队列 + strand 保证安全。
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    // ---- 回调函数类型定义 ----

    /// 握手成功后触发（连接就绪，可收发数据）
    using OpenHandler = std::function<void(std::shared_ptr<WsSession>)>;

    /// 收到一条完整消息（分片已重组）后触发
    using MessageHandler =
        std::function<void(std::shared_ptr<WsSession>, const Message&)>;

    /// 收到 Ping 帧后触发。默认实现会自动回复 Pong。
    using PingHandler =
        std::function<void(std::shared_ptr<WsSession>, const std::string&)>;

    /// 连接关闭后触发
    using CloseHandler =
        std::function<void(std::shared_ptr<WsSession>, CloseCode)>;

    /// @brief 构造会话
    /// @param socket 由 acceptor 转移进来的 TCP socket
    /// @param server 关联的服务器（用于从连接列表中移除自身）
    WsSession(asio::ip::tcp::socket socket, WsServer* server);

    ~WsSession();

    /// @brief 开始处理连接（先做 HTTP 握手）
    void Start();

    /// @brief 主动发送文本消息
    void SendText(const std::string& text);

    /// @brief 主动发送二进制消息
    void SendBinary(const std::vector<uint8_t>& data);

    /// @brief 主动发送 Ping（用于心跳检测）
    void SendPing(const std::string& payload = "");

    /// @brief 主动关闭连接
    /// @param code 关闭状态码
    /// @param reason 关闭原因（可选）
    void Close(CloseCode code = CloseCode::NormalClosure,
               const std::string& reason = "");

    // ---- 回调设置 ----
    void SetOpenHandler(OpenHandler h)    { open_handler_    = std::move(h); }
    void SetMessageHandler(MessageHandler h){ message_handler_= std::move(h); }
    void SetPingHandler(PingHandler h)    { ping_handler_    = std::move(h); }
    void SetCloseHandler(CloseHandler h)  { close_handler_   = std::move(h); }

    /// 获取远端地址（"ip:port" 形式）
    std::string GetRemoteAddress() const;

    /// 获取唯一会话 ID
    const std::string& GetId() const { return id_; }

    /// 获取协商后的子协议；若未协商成功则为空
    const std::string& GetSubProtocol() const { return subprotocol_; }

    /// 判断连接是否处于活动状态
    bool IsOpen() const { return state_ == State::Active; }

private:
    /// 会话状态机
    enum class State {
        Handshaking, ///< 等待 HTTP 握手
        Active,      ///< 握手完成，正常收发
        Closing,     ///< 已发送/收到 Close 帧，等待关闭
        Closed       ///< 已关闭
    };

    /// 发送队列项
    struct OutgoingItem {
        std::vector<uint8_t> data;   ///< 已编码的帧字节
        bool is_close = false;       ///< 是否为 Close 帧（发送后应关闭连接）
    };

    // ---- 内部流程 ----
    void DoReadHandshake();
    void DoReadFrame();
    void DoWrite();
    void RefreshHeartbeat();
    void ArmHeartbeatTimer();
    void OnHeartbeatTimer(const asio::error_code& ec);

    // ---- 帧处理 ----
    void OnFrameData(const asio::error_code& ec, std::size_t bytes_transferred);
    void ProcessFrameBuffer(std::size_t available);
    void HandleControlFrame(OpCode op, const uint8_t* payload, std::size_t len);
    void HandleDataFrame(OpCode op, bool fin, const uint8_t* payload, std::size_t len);

    /// 完成一条消息的重组并回调
    void DeliverMessage();

    /// 入队一条待发送帧，并触发写循环
    void EnqueueFrame(OutgoingItem item);

    /// 关闭底层 socket 并通知上层
    void DoClose(CloseCode code);

    /// 生成随机会话 ID
    static std::string GenerateId();

    // ---- 成员 ----
    asio::ip::tcp::socket socket_;
    WsServer*             server_;
    std::string           id_;
    std::atomic<State>    state_{State::Handshaking};
    SessionOptions        options_{};

    // 读取相关
    std::array<uint8_t, 65536> read_buf_{};
    std::vector<uint8_t>       handshake_buf_{}; ///< HTTP 握手读取缓冲
    std::vector<uint8_t>       frame_buf_{};   ///< 帧读取累积缓冲（握手后用于累积帧字节）

    // 消息重组（分片帧合并为一条消息）
    OpCode               msg_opcode_{OpCode::Text}; ///< 当前正在重组的消息类型
    std::vector<uint8_t> msg_payload_;               ///< 当前消息累积载荷
    bool                 msg_started_ = false;       ///< 是否已收到首片

    // 发送相关（strand 保证发送回调串行化）
    asio::any_io_executor   strand_;
    asio::steady_timer      heartbeat_timer_;
    std::queue<OutgoingItem> send_queue_;
    std::atomic<bool>        writing_{false};
    std::chrono::steady_clock::time_point last_activity_{};
    bool awaiting_pong_ = false;
    std::string subprotocol_;

    // 回调
    OpenHandler    open_handler_;
    MessageHandler message_handler_;
    PingHandler    ping_handler_;
    CloseHandler   close_handler_;
};

} // namespace ws

#endif // SERVER_WS_WS_SESSION_HPP
