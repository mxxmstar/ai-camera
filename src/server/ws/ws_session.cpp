#include "server/ws/ws_server.hpp"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace ws {

// ============================================================
// 工具：生成随机会话 ID
// ============================================================
std::string WsSession::GenerateId() {
    std::ostringstream oss;
    std::random_device rd;
    std::mt19937 gen(rd());
    oss << std::hex << std::setw(8) << std::setfill('0')
        << std::uniform_int_distribution<uint32_t>(0, 0xFFFFFFFF)(gen);
    return oss.str();
}

// ============================================================
// 构造与析构
// ============================================================
WsSession::WsSession(asio::ip::tcp::socket socket, WsServer* server)
    : socket_(std::move(socket)),
      server_(server),
      id_(GenerateId()),
      options_(server == nullptr ? SessionOptions() : server->GetSessionOptions()),
      strand_(asio::make_strand(socket_.get_executor())),
      heartbeat_timer_(strand_)
{
}

WsSession::~WsSession() {
    // 确保底层 socket 被关闭
    asio::error_code ignored_ec;
    socket_.close(ignored_ec);
}

// ============================================================
// 启动：先读取 HTTP 握手请求
// ============================================================
void WsSession::Start() {
    DoReadHandshake();
}

void WsSession::DoReadHandshake() {
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buf_),
        [this, self](const asio::error_code& ec, std::size_t n) {
            if (ec) {
                DoClose(CloseCode::AbnormalClosure);
                return;
            }

            handshake_buf_.insert(handshake_buf_.end(),
                                  read_buf_.data(),
                                  read_buf_.data() + n);
            if (handshake_buf_.size() > options_.max_handshake_bytes) {
                static const char* bad = "HTTP/1.1 400 Bad Request\r\n"
                                         "Connection: close\r\n\r\n";
                asio::error_code ec_wr;
                asio::write(socket_, asio::buffer(bad, std::strlen(bad)), ec_wr);
                DoClose(CloseCode::ProtocolError);
                return;
            }

            handshake::HttpRequest req;
            const long consumed = handshake::ParseHttpRequest(
                handshake_buf_.data(), handshake_buf_.size(), req);

            if (consumed == 0) {
                DoReadHandshake();
                return;
            }
            if (consumed < 0 || !handshake::IsWebSocketUpgrade(req)) {
                static const char* bad = "HTTP/1.1 400 Bad Request\r\n"
                                         "Connection: close\r\n\r\n";
                asio::error_code ec_wr;
                asio::write(socket_, asio::buffer(bad, std::strlen(bad)),
                            ec_wr);
                DoClose(CloseCode::ProtocolError);
                return;
            }

            const std::string key = req.headers["sec-websocket-key"];
            subprotocol_ = server_ == nullptr
                ? std::string()
                : handshake::NegotiateSubProtocol(
                      req, server_->GetSupportedSubProtocols());
            const std::string resp =
                handshake::BuildHandshakeResponse(key, subprotocol_);
            asio::error_code ec_wr;
            asio::write(socket_, asio::buffer(resp), ec_wr);
            if (ec_wr) {
                DoClose(CloseCode::AbnormalClosure);
                return;
            }

            state_ = State::Active;
            RefreshHeartbeat();
            ArmHeartbeatTimer();

            const long remaining =
                static_cast<long>(handshake_buf_.size()) - consumed;
            if (remaining > 0) {
                frame_buf_.insert(frame_buf_.end(),
                                  handshake_buf_.data() + consumed,
                                  handshake_buf_.data() + consumed + remaining);
            }
            handshake_buf_.clear();

            if (open_handler_) open_handler_(self);

            if (!frame_buf_.empty()) {
                ProcessFrameBuffer(frame_buf_.size());
            } else {
                DoReadFrame();
            }
        });
}

// ============================================================
// 读取 WebSocket 帧
// ============================================================
void WsSession::DoReadFrame() {
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buf_),
        [this, self](const asio::error_code& ec, std::size_t n) {
            OnFrameData(ec, n);
        });
}

