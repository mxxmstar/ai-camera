#include "ai-camera/serial/serial_port.h"
#include "ai-camera/log/logger.h"

#include <asio.hpp>
#include <iostream>

namespace serial {

SerialPort::SerialPort(asio::io_context& io_ctx,
                       const std::string& name,
                       const SerialConfig& config)
    : io_ctx_(io_ctx)
    , name_(name)
    , config_(config)
    , writing_(false)
    , recv_handler_(nullptr)
    , error_handler_(nullptr) {
}

SerialPort::~SerialPort() {
    Close();
}

bool SerialPort::Open() {
    try {
        port_ = std::make_unique<asio::serial_port>(io_ctx_);
        port_->open(config_.device);
        if (!port_->is_open()) {
            LOG_ERROR("[SerialPort] Failed to open device: {}", config_.device);
            return false;
        }

        // 应用串口配置
        if (!ApplyConfig()) {
            LOG_ERROR("[SerialPort] Failed to apply config for {}", name_);
            port_->close();
            port_ = nullptr;
            return false;
        }

        // 创建协议解析器
        protocol_ = CreateProtocol(config_.protocol, config_);
        if (!protocol_) {
            LOG_WARN("[SerialPort] Unknown protocol '{}' for {}, using raw mode",
                     config_.protocol, name_);
            protocol_ = std::make_unique<RawProtocol>();
        }

        LOG_INFO("[SerialPort] Opened {} on device {}", name_, config_.device);
        DoRead();
        return true;

    } catch (const asio::system_error& e) {
        LOG_ERROR("[SerialPort] Open {} failed: {}", name_, e.what());
        port_ = nullptr;
        return false;
    }
}

void SerialPort::Close() {
    if (port_ && port_->is_open()) {
        try {
            port_->cancel();   // 取消所有异步操作
            port_->close();
            LOG_INFO("[SerialPort] Closed {}", name_);
        } catch (const asio::system_error& e) {
            LOG_ERROR("[SerialPort] Close {} error: {}", name_, e.what());
        }
    }
    port_ = nullptr;
    protocol_ = nullptr;

    // 清空写队列
    std::lock_guard<std::mutex> lock(write_mutex_);
    while (!write_queue_.empty()) {
        write_queue_.pop();
    }
    writing_ = false;
}

bool SerialPort::IsOpen() const {
    return port_ && port_->is_open();
}

bool SerialPort::Send(const std::string& data) {
    if (!IsOpen()) {
        LOG_WARN("[SerialPort] Send failed: {} not open", name_);
        return false;
    }

    // 将数据加入写队列（线程安全）
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_queue_.push(data);
    }

    // 若当前未在写，则启动写流程
    // 注意：需在 IO 线程中调用 DoWrite，使用 post 确保线程安全
    asio::post(io_ctx_, [this]() {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (!writing_) {
            DoWrite();
        }
    });

    return true;
}

// ============================================================================
// 私有方法
// ============================================================================

void SerialPort::DoRead() {
    if (!IsOpen()) return;

    port_->async_read_some(
        asio::buffer(read_buf_.data(), read_buf_.size()),
        [this](const asio::error_code& ec, size_t bytes_transferred) {
            OnRead(ec, bytes_transferred);
        }
    );
}

void SerialPort::OnRead(const asio::error_code& ec, size_t bytes_transferred) {
    if (ec) {
        if (ec == asio::error::operation_aborted) {
            return;  // 正常取消（Close 时）
        }
        LOG_ERROR("[SerialPort] {} read error: {}", name_, ec.message());
        if (error_handler_) {
            error_handler_(name_, ec.message());
        }
        Close();
        return;
    }

    // 将原始数据交给协议解析器
    if (protocol_ && bytes_transferred > 0) {
        auto frames = protocol_->OnData(read_buf_.data(), bytes_transferred);
        for (const auto& frame : frames) {
            if (recv_handler_) {
                recv_handler_(name_, frame);
            }
        }
    }

    // 继续异步读取
    DoRead();
}

void SerialPort::DoWrite() {
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.empty() || !IsOpen()) {
            writing_ = false;
            return;
        }
        writing_ = true;
    }

    // 取队首数据（需在锁外，因为 async_write 需要 buffer 的生命周期）
    std::string data;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.empty()) {
            writing_ = false;
            return;
        }
        data = std::move(write_queue_.front());
        write_queue_.pop();
    }

    // 使用 shared_ptr 确保 data 在回调期间有效
    auto buf = std::make_shared<std::string>(std::move(data));

    asio::async_write(
        *port_,
        asio::buffer(buf->data(), buf->size()),
        [this, buf](const asio::error_code& ec, size_t bytes_transferred) {
            OnWrite(ec, bytes_transferred);
        }
    );
}

void SerialPort::OnWrite(const asio::error_code& ec, size_t bytes_transferred) {
    if (ec) {
        LOG_ERROR("[SerialPort] {} write error: {}", name_, ec.message());
        if (error_handler_) {
            error_handler_(name_, ec.message());
        }
        writing_ = false;
        Close();
        return;
    }

    LOG_DEBUG("[SerialPort] {} sent {} bytes", name_, bytes_transferred);

    // 继续发送队列中的下一条数据
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (!write_queue_.empty()) {
            DoWrite();  // 递归调用，但队列深度有限，不会栈溢出
        } else {
            writing_ = false;
        }
    }
}

bool SerialPort::ApplyConfig() {
    if (!port_ || !port_->is_open()) return false;

    try {
        // 波特率
        port_->set_option(asio::serial_port_base::baud_rate(config_.baud_rate));

        // 数据位
        port_->set_option(asio::serial_port_base::character_size(
            static_cast<uint8_t>(config_.data_bits)
        ));

        // 校验位
        asio::serial_port_base::parity parity_opt;
        switch (config_.parity) {
            case 'O': parity_opt = asio::serial_port_base::parity::odd;    break;
            case 'E': parity_opt = asio::serial_port_base::parity::even;    break;
            default:  parity_opt = asio::serial_port_base::parity::none;   break;
        }
        port_->set_option(parity_opt);

        // 停止位
        asio::serial_port_base::stop_bits stop_opt;
        if (config_.stop_bits == 2) {
            stop_opt = asio::serial_port_base::stop_bits::two;
        } else {
            stop_opt = asio::serial_port_base::stop_bits::one;
        }
        port_->set_option(stop_opt);

        return true;

    } catch (const asio::system_error& e) {
        LOG_ERROR("[SerialPort] ApplyConfig {} failed: {}", name_, e.what());
        return false;
    }
}

} // namespace serial
