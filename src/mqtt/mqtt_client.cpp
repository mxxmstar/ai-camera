#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_types.h"

#include <asio.hpp>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace mqtt {

// ============================================================
// 工具：MQTT 剩余长度编码（变长整数，最多 4 字节）
// ============================================================
static std::vector<uint8_t> EncodeRemainingLength(uint32_t len) {
    std::vector<uint8_t> out;
    do {
        uint8_t byte = len % 128;
        len /= 128;
        if (len > 0) byte |= 0x80;
        out.push_back(byte);
    } while (len > 0);
    return out;
}

// ============================================================
// 工具：MQTT 剩余长度解码
// ============================================================
static uint32_t DecodeRemainingLength(const uint8_t* data, std::size_t max_len, std::size_t& consumed) {
    uint32_t multiplier = 1;
    uint32_t value = 0;
    std::size_t i = 0;
    uint8_t byte;
    do {
        if (i >= max_len) { consumed = 0; return 0; }
        byte = data[i++];
        value += (byte & 0x7F) * multiplier;
        multiplier *= 128;
        if (multiplier > 128 * 128 * 128) { consumed = 0; return 0; } // 格式错误
    } while (byte & 0x80);
    consumed = i;
    return value;
}

// ============================================================
// 工具：写入 UTF-8 字符串（2 字节长度 + 内容）
// ============================================================
static void WriteUtf8(std::vector<uint8_t>& out, const std::string& s) {
    uint16_t len = static_cast<uint16_t>(s.size());
    out.push_back(static_cast<uint8_t>(len >> 8));
    out.push_back(static_cast<uint8_t>(len & 0xFF));
    out.insert(out.end(), s.begin(), s.end());
}

// ============================================================
// 构造函数的实现
// ============================================================
MqttClient::MqttClient()
    : io_context_()
    , socket_(io_context_)
    , keep_alive_timer_(io_context_)
    , reconnect_timer_(io_context_)
    , strand_(asio::make_strand(io_context_))
{
    read_buf_.resize(READ_BUF_SIZE);
}

MqttClient::~MqttClient() {
    stopped_ = true;
    CancelReconnect();
    CloseSocket();
    if (io_thread_ && io_thread_->joinable()) {
        io_context_.stop();
        io_thread_->join();
    }
}

// ============================================================
// SetConfig：注入连接配置
// ============================================================
void MqttClient::SetConfig(const MqttConfig& config) {
    config_ = config;
    if (config_.client_id.empty()) {
        // 自动生成 client_id
        std::ostringstream oss;
        std::random_device rd;
        std::mt19937 gen(rd());
        oss << "AICAM-" << std::hex << std::setw(8) << std::setfill('0')
            << std::uniform_int_distribution<uint32_t>(0, 0xFFFFFFFF)(gen);
        config_.client_id = oss.str();
    }
    if (config_.will_topic.empty()) {
        config_.will_topic = "aicamera/" + config_.device_id + "/status";
    }
    if (config_.will_payload.empty()) {
        config_.will_payload = R"({"device_id":")" + config_.device_id + R"(","status":"offline"})";
    }
}

// ============================================================
// Connect：启动重连循环
// ============================================================
void MqttClient::Connect() {
    stopped_ = false;
    // 启动内部 io_context 线程（如果没有运行）
    if (!io_thread_) {
        io_thread_ = std::make_unique<std::thread>([this]() {
            io_context_.run();
        });
    }
    // 在 io_context 线程中执行连接
    asio::post(io_context_, [this]() {
        DoConnect();
    });
}

// ============================================================
// DoConnect：解析地址并发起 TCP 连接
// ============================================================
void MqttClient::DoConnect() {
    asio::error_code ec;
    asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(config_.broker_host, std::to_string(config_.broker_port), ec);
    if (ec) {
        std::cerr << "[MQTT] DNS resolution failed: " << ec.message() << std::endl;
        ScheduleReconnect();
        return;
    }

    asio::async_connect(
        socket_,
        endpoints,
        [this](const asio::error_code& ec, const asio::ip::tcp::endpoint&) {
            if (ec) {
                std::cerr << "[MQTT] TCP connection failed: " << ec.message() << std::endl;
                ScheduleReconnect();
                return;
            }
            std::cout << "[MQTT] TCP connected: " << config_.broker_host
                      << ":" << config_.broker_port << std::endl;
            // 发送 CONNECT 报文
            auto packet = BuildConnectPacket();
            SendPacket(packet);
            // 等待 CONNACK
            DoReadHeader();
        }
    );
}