void WsSession::OnFrameData(const asio::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
        DoClose(CloseCode::AbnormalClosure);
        return;
    }

    // 将新读取的数据追加到帧缓冲
    frame_buf_.insert(frame_buf_.end(),
                      read_buf_.data(),
                      read_buf_.data() + bytes_transferred);
    if (frame_buf_.size() > options_.max_frame_buffer_bytes) {
        Close(CloseCode::MessageTooBig, "frame buffer too large");
        return;
    }

    ProcessFrameBuffer(frame_buf_.size());
}

/// @brief 尝试从帧缓冲中连续解析并处理帧
void WsSession::ProcessFrameBuffer(std::size_t available) {
    auto self = shared_from_this();
    std::size_t offset = 0;

    while (offset < available) {
        const uint8_t* base = frame_buf_.data() + offset;
        std::size_t    remain = available - offset;

        // 1. 解析帧头
        frame::Header hdr;
        std::size_t header_len = frame::ParseHeader(base, remain, hdr);

        if (hdr.payload_len == UINT64_MAX) {
            // 协议错误
            DoClose(CloseCode::ProtocolError);
            return;
        }

        if (header_len == 0) {
            // 帧头不完整，等待更多数据
            break;
        }

        if (!hdr.masked) {
            DoClose(CloseCode::ProtocolError);
            return;
        }

        if (!IsControlFrame(hdr.opcode)) {
            const uint64_t accumulated =
                (hdr.opcode == OpCode::Continuation) ? msg_payload_.size() : 0;
            if (accumulated + hdr.payload_len > options_.max_message_bytes) {
                Close(CloseCode::MessageTooBig, "message too large");
                return;
            }
        }

        // 2. 检查整帧是否已完整
        const uint64_t total = frame::FrameTotalSize(hdr);
        if (total > options_.max_frame_buffer_bytes) {
            Close(CloseCode::MessageTooBig, "frame too large");
            return;
        }
        if (remain < total) {
            // 载荷不完整，等待更多数据
            break;
        }

        RefreshHeartbeat();

        // 3. 提取载荷（并解掩码）
        const uint8_t* payload = base + header_len;
        std::vector<uint8_t> payload_data;
        if (hdr.payload_len > 0) {
            payload_data.assign(payload, payload + hdr.payload_len);
            if (hdr.masked) {
                frame::ApplyMask(payload_data.data(), hdr.payload_len, hdr.mask_key);
            }
        }

        // 4. 处理帧
        if (IsControlFrame(hdr.opcode)) {
            // 控制帧不可分片，且载荷 <=125
            if (!hdr.fin || hdr.payload_len > 125) {
                DoClose(CloseCode::ProtocolError);
                return;
            }
            HandleControlFrame(hdr.opcode,
                               payload_data.data(),
                               payload_data.size());
            if (state_ == State::Closed) return;
        } else {
            // 数据帧（Text / Binary / Continuation）
            HandleDataFrame(hdr.opcode, hdr.fin,
                            payload_data.data(),
                            payload_data.size());
        }

        offset += static_cast<std::size_t>(total);
    }

    // 移除已处理的部分，保留未完整的尾部
    if (offset > 0) {
        frame_buf_.erase(frame_buf_.begin(),
                         frame_buf_.begin() + offset);
    } else {
        // 未消费任何字节，但仍保留缓冲（避免 erase 空操作的开销）
    }

    // 继续读取
    if (state_ != State::Closed && state_ != State::Closing) {
        DoReadFrame();
    }
}

