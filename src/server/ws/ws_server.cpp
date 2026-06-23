#include "server/ws/ws_server.hpp"

#include <iostream>

namespace ws {

// ============================================================
// 构造与析构
// ============================================================
WsServer::WsServer(asio::io_context* io_context,
                   const std::string& address,
                   unsigned short port)
    // 成员初始化顺序与头文件声明顺序一致：
    //   owns_io_context_ -> internal_io_ -> io_context_ -> acceptor_
    : owns_io_context_(io_context == nullptr),
      internal_io_(io_context ? nullptr : std::make_unique<asio::io_context>()),
      io_context_(io_context ? *io_context : *internal_io_),
      acceptor_(io_context_,
                asio::ip::tcp::endpoint(asio::ip::make_address(address), port))
{
    if (owns_io_context_) {
        work_guard_ = std::make_unique<
            asio::executor_work_guard<asio::io_context::executor_type>>(
                io_context_.get_executor());
    }
    std::cout << "[WsServer] Will listen on " << address << ":" << port << std::endl;
}

WsServer::~WsServer() {
    Stop();
}

// ============================================================
// 启动
// ============================================================
void WsServer::Start() {
    if (running_.exchange(true)) {
        return; // 已在运行
    }

    DoAccept();

    // 若服务器自管理 io_context，则启动后台线程
    if (owns_io_context_) {
        io_thread_ = std::thread([this]() {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                std::cerr << "[WsServer] io_context exception: " << e.what() << std::endl;
            }
        });
        std::cout << "[WsServer] Started with internal io_context thread." << std::endl;
    } else {
        std::cout << "[WsServer] Started (using external io_context)." << std::endl;
    }
}

// ============================================================
// 停止
// ============================================================
void WsServer::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

    asio::error_code ec;
    acceptor_.close(ec);

    // 关闭所有会话
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& kv : sessions_) {
            if (kv.second) {
                kv.second->Close(CloseCode::GoingAway);
            }
        }
        sessions_.clear();
    }

    if (owns_io_context_) {
        work_guard_.reset();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }
    std::cout << "[WsServer] Stopped." << std::endl;
}

// ============================================================
// 接受连接
// ============================================================
void WsServer::DoAccept() {
    if (!running_) return;

    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<WsSession>(std::move(socket), this);

                // 将服务器的默认回调绑定到会话
                if (on_open_)     session->SetOpenHandler(on_open_);
                if (on_message_)  session->SetMessageHandler(on_message_);
                if (on_ping_)     session->SetPingHandler(on_ping_);
                if (on_close_)    session->SetCloseHandler(on_close_);

                AddSession(session);
                session->Start();
            } else if (ec != asio::error::operation_aborted) {
                std::cerr << "[WsServer] Accept error: " << ec.message() << std::endl;
            }

            // 继续接受下一个连接
            if (running_) {
                DoAccept();
            }
        });
}

// ============================================================
// 会话管理
// ============================================================
void WsServer::AddSession(std::shared_ptr<WsSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_[session->GetId()] = std::move(session);
}

void WsServer::RemoveSession(const std::string& id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(id);
}

std::size_t WsServer::GetSessionCount() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

// ============================================================
// 广播
// ============================================================
void WsServer::BroadcastText(const std::string& text) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& kv : sessions_) {
        if (kv.second && kv.second->IsOpen()) {
            kv.second->SendText(text);
        }
    }
}

void WsServer::BroadcastBinary(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& kv : sessions_) {
        if (kv.second && kv.second->IsOpen()) {
            kv.second->SendBinary(data);
        }
    }
}

} // namespace ws