// ============================================================
// Disconnect：发送 DISCONNECT 后关闭
// ============================================================
void MqttClient::Disconnect() {
    stopped_ = true;
    CancelReconnect();
    asio::post(strand_, [this]() {
        if (connected_) {
            auto packet = BuildDisconnectPacket();
            SendPacket(packet);
        }
        CloseSocket();
        NotifyConnect(false);
    });
}

// ============================================================
// Publish：发布消息（线程安全）
// ============================================================
bool MqttClient::Publish(const std::string& topic, const std::string& payload, QoS qos, bool retain) {
    if (!connected_ && qos == QoS::AT_MOST_ONCE) return false;

    asio::post(strand_, [this, topic, payload, qos, retain]() {
        uint16_t packet_id = (qos != QoS::AT_MOST_ONCE) ? AcquirePacketId() : 0;
        auto packet = BuildPublishPacket(topic, payload, qos, retain, packet_id);
        SendPacket(packet);

        // QoS 1/2 需要等待确认，暂存 pending
        if (qos != QoS::AT_MOST_ONCE) {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_publishes_[packet_id] = {topic, payload, qos, retain, packet_id};
        }
    });
    return true;
}

// ============================================================
// Subscribe：订阅主题
// ============================================================
void MqttClient::Subscribe(const std::string& topic, QoS qos) {
    asio::post(strand_, [this, topic, qos]() {
        auto packet = BuildSubscribePacket(topic, qos);
        SendPacket(packet);
    });
}

// ============================================================
// Unsubscribe：取消订阅
// ============================================================
void MqttClient::Unsubscribe(const std::string& topic) {
    asio::post(strand_, [this, topic]() {
        auto packet = BuildUnsubscribePacket(topic);
        SendPacket(packet);
    });
}

// ============================================================
// DoReadHeader：读取 1 字节固定头 + 剩余长度
// ============================================================
void MqttClient::DoReadHeader() {
    auto buf = std::make_shared<std::vector<uint8_t>>();
    buf->resize(1);  // 先读 1 字节固定头

    std::shared_ptr<MqttClient> self = shared_from_this();
    asio::async_read(
        socket_,
        asio::buffer(*buf),
        [this, self, buf](const asio::error_code& ec, std::size_t) {
            if (ec) {
                std::cerr << "[MQTT] Read failed: " << ec.message() << std::endl;
                NotifyConnect(false);
                ScheduleReconnect();
                return;
            }
            // 继续读取剩余长度（将固定头也传递过去）
            auto len_buf = std::make_shared<std::vector<uint8_t>>();
            len_buf->resize(1);
            DoReadRemainingLength(buf, len_buf, 0);
        }
    );
}

void MqttClient::DoReadRemainingLength(
    const std::shared_ptr<std::vector<uint8_t>>& header_buf,
    const std::shared_ptr<std::vector<uint8_t>>& len_buf,
    std::size_t len_bytes_read)
{
    std::shared_ptr<MqttClient> self = shared_from_this();
    asio::async_read(
        socket_,
        asio::buffer(*len_buf),
        [this, self, header_buf, len_buf, len_bytes_read](const asio::error_code& ec, std::size_t) {
            if (ec) {
                std::cerr << "[MQTT] Failed to read remaining length: " << ec.message() << std::endl;
                NotifyConnect(false);
                ScheduleReconnect();
                return;
            }
            header_buf->push_back((*len_buf)[0]);
            if ((*len_buf)[0] & 0x80) {
                // 还有后续字节
                if (len_bytes_read + 1 >= 4) {
                    std::cerr << "[MQTT] Remaining length format error" << std::endl;
                    NotifyConnect(false);
                    ScheduleReconnect();
                    return;
                }
                len_buf->resize(1);
                DoReadRemainingLength(header_buf, len_buf, len_bytes_read + 1);
            } else {
                // 剩余长度读取完成，解析总长度
                std::size_t consumed = 0;
                uint32_t remaining_len = DecodeRemainingLength(
                    header_buf->data() + 1,
                    header_buf->size() - 1,
                    consumed
                );
                if (consumed == 0) {
                    std::cerr << "[MQTT] Remaining length decode failed" << std::endl;
                    NotifyConnect(false);
                    ScheduleReconnect();
                    return;
                }
                // 读取剩余载荷
                auto full_packet = std::make_shared<std::vector<uint8_t>>(*header_buf);
                full_packet->resize(header_buf->size() + remaining_len);
                if (remaining_len > 0) {
                    DoReadRemaining(full_packet, header_buf->size(), remaining_len);
                } else {
                    // 无载荷，直接处理
                    HandlePacket(*full_packet);
                    DoReadHeader();
                }
            }
        }
    );
}