// ============================================================
// 控制帧处理
// ============================================================
void WsSession::HandleControlFrame(OpCode op,
                                   const uint8_t* payload,
                                   std::size_t len)
{
    auto self = shared_from_this();

    if (op == OpCode::Close) {
        // 解析关闭帧的载荷：前 2 字节为状态码（可选），其余为原因
        CloseCode code = CloseCode::NoStatusReceived;
        std::string reason;
        if (len >= 2) {
            uint16_t raw = (static_cast<uint16_t>(payload[0]) << 8) |
                           payload[1];
            code = static_cast<CloseCode>(raw);
            if (len > 2) {
                reason.assign(reinterpret_cast<const char*>(payload + 2), len - 2);
            }
        }
        (void)reason;

        // 如果是被动收到 Close，需回复 Close 帧再关闭
        if (state_ == State::Active) {
            state_ = State::Closing;
            // 回复相同状态码的 Close 帧
            uint8_t close_payload[2] = {
                static_cast<uint8_t>((static_cast<uint16_t>(code) >> 8) & 0xFF),
                static_cast<uint8_t>(static_cast<uint16_t>(code) & 0xFF)
            };
            auto frame = frame::EncodeServerFrame(
                true, OpCode::Close, close_payload, 2);
            OutgoingItem item{std::move(frame), /*is_close=*/true};
            EnqueueFrame(std::move(item));
        }
        // 发送完 Close 帧后真正关闭（在 DoWrite 完成后）
        return;
    }

    if (op == OpCode::Ping) {
        std::string ping_payload(reinterpret_cast<const char*>(payload), len);
        // 用户回调（可选）
        if (ping_handler_) {
            ping_handler_(self, ping_payload);
        }
        // 自动回复 Pong（载荷与 Ping 一致）
        auto frame = frame::EncodeServerFrame(
            true, OpCode::Pong, payload, len);
        OutgoingItem item{std::move(frame), false};
        EnqueueFrame(std::move(item));
        return;
    }

    if (op == OpCode::Pong) {
        // 收到 Pong，说明对端存活。这里不做特殊处理（可扩展为更新心跳时间戳）
        return;
    }
}

// ============================================================
// 数据帧处理（含分片重组）
// ============================================================
void WsSession::HandleDataFrame(OpCode op, bool fin,
                                const uint8_t* payload,
                                std::size_t len)
{
    // 首片必须为 Text 或 Binary；后续片为 Continuation
    if (op == OpCode::Text || op == OpCode::Binary) {
        if (msg_started_) {
            // 上一条消息未完成却开始了新消息
            DoClose(CloseCode::ProtocolError);
            return;
        }
        msg_started_ = true;
        msg_opcode_ = op;
        msg_payload_.clear();
        if (len > 0) {
            if (len > options_.max_message_bytes) {
                Close(CloseCode::MessageTooBig, "message too large");
                return;
            }
            msg_payload_.assign(payload, payload + len);
        }
    } else if (op == OpCode::Continuation) {
        if (!msg_started_) {
            // 没有前导分片却收到 Continuation
            DoClose(CloseCode::ProtocolError);
            return;
        }
        if (len > 0) {
            if (msg_payload_.size() + len > options_.max_message_bytes) {
                Close(CloseCode::MessageTooBig, "message too large");
                return;
            }
            msg_payload_.insert(msg_payload_.end(), payload, payload + len);
        }
    } else {
        DoClose(CloseCode::ProtocolError);
        return;
    }

    if (fin) {
        // 消息重组完成
        DeliverMessage();
        msg_started_ = false;
        msg_payload_.clear();
    }
}

void WsSession::RefreshHeartbeat() {
    last_activity_ = std::chrono::steady_clock::now();
    awaiting_pong_ = false;
}

void WsSession::ArmHeartbeatTimer() {
    auto self = shared_from_this();
    heartbeat_timer_.expires_after(options_.heartbeat_interval);
    heartbeat_timer_.async_wait(
        asio::bind_executor(strand_,
            [this, self](const asio::error_code& ec) {
                OnHeartbeatTimer(ec);
            }));
}

void WsSession::OnHeartbeatTimer(const asio::error_code& ec) {
    if (ec || state_ != State::Active) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto idle = now - last_activity_;
    if (awaiting_pong_) {
        if (idle >= options_.heartbeat_timeout) {
            DoClose(CloseCode::AbnormalClosure);
            return;
        }
    } else if (idle >= options_.heartbeat_interval) {
        awaiting_pong_ = true;
        SendPing();
    }

    ArmHeartbeatTimer();
}

