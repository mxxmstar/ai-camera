#include "mqtt/mqtt_client.h"
#include "mqtt/mqtt_manager.h"

#include <chrono>
#include <csignal>
#include <iostream>
#include <random>
#include <string>
#include <thread>

static volatile bool g_running = true;

static void signal_handler(int /*signum*/) {
    std::cout << "\n[Test] Caught signal, shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ============================================================
    // 配置 MQTT 连接
    // ============================================================
    mqtt::MqttConfig config;
    config.broker_host   = "127.0.0.1";
    config.broker_port   = 1883;
    // 生成唯一 client_id
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    config.client_id     = "ai-camera-test-" + std::to_string(dis(gen));
    config.username      = "";
    config.password      = "";
    config.clean_session = true;
    config.keep_alive_seconds = 60;
    config.device_id     = "AICAM-TEST";

    // ============================================================
    // 初始化 MqttManager
    // ============================================================
    auto& mgr = mqtt::MqttManager::Instance();
    mgr.SetConfig(config);

    // 设置回调
    mgr.SetOnConnect([](bool connected) {
        if (connected) {
            std::cout << "[Test] MQTT connected!" << std::endl;
        } else {
            std::cout << "[Test] MQTT disconnected!" << std::endl;
        }
    });

    mgr.SetOnCommand([](const std::string& msg_id,
                         const std::string& cmd,
                         const std::string& params) {
        std::cout << "[Test] Received command: " << cmd
                  << " (msg_id=" << msg_id << ")" << std::endl;
        std::cout << "[Test] Params: " << params << std::endl;
    });

    mgr.SetOnOtaNotify([](const std::string& version,
                           const std::string& url,
                           const std::string& md5,
                           uint64_t size) {
        std::cout << "[Test] OTA notify: version=" << version
                  << ", url=" << url << ", size=" << size << std::endl;
    });

    // ============================================================
    // 启动 MQTT 客户端
    // ============================================================
    if (!mgr.Start()) {
        std::cerr << "[Test] Failed to start MQTT client!" << std::endl;
        return 1;
    }

    std::cout << "[Test] MQTT client started, connecting to "
              << config.broker_host << ":" << config.broker_port << std::endl;
    std::cout << "[Test] Press Ctrl+C to exit" << std::endl;

    // ============================================================
    // 主循环：定时发布消息
    // ============================================================
    int count = 0;
    while (g_running) {
        if (mgr.IsConnected()) {
            // 上报属性
            if (count % 5 == 0) {
                std::string json_data = R"({"firmware_version":"v1.2.3","ip":"192.168.1.200","temperature":)" + std::to_string(25.0 + count * 0.1) + R"(})";
                mgr.PublishProperty(json_data);
                std::cout << "[Test] Published property: " << json_data << std::endl;
            }

            // 上报事件
            if (count % 10 == 0) {
                std::string event_data = R"({"confidence":0.95,"bbox":[100,200,300,400]})";
                mgr.PublishEvent("person_detect", event_data);
                std::cout << "[Test] Published event: person_detect" << std::endl;
            }

            ++count;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // ============================================================
    // 停止 MQTT 客户端
    // ============================================================
    std::cout << "[Test] Stopping MQTT client..." << std::endl;
    mgr.Stop();

    std::cout << "[Test] Test completed!" << std::endl;
    return 0;
}