void MqttClient::DoReadRemaining(
    const std::shared_ptr<std::vector<uint8_t>>& packet,
    std::size_t offset,
    std::size_t remaining)
{
    auto self = std::static_pointer_cast<MqttClient>(shared_from_this());
    asio::async_read(
        socket_,
        asio::buffer(packet->data() + offset, remaining),
        [this, self, packet, offset, remaining](const asio::error_code& ec, std::size_t n) {
            if (ec) {
                std::cerr << "[MQTT] Failed to read payload: " << ec.message() << std::endl;
                NotifyConnect(false);
                ScheduleReconnect();
                return;
            }
            if (n < remaining) {
                DoReadRemaining(packet, offset + n, remaining - n);
                return;
            }
            HandlePacket(*packet);
            DoReadHeader();
        }
    );
}

// ============================================================
// HandlePacket：根据报文类型分发处理
// ============================================================
void MqttClient::HandlePacket(const std::vector<uint8_t>& packet) {
    if (packet.empty()) return;
    uint8_t byte0 = packet[0];
    PacketType type = static_cast<PacketType>((byte0 >> 4) & 0x0F);
    switch (type) {
        case PacketType::CONNACK:    ParseConnAck(packet);   break;
        case PacketType::PUBLISH:    ParsePublish(packet);   break;
        case PacketType::PUBACK:     ParsePubAck(packet);    break;
        case PacketType::PUBREC:    ParsePubRec(packet);    break;
        case PacketType::PUBREL:    ParsePubRel(packet);    break;
        case PacketType::PUBCOMP:   ParsePubComp(packet);   break;
        case PacketType::SUBACK:     ParseSubAck(packet);    break;
        case PacketType::UNSUBACK:  ParseUnsubAck(packet); break;
        case PacketType::PINGRESP:  ParsePingResp(packet);  break;
        default:
            std::cerr << "[MQTT] Unknown packet type: " << static_cast<int>(type) << std::endl;
            break;
    }
}

// ============================================================
// 报文构建：CONNECT
// ============================================================
std::vector<uint8_t> MqttClient::BuildConnectPacket() const {
    std::vector<uint8_t> payload;

    // 协议名 "MQTT"（UTF-8 字符串）
    WriteUtf8(payload, PROTOCOL_NAME);
    // 协议等级 4（MQTT 3.1.1）
    payload.push_back(PROTOCOL_LEVEL_3_1_1);

    // 连接标志
    uint8_t connect_flags = 0;
    if (config_.clean_session)  connect_flags |= 0x02;
    if (!config_.username.empty()) connect_flags |= 0x80;
    if (!config_.password.empty()) connect_flags |= 0x40;
    if (!config_.will_topic.empty()) {
        connect_flags |= 0x04;  // Will Flag
        connect_flags |= (static_cast<uint8_t>(config_.will_qos) & 0x03) << 3;
        if (config_.will_retain) connect_flags |= 0x20;
    }
    payload.push_back(connect_flags);

    // Keep Alive
    uint16_t ka = config_.keep_alive_seconds;
    payload.push_back(static_cast<uint8_t>(ka >> 8));
    payload.push_back(static_cast<uint8_t>(ka & 0xFF));

    // 客户端 ID（必填）
    WriteUtf8(payload, config_.client_id);

    // Will Topic / Payload
    if (!config_.will_topic.empty()) {
        WriteUtf8(payload, config_.will_topic);
        WriteUtf8(payload, config_.will_payload);
    }

    // Username / Password
    if (!config_.username.empty()) {
        WriteUtf8(payload, config_.username);
        if (!config_.password.empty()) {
            WriteUtf8(payload, config_.password);
        }
    }

    // 构建完整报文
    std::vector<uint8_t> packet;
    // 固定头：Packet Type = CONNECT (1)，Flags = 0
    packet.push_back(static_cast<uint8_t>(PacketType::CONNECT) << 4);
    // 剩余长度
    auto remaining = EncodeRemainingLength(static_cast<uint32_t>(payload.size()));
    packet.insert(packet.end(), remaining.begin(), remaining.end());
    // 载荷
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;
}