void WsSession::DeliverMessage() {
    if (!message_handler_) return;
    Message msg;
    msg.opcode = msg_opcode_;
    msg.payload = std::move(msg_payload_);
    // 注意：move 后 msg_payload_ 为空，但调用方 (HandleDataFrame) 会 clear
    // 这里需恢复以便 clear 正常工作
    msg_payload_.clear();
    message_handler_(shared_from_this(), msg);
}

// ============================================================
// 发送逻辑
// ============================================================
void WsSession::SendText(const std::string& text) {
    auto frame = frame::EncodeServerFrame(
        true, OpCode::Text,
        reinterpret_cast<const uint8_t*>(text.data()),
        text.size());
    OutgoingItem item{std::move(frame), false};
    EnqueueFrame(std::move(item));
}

void WsSession::SendBinary(const std::vector<uint8_t>& data) {
    auto frame = frame::EncodeServerFrame(
        true, OpCode::Binary, data.data(), data.size());
    OutgoingItem item{std::move(frame), false};
    EnqueueFrame(std::move(item));
}

void WsSession::SendPing(const std::string& payload) {
    auto frame = frame::EncodeServerFrame(
        true, OpCode::Ping,
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size());
    OutgoingItem item{std::move(frame), false};
    EnqueueFrame(std::move(item));
}

void WsSession::Close(CloseCode code, const std::string& reason) {
    if (state_ != State::Active) return;
    state_ = State::Closing;

    // 构建 Close 帧载荷：2 字节状态码 + 可选原因
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>((static_cast<uint16_t>(code) >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(static_cast<uint16_t>(code) & 0xFF));
    if (!reason.empty()) {
        payload.insert(payload.end(), reason.begin(), reason.end());
    }

    auto frame = frame::EncodeServerFrame(
        true, OpCode::Close, payload.data(), payload.size());
    OutgoingItem item{std::move(frame), /*is_close=*/true};
    EnqueueFrame(std::move(item));
}

void WsSession::EnqueueFrame(OutgoingItem item) {
    // 在 strand 上串行化入队与启动写操作
    auto self = shared_from_this();
    asio::post(strand_, [this, self, item = std::move(item)]() mutable {
        bool was_empty = send_queue_.empty();
        send_queue_.push(std::move(item));
        if (was_empty && !writing_) {
            DoWrite();
        }
    });
}

void WsSession::DoWrite() {
    auto self = shared_from_this();

    if (send_queue_.empty()) {
        writing_ = false;
        return;
    }

    writing_ = true;
    auto& item = send_queue_.front();
    auto buf = asio::buffer(item.data);
    bool is_close = item.is_close;

    asio::async_write(socket_, buf,
        asio::bind_executor(strand_,
            [this, self, is_close](const asio::error_code& ec, std::size_t) {
                send_queue_.pop();

                if (ec) {
                    writing_ = false;
                    DoClose(CloseCode::AbnormalClosure);
                    return;
                }

                if (is_close) {
                    // Close 帧已发送完毕，关闭连接
                    writing_ = false;
                    DoClose(CloseCode::NormalClosure);
                    return;
                }

                // 继续发送队列中的下一帧
                if (!send_queue_.empty()) {
                    DoWrite();
                } else {
                    writing_ = false;
                }
            }));
}

// ============================================================
// 关闭
// ============================================================
void WsSession::DoClose(CloseCode code) {
    State expected = State::Closed;
    // 用 compare_exchange 保证只触发一次关闭流程
    if (!state_.compare_exchange_strong(expected, State::Closed)) {
        // state_ 不是 Closed，尝试从其它状态转为 Closed
        State old = state_.exchange(State::Closed);
        if (old == State::Closed) return; // 已经关闭过
    }

    heartbeat_timer_.cancel();
    asio::error_code ignored_ec;
    socket_.close(ignored_ec);

    if (close_handler_) {
        close_handler_(shared_from_this(), code);
    }

    // 从服务器会话表中移除
    if (server_) {
        server_->RemoveSession(id_);
    }
}

// ============================================================
// 获取远端地址
// ============================================================
std::string WsSession::GetRemoteAddress() const {
    asio::error_code ec;
    auto ep = socket_.remote_endpoint(ec);
    if (ec) return "unknown";
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

} // namespace ws
