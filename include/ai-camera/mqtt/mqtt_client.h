#ifndef AI_CAMERA_MQTT_MQTT_CLIENT_H
#define AI_CAMERA_MQTT_MQTT_CLIENT_H

#include "mqtt_types.h"

#include <asio.hpp>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace mqtt {

/// @brief MQTT 3.1.1 异步客户端
///
/// 特性：
///   - 基于 standalone Asio 实现，不依赖 Boost
///   - 自动重连（指数退避）
///   - LWT（遗嘱消息）
///   - Keep-Alive 心跳（PINGREQ/PINGRESP）
///   - QoS 0/1/2 支持
///
/// 线程模型：
///   - 所有网络操作在内部 io_context 线程执行
///   - 回调在 io_context 线程调用
///   - Publish() 可跨线程安全调用（内部通过 strand 串行化）
///
/// 用法示例：
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
///   auto client = std::make_shared<MqttClient>();
///   client->SetConfig({
///       "127.0.0.1", 1883, "my_client_id", "user", "pass"
///   });
///   client->SetOnMessage([](const std::string& topic,
///                           const std::string& payload,
///                           QoS qos, bool retain) {
///       std::cout << "RX: " << topic << " -> " << payload << "\n";
///   });
///   client->Connect();
///
///   // 发布消息（可在任意线程调用）
///   client->Publish("test/topic", "hello", QoS::AT_LEAST_ONCE);
///
///   // 订阅
///   client->Subscribe("test/topic", QoS::AT_MOST_ONCE);
///
///   // 断开连接
///   client->Disconnect();
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class MqttClient : public std::enable_shared_from_this<MqttClient> {
public:
    MqttClient();
    ~MqttClient();

    /// 设置连接配置（Connect() 前调用）
    void SetConfig(const MqttConfig& config);

    /// 设置回调
    void SetOnConnect(ConnectHandler h)    { on_connect_    = std::move(h); }
    void SetOnDisconnect(ConnectHandler h) { on_disconnect_ = std::move(h); }
    void SetOnMessage(MessageHandler h)     { on_message_    = std::move(h); }

    /// 发起连接（异步，立即返回）
    /// 连接结果通过 OnConnect 回调通知
    void Connect();

    /// 断开连接（发送 DISCONNECT 报文后关闭）
    void Disconnect();

    /// 是否已连接
    bool IsConnected() const { return connected_.load(); }

    /// 发布消息
    /// @param topic  主题
    /// @param payload 载荷（字符串）
    /// @param qos    QoS 等级
    /// @param retain 是否保留消息
    /// @return 成功入队返回 true
    bool Publish(const std::string& topic,
                 const std::string& payload,
                 QoS qos = QoS::AT_MOST_ONCE,
                 bool retain = false);

    /// 订阅主题
    /// @param topic 主题（支持通配符 # 和 +）
    /// @param qos  最大 QoS 等级
    void Subscribe(const std::string& topic, QoS qos = QoS::AT_MOST_ONCE);

    /// 取消订阅
    void Unsubscribe(const std::string& topic);

private:
    // ---- 内部类型 ----
    struct PendingPublish {
        std::string  topic;
        std::string  payload;
        QoS          qos;
        bool         retain;
        uint16_t     packet_id;
    };

    // ---- 连接与重连 ----
    void DoConnect();
    void ScheduleReconnect();
    void CancelReconnect();

    // ---- MQTT 报文读写 ----
    void DoReadHeader();
    void DoReadRemainingLength(
        const std::shared_ptr<std::vector<uint8_t>>& header_buf,
        const std::shared_ptr<std::vector<uint8_t>>& len_buf,
        std::size_t len_bytes_read);
    void DoReadRemaining(
        const std::shared_ptr<std::vector<uint8_t>>& packet,
        std::size_t offset,
        std::size_t remaining);
    void HandlePacket(const std::vector<uint8_t>& packet);

    // ---- 报文构建 ----
    std::vector<uint8_t> BuildConnectPacket() const;
    std::vector<uint8_t> BuildPublishPacket(const std::string& topic,
                                             const std::string& payload,
                                             QoS qos,
                                             bool retain,
                                             uint16_t packet_id);
    std::vector<uint8_t> BuildSubscribePacket(const std::string& topic, QoS qos);
    std::vector<uint8_t> BuildUnsubscribePacket(const std::string& topic);
    std::vector<uint8_t> BuildPingReqPacket() const;
    std::vector<uint8_t> BuildDisconnectPacket() const;
    std::vector<uint8_t> BuildPubAckPacket(uint16_t packet_id) const;
    std::vector<uint8_t> BuildPubRecPacket(uint16_t packet_id) const;
    std::vector<uint8_t> BuildPubRelPacket(uint16_t packet_id) const;
    std::vector<uint8_t> BuildPubCompPacket(uint16_t packet_id) const;

    // ---- 报文解析 ----
    void ParseConnAck(const std::vector<uint8_t>& data);
    void ParsePublish(const std::vector<uint8_t>& data);
    void ParsePubAck(const std::vector<uint8_t>& data);
    void ParsePubRec(const std::vector<uint8_t>& data);
    void ParsePubRel(const std::vector<uint8_t>& data);
    void ParsePubComp(const std::vector<uint8_t>& data);
    void ParseSubAck(const std::vector<uint8_t>& data);
    void ParseUnsubAck(const std::vector<uint8_t>& data);
    void ParsePingResp(const std::vector<uint8_t>& data);

    // ---- 工具 ----
    uint16_t AcquirePacketId();
    void SendPacket(const std::vector<uint8_t>& packet);
    void StartKeepAliveTimer();
    void OnKeepAliveTimer(const asio::error_code& ec);
    void NotifyConnect(bool connected);
    void CloseSocket();

    // ---- 成员 ----
    MqttConfig                    config_;
    asio::io_context              io_context_;
    std::unique_ptr<std::thread>  io_thread_;
    asio::ip::tcp::socket        socket_;
    asio::steady_timer             keep_alive_timer_;
    asio::steady_timer             reconnect_timer_;
    asio::any_io_executor         strand_;

    std::atomic<bool>             connected_{false};
    std::atomic<bool>             stopped_{false};
    std::atomic<uint16_t>         next_packet_id_{1};

    // 待重发报文（QoS 1/2）
    std::mutex                     pending_mutex_;
    std::map<uint16_t, PendingPublish> pending_publishes_;

    // 回调
    ConnectHandler                 on_connect_;
    ConnectHandler                 on_disconnect_;
    MessageHandler                 on_message_;

    // 读取缓冲
    std::vector<uint8_t>          read_buf_;
    static constexpr std::size_t  READ_BUF_SIZE = 4096;
};

} // namespace mqtt

#endif // AI_CAMERA_MQTT_MQTT_CLIENT_H