// ============================================================
// 报文构建：PUBLISH
// ============================================================
std::vector<uint8_t> MqttClient::BuildPublishPacket(
    const std::string& topic,
    const std::string& payload,
    QoS qos,
    bool retain,
    uint16_t packet_id)
{
    std::vector<uint8_t> packet;

    // 固定头
    uint8_t byte0 = static_cast<uint8_t>(PacketType::PUBLISH) << 4;
    if (retain) byte0 |= 0x01;
    byte0 |= (static_cast<uint8_t>(qos) & 0x03) << 1;
    packet.push_back(byte0);

    // 可变头 + 载荷
    std::vector<uint8_t> variable;
    WriteUtf8(variable, topic);
    if (qos != QoS::AT_MOST_ONCE) {
        variable.push_back(static_cast<uint8_t>(packet_id >> 8));
        variable.push_back(static_cast<uint8_t>(packet_id & 0xFF));
    }
    variable.insert(variable.end(), payload.begin(), payload.end());

    // 剩余长度
    auto remaining = EncodeRemainingLength(static_cast<uint32_t>(variable.size()));
    packet.insert(packet.end(), remaining.begin(), remaining.end());
    packet.insert(packet.end(), variable.begin(), variable.end());

    return packet;
}

// ============================================================
// 报文构建：SUBSCRIBE
// ============================================================
std::vector<uint8_t> MqttClient::BuildSubscribePacket(const std::string& topic, QoS qos) {
    std::vector<uint8_t> packet;

    // 固定头：SUBSCRIBE (8)，Flags = 0010（固定）
    packet.push_back((static_cast<uint8_t>(PacketType::SUBSCRIBE) << 4) | 0x02);

    // 可变头 + 载荷
    std::vector<uint8_t> variable;
    uint16_t pid = AcquirePacketId();
    variable.push_back(static_cast<uint8_t>(pid >> 8));
    variable.push_back(static_cast<uint8_t>(pid & 0xFF));
    WriteUtf8(variable, topic);
    variable.push_back(static_cast<uint8_t>(qos) & 0x03);

    auto remaining = EncodeRemainingLength(static_cast<uint32_t>(variable.size()));
    packet.insert(packet.end(), remaining.begin(), remaining.end());
    packet.insert(packet.end(), variable.begin(), variable.end());

    return packet;
}

// ============================================================
// 报文构建：UNSUBSCRIBE
// ============================================================
std::vector<uint8_t> MqttClient::BuildUnsubscribePacket(const std::string& topic) {
    std::vector<uint8_t> packet;

    // 固定头：UNSUBSCRIBE (10)，Flags = 0010（固定）
    packet.push_back((static_cast<uint8_t>(PacketType::UNSUBSCRIBE) << 4) | 0x02);

    std::vector<uint8_t> variable;
    uint16_t pid = AcquirePacketId();
    variable.push_back(static_cast<uint8_t>(pid >> 8));
    variable.push_back(static_cast<uint8_t>(pid & 0xFF));
    WriteUtf8(variable, topic);

    auto remaining = EncodeRemainingLength(static_cast<uint32_t>(variable.size()));
    packet.insert(packet.end(), remaining.begin(), remaining.end());
    packet.insert(packet.end(), variable.begin(), variable.end());

    return packet;
}

// ============================================================
// 报文构建：PINGREQ
// ============================================================
std::vector<uint8_t> MqttClient::BuildPingReqPacket() const {
    return { static_cast<uint8_t>(PacketType::PINGREQ) << 4, 0x00 };
}

