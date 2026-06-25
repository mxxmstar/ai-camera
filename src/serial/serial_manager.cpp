#include "ai-camera/serial/serial_manager.h"
#include "ai-camera/log/logger.h"

#include <future>
#include <iostream>
#include <thread>

namespace serial {

// ============================================================================
// 单例实现
// ============================================================================

SerialManager& SerialManager::Instance() {
    static SerialManager instance;
    return instance;
}

SerialManager::SerialManager()
    : running_(false) {
}

SerialManager::~SerialManager() {
    Stop();
}

// ============================================================================
// 生命周期管理
// ============================================================================

bool SerialManager::Start() {
    if (running_) return true;

    try {
        io_ctx_ = std::make_unique<asio::io_context>();

        // 启动 IO 线程
        running_ = true;
        io_thread_ = std::thread(&SerialManager::IoThreadFunc, this);

        LOG_INFO("[SerialManager] Started");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("[SerialManager] Start failed: {}", e.what());
        return false;
    }
}

void SerialManager::Stop() {
    if (!running_) return;

    running_ = false;

    // 关闭所有串口
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, port] : ports_) {
        if (port) {
            port->Close();
        }
    }
    ports_.clear();

    // 停止 IO 上下文
    if (io_ctx_) {
        io_ctx_->stop();
    }

    // 等待 IO 线程退出
    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    io_ctx_.reset();
    LOG_INFO("[SerialManager] Stopped");
}

// ============================================================================
// 串口管理
// ============================================================================

bool SerialManager::Open(const std::string& name, const SerialConfig& cfg) {
    if (!running_ || !io_ctx_) {
        LOG_ERROR("[SerialManager] Not started");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 若已存在同名串口，先关闭
    if (ports_.count(name)) {
        LOG_WARN("[SerialManager] {} already open, closing first", name);
        ports_[name]->Close();
    }

    // 创建 SerialPort 实例
    auto port = std::make_unique<SerialPort>(*io_ctx_, name, cfg);

    // 在 IO 线程中打开（确保 IO 操作在同一线程）
    // 使用 post 将 Open 操作派发到 IO 线程
    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();

    asio::post(*io_ctx_, [&port, &promise]() {
        bool result = port->Open();
        promise.set_value(result);
    });

    // 等待打开结果（超时 1 秒）
    auto status = future.wait_for(std::chrono::seconds(1));
    if (status == std::future_status::timeout) {
        LOG_ERROR("[SerialManager] Open {} timeout", name);
        return false;
    }

    bool result = future.get();
    if (result) {
        ports_[name] = std::move(port);
        LOG_INFO("[SerialManager] Opened serial port: {}", name);
    } else {
        LOG_ERROR("[SerialManager] Failed to open serial port: {}", name);
    }

    return result;
}

void SerialManager::Close(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = ports_.find(name);
    if (it == ports_.end()) {
        LOG_WARN("[SerialManager] Close failed: {} not found", name);
        return;
    }

    // 在 IO 线程中关闭
    asio::post(*io_ctx_, [port = it->second.get()]() {
        port->Close();
    });

    ports_.erase(it);
    LOG_INFO("[SerialManager] Closed serial port: {}", name);
}

bool SerialManager::Send(const std::string& name, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto* port = FindPortUnsafe(name);
    if (!port) {
        LOG_WARN("[SerialManager] Send failed: {} not found", name);
        return false;
    }

    return port->Send(data);
}

bool SerialManager::IsOpen(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ports_.find(name);
    if (it == ports_.end()) return false;
    return it->second->IsOpen();
}

// ============================================================================
// 回调注册
// ============================================================================

void SerialManager::RegisterRecvCallback(const std::string& name, RecvHandler cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* port = FindPortUnsafe(name);
    if (port) {
        port->SetRecvHandler(std::move(cb));
    }
}

void SerialManager::RegisterErrorCallback(const std::string& name, ErrorHandler cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* port = FindPortUnsafe(name);
    if (port) {
        port->SetErrorHandler(std::move(cb));
    }
}

// ============================================================================
// 查询接口
// ============================================================================

std::vector<std::string> SerialManager::GetPortNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& [name, _] : ports_) {
        names.push_back(name);
    }
    return names;
}

SerialConfig SerialManager::GetConfig(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ports_.find(name);
    if (it == ports_.end()) {
        return {};  // 返回默认配置
    }
    return it->second->GetConfig();
}

std::string SerialManager::GetAllStatusJson() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream oss;
    oss << "{\"ports\":[";

    bool first = true;
    for (const auto& [name, port] : ports_) {
        if (!first) oss << ",";
        first = false;

        oss << "{"
            << "\"name\":\"" << name << "\","
            << "\"device\":\"" << port->GetConfig().device << "\","
            << "\"baud_rate\":" << port->GetConfig().baud_rate << ","
            << "\"protocol\":\"" << port->GetConfig().protocol << "\","
            << "\"open\":" << (port->IsOpen() ? "true" : "false")
            << "}";
    }

    oss << "]}";
    return oss.str();
}

std::string SerialManager::GetPortStatusJson(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = ports_.find(name);
    if (it == ports_.end()) {
        return "{\"error\":\"port not found\"}";
    }

    const auto& port = it->second;
    const auto& cfg  = port->GetConfig();

    std::ostringstream oss;
    oss << "{"
        << "\"name\":\"" << name << "\","
        << "\"device\":\"" << cfg.device << "\","
        << "\"baud_rate\":" << cfg.baud_rate << ","
        << "\"data_bits\":" << cfg.data_bits << ","
        << "\"parity\":\"" << cfg.parity << "\","
        << "\"stop_bits\":" << cfg.stop_bits << ","
        << "\"protocol\":\"" << cfg.protocol << "\","
        << "\"open\":" << (port->IsOpen() ? "true" : "false")
        << "}";

    return oss.str();
}

// ============================================================================
// 私有方法
// ============================================================================

void SerialManager::IoThreadFunc() {
    LOG_INFO("[SerialManager] IO thread started");

    // 运行 IO 上下文（阻塞直到 stop 被调用）
    try {
        io_ctx_->run();
    } catch (const std::exception& e) {
        LOG_ERROR("[SerialManager] IO thread exception: {}", e.what());
    }

    LOG_INFO("[SerialManager] IO thread stopped");
}

SerialPort* SerialManager::FindPortUnsafe(const std::string& name) const {
    auto it = ports_.find(name);
    if (it == ports_.end()) return nullptr;
    return it->second.get();
}

} // namespace serial
