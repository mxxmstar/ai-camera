/**
 * @file test_mdns.cpp
 * @brief mDNS 模块测试程序
 * 
 * 功能：
 *   - 测试服务注册
 *   - 测试服务发现
 *   - 测试服务解析
 * 
 * 编译：
 *   mkdir build && cd build
 *   cmake ..
 *   cmake --build . --target test_mdns
 * 
 * 运行：
 *   ./test_mdns
 */

#include "mdns/mdns_service.h"
#include "mdns/mdns_types.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace mdns;

// ============================================================
// 测试服务注册
// ============================================================

void TestServiceRegistration() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test: Service Registration" << std::endl;
    std::cout << "========================================\n" << std::endl;

    auto mdns = MdnsService::Instance();

    // 初始化
    if (!mdns->Init("ai-camera")) {
        std::cerr << "[ERROR] Failed to initialize mDNS" << std::endl;
        return;
    }

    std::cout << "[INFO] mDNS initialized successfully" << std::endl;
    std::cout << "[INFO] Host name: " << mdns->GetHostName() << std::endl;

    // 注册 RTSP 服务
    ServiceInfo rtsp_service;
    rtsp_service.name = "AI-Camera-001";
    rtsp_service.type = ServiceType::RTSP;
    rtsp_service.port = 554;
    rtsp_service.txt_records = {
        {"path", "/live/0"},
        {"resolution", "1920x1080"},
        {"codec", "H.264"},
        {"fps", "30"}
    };

    if (mdns->RegisterService(rtsp_service)) {
        std::cout << "[INFO] RTSP service registered successfully" << std::endl;
    } else {
        std::cerr << "[ERROR] Failed to register RTSP service" << std::endl;
    }

    // 注册 HTTP API 服务
    ServiceInfo http_service;
    http_service.name = "AI-Camera-001";
    http_service.type = ServiceType::HTTP;
    http_service.port = 8080;
    http_service.txt_records = {
        {"api_version", "v1"},
        {"model", "AI-Cam-1.0"},
        {"firmware", "1.0.0"}
    };

    if (mdns->RegisterService(http_service)) {
        std::cout << "[INFO] HTTP service registered successfully" << std::endl;
    } else {
        std::cerr << "[ERROR] Failed to register HTTP service" << std::endl;
    }

    // 查询已注册的服务
    auto registered = mdns->GetRegisteredServices();
    std::cout << "\n[INFO] Registered services (" << registered.size() << "):" << std::endl;
    for (const auto& svc : registered) {
        std::cout << "  - " << svc.name << " (" << ServiceTypeToString(svc.type) 
                  << ") on port " << svc.port << std::endl;
    }

    // 等待一段时间，让服务被网络中的其他设备发现
    std::cout << "\n[INFO] Waiting for 10 seconds..." << std::endl;
    std::cout << "[INFO] Other devices should be able to discover this camera now" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 注销服务
    mdns->UnregisterService("AI-Camera-001", ServiceType::RTSP);
    mdns->UnregisterService("AI-Camera-001", ServiceType::HTTP);

    std::cout << "\n[INFO] Services unregistered" << std::endl;

    // 关闭
    mdns->Shutdown();
    std::cout << "[INFO] mDNS shutdown complete" << std::endl;
}

// ============================================================
// 测试服务发现
// ============================================================

void TestServiceDiscovery() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test: Service Discovery" << std::endl;
    std::cout << "========================================\n" << std::endl;

    auto mdns = MdnsService::Instance();

    // 初始化
    if (!mdns->Init("test-client")) {
        std::cerr << "[ERROR] Failed to initialize mDNS" << std::endl;
        return;
    }

    std::cout << "[INFO] mDNS initialized successfully" << std::endl;
    std::cout << "[INFO] Host name: " << mdns->GetHostName() << std::endl;

    // 开始浏览 RTSP 服务
    std::cout << "\n[INFO] Browsing for RTSP services..." << std::endl;

    int browse_id = mdns->StartBrowse(
        ServiceType::RTSP,
        [](const ServiceInfo& service) {
            std::cout << "\n[FOUND] Service discovered:" << std::endl;
            std::cout << "  Name: " << service.name << std::endl;
            std::cout << "  Type: " << ServiceTypeToString(service.type) << std::endl;
            std::cout << "  Port: " << service.port << std::endl;
            std::cout << "  Host: " << service.host << std::endl;
            
            // 打印 TXT 记录
            if (!service.txt_records.empty()) {
                std::cout << "  TXT Records:" << std::endl;
                for (const auto& [key, value] : service.txt_records) {
                    std::cout << "    " << key << " = " << value << std::endl;
                }
            }
        },
        [](const std::string& service_name) {
            std::cout << "\n[LOST] Service lost: " << service_name << std::endl;
        }
    );

    if (browse_id >= 0) {
        std::cout << "[INFO] Browse started (browse_id=" << browse_id << ")" << std::endl;
    } else {
        std::cerr << "[ERROR] Failed to start browse" << std::endl;
        return;
    }

    // 等待一段时间，发现网络中的服务
    std::cout << "[INFO] Waiting for 30 seconds to discover services..." << std::endl;
    std::cout << "[INFO] Make sure other mDNS services are running in the network" << std::endl;
    
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "." << std::flush;
        
        // 每 5 秒打印一次已发现的服务
        if ((i + 1) % 5 == 0) {
            std::cout << std::endl;
            auto discovered = mdns->GetDiscoveredServices(ServiceType::RTSP);
            std::cout << "[INFO] Discovered " << discovered.size() << " RTSP service(s)" << std::endl;
        }
    }

    std::cout << std::endl;

    // 停止浏览
    mdns->StopBrowse(browse_id);
    std::cout << "[INFO] Browse stopped" << std::endl;

    // 关闭
    mdns->Shutdown();
    std::cout << "[INFO] mDNS shutdown complete" << std::endl;
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "       mDNS Module Test Program         " << std::endl;
    std::cout << "========================================" << std::endl;

    if (argc < 2) {
        std::cout << "\nUsage: test_mdns <mode>" << std::endl;
        std::cout << "  Modes:" << std::endl;
        std::cout << "    register  - Test service registration (act as server)" << std::endl;
        std::cout << "    discover  - Test service discovery (act as client)" << std::endl;
        std::cout << "\nExample:" << std::endl;
        std::cout << "  test_mdns register  # Run on camera device" << std::endl;
        std::cout << "  test_mdns discover  # Run on client device" << std::endl;
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "register") {
        TestServiceRegistration();
    } else if (mode == "discover") {
        TestServiceDiscovery();
    } else {
        std::cerr << "[ERROR] Unknown mode: " << mode << std::endl;
        std::cerr << "[ERROR] Use 'register' or 'discover'" << std::endl;
        return 1;
    }

    return 0;
}