// ============================================================
// 报文构建：DISCONNECT
// ============================================================
std::vector<uint8_t> MqttClient::BuildDisconnectPacket() const {
    return { static_cast<uint8_t>(PacketType::DISCONNECT) << 4, 0x00 };
}

// ============================================================
// 报文构建：PUBACK / PUBREC / PUBREL / PUBCOMP
// ============================================================
std::vector<uint8_t> MqttClient::BuildPubAckPacket(uint16_t packet_id) const {
    return {
        static_cast<uint8_t>(PacketType::PUBACK) << 4,
        0x02,
        static_cast<uint8_t>(packet_id >> 8),
        static_cast<uint8_t>(packet_id & 0xFF)
    };
}

std::vector<uint8_t> MqttClient::BuildPubRecPacket(uint16_t packet_id) const {
    return {
        static_cast<uint8_t>(PacketType::PUBREC) << 4,
        0x02,
        static_cast<uint8_t>(packet_id >> 8),
        static_cast<uint8_t>(packet_id & 0xFF)
    };
}

std::vector<uint8_t> MqttClient::BuildPubRelPacket(uint16_t packet_id) const {
    return {
        static_cast<uint8_t>(PacketType::PUBREL) << 4 | 0x02,
        0x02,
        static_cast<uint8_t>(packet_id >> 8),
        static_cast<uint8_t>(packet_id & 0xFF)
    };
}

std::vector<uint8_t> MqttClient::BuildPubCompPacket(uint16_t packet_id) const {
    return {
        static_cast<uint8_t>(PacketType::PUBCOMP) << 4,
        0x02,
        static_cast<uint8_t>(packet_id >> 8),
        static_cast<uint8_t>(packet_id & 0xFF)
    };
}

// ============================================================
// 报文解析：CONNACK
// ============================================================
void MqttClient::ParseConnAck(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;
    uint8_t flags = data[2];
    uint8_t rc    = data[3];
    bool session_present = (flags & 0x01) != 0;
    auto return_code = static_cast<ConnectReturnCode>(rc);

    std::cout << "[MQTT] CONNACK: session_present=" << session_present
              << ", return_code=" << static_cast<int>(return_code) << std::endl;

    if (return_code == ConnectReturnCode::ACCEPTED) {
        connected_ = true;
        NotifyConnect(true);
        StartKeepAliveTimer();

        // 自动重新订阅（如果 clean_session = false，服务端会保留订阅）
        if (config_.clean_session) {
            // 这里可以订阅默认 Topic，但由 MqttManager 管理
        }
    } else {
        std::cerr << "[MQTT] Connection refused, return code: " << static_cast<int>(return_code) << std::endl;
        CloseSocket();
        if (return_code != ConnectReturnCode::BAD_PROTOCOL_VERSION &&
            return_code != ConnectReturnCode::BAD_CLIENT_ID) {
            ScheduleReconnect();
        }
    }
}

// ============================================================
// 报文解析：PUBLISH
// ============================================================
void MqttClient::ParsePublish(const std::vector<uint8_t>& data) {
    if (data.empty()) return;
    uint8_t byte0 = data[0];
    QoS qos = static_cast<QoS>((byte0 >> 1) & 0x03);
    bool retain = (byte0 & 0x01) != 0;
    bool dup   = (byte0 & 0x08) != 0;

    std::size_t offset = 1;
    // 解码剩余长度
    std::size_t consumed = 0;
    uint32_t remaining_len = DecodeRemainingLength(data.data() + offset, data.size() - offset, consumed);
    if (consumed == 0) return;
    offset += consumed;

    // 主题名（UTF-8 字符串）
    if (offset + 2 > data.size()) return;
    uint16_t topic_len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    if (offset + topic_len > data.size()) return;
    std::string topic(data.begin() + offset, data.begin() + offset + topic_len);
    offset += topic_len;

    // 报文标识符（QoS > 0 时存在）
    uint16_t packet_id = 0;
    if (qos != QoS::AT_MOST_ONCE) {
        if (offset + 2 > data.size()) return;
        packet_id = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
    }

    // 载荷
    std::string payload(data.begin() + offset, data.end());

    // QoS 确认响应
    if (qos == QoS::AT_LEAST_ONCE) {
        auto ack = BuildPubAckPacket(packet_id);
        SendPacket(ack);
    } else if (qos == QoS::EXACTLY_ONCE) {
        auto rec = BuildPubRecPacket(packet_id);
        SendPacket(rec);
    }

    // 回调
    if (on_message_) {
        on_message_(topic, payload, qos, retain);
    }
}

// ============================================================
// 报文解析：PUBACK / PUBREC / PUBREL / PUBCOMP
// ============================================================
void MqttClient::ParsePubAck(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;
    uint16_t pid = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_publishes_.erase(pid);
    std::cout << "[MQTT] PUBACK received, packet_id=" << pid << std::endl;
}

void MqttClient::ParsePubRec(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;
    uint16_t pid = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    auto rel = BuildPubRelPacket(pid);
    SendPacket(rel);
}

void MqttClient::ParsePubRel(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;
    uint16_t pid = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    auto comp = BuildPubCompPacket(pid);
    SendPacket(comp);
}

void MqttClient::ParsePubComp(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;
    uint16_t pid = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_publishes_.erase(pid);
    std::cout << "[MQTT] PUBCOMP received, packet_id=" << pid << std::endl;
}

// ============================================================
// 报文解析：SUBACK
// ============================================================
void MqttClient::ParseSubAck(const std::vector<uint8_t>& data) {
    if (data.size() < 5) return;
    uint16_t pid = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    uint8_t  rc = data[4];
    std::cout << "[MQTT] SUBACK: packet_id=" << pid << ", return_code=" << static_cast<int>(rc) << std::endl;
}

// ============================================================
// 报文解析：UNSUBACK
// ============================================================
void MqttClient::ParseUnsubAck(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;
    uint16_t pid = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    std::cout << "[MQTT] UNSUBACK: packet_id=" << pid << std::endl;
}

// ============================================================
// 报文解析：PINGRESP
// ============================================================
void MqttClient::ParsePingResp(const std::vector<uint8_t>& data) {
    std::cout << "[MQTT] PINGRESP received" << std::endl;
}

// ============================================================
// 重连管理：指数退避
// ============================================================
void MqttClient::ScheduleReconnect() {
    if (stopped_) return;
    auto interval = config_.reconnect_interval;
    // 指数退避（简单实现：每次重连失败翻倍，上限 max_reconnect_interval）
    // 实际项目中可以用计数器，这里简化
    reconnect_timer_.expires_after(interval);
    reconnect_timer_.async_wait([this](const asio::error_code& ec) {
        if (ec || stopped_) return;
        std::cout << "[MQTT] Attempting to reconnect..." << std::endl;
        DoConnect();
    });
}

void MqttClient::CancelReconnect() {
    reconnect_timer_.cancel();
}

// ============================================================
// Keep Alive 管理
// ============================================================
void MqttClient::StartKeepAliveTimer() {
    if (config_.keep_alive_seconds == 0) return;
    keep_alive_timer_.expires_after(std::chrono::seconds(config_.keep_alive_seconds));
    keep_alive_timer_.async_wait([this](const asio::error_code& ec) {
        if (ec || !connected_) return;
        auto ping = BuildPingReqPacket();
        SendPacket(ping);
        StartKeepAliveTimer();
    });
}

// ============================================================
// 工具函数
// ============================================================
uint16_t MqttClient::AcquirePacketId() {
    uint16_t id = next_packet_id_.fetch_add(1);
    if (id == 0) id = next_packet_id_.fetch_add(1);
    return id;
}

void MqttClient::SendPacket(const std::vector<uint8_t>& packet) {
    asio::error_code ec;
    asio::write(socket_, asio::buffer(packet), ec);
    if (ec) {
        std::cerr << "[MQTT] Send failed: " << ec.message() << std::endl;
    }
}

void MqttClient::NotifyConnect(bool connected) {
    if (connected && on_connect_) {
        on_connect_(true);
    } else if (!connected && on_disconnect_) {
        on_disconnect_(false);
    }
}

void MqttClient::CloseSocket() {
    asio::error_code ec;
    socket_.close(ec);
    connected_ = false;
}

} // namespace mqtt
