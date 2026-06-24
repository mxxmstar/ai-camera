#include <iostream>
#include <thread>
#include <chrono>
#include "gb28181/gb28181_manager.h"
#include "gb28181/gb28181_config.h"

// 简单的 GB28181 模块测试程序
// 测试流程：
// 1. 初始化 Gb28181Manager
// 2. 启动 SIP 代理
// 3. 等待平台注册
// 4. 响应平台发来的请求（Catalog、DeviceInfo 等）
// 5. 按 Ctrl+C 退出

int main(int argc, char* argv[]) {
    std::cout << "==========================================" << std::endl;
    std::cout << "  GB28181 Module Test Program" << std::endl;
    std::cout << "==========================================" << std::endl;

    // 1. 配置 GB28181 参数
    gb28181::Gb28181Config config;
    config.device_id = "34020000001320000001";   // 20 位设备编码
    config.server_ip = "192.168.1.10";           // SIP 服务器 IP（请修改为实际地址）
    config.server_port = 5060;                    // SIP 服务器端口
    config.local_sip_port = 5060;                 // 本地监听端口
    config.sip_realm = "3402000000";             // SIP 域
    config.username = "34020000001320000001";    // 用户名
    config.password = "123456";                   // 密码（请修改为实际密码）
    config.expires = 3600;                        // 注册有效期（秒）
    config.keepalive_interval = 30;               // 保活间隔（秒）

    // 打印配置
    std::cout << "[Test] 配置信息:" << std::endl;
    std::cout << "  设备ID: " << config.device_id << std::endl;
    std::cout << "  服务器: " << config.server_ip << ":" << config.server_port << std::endl;
    std::cout << "  本地端口: " << config.local_sip_port << std::endl;
    std::cout << "  SIP 域: " << config.sip_realm << std::endl;

    // 2. 初始化 Gb28181Manager
    std::cout << "\n[Test] 初始化 Gb28181Manager..." << std::endl;
    gb28181::Gb28181Manager& manager = gb28181::Gb28181Manager::Instance();
    
    if (!manager.Init(config)) {
        std::cerr << "[Test] 初始化失败!" << std::endl;
        return -1;
    }
    std::cout << "[Test] 初始化成功" << std::endl;

    // 3. 启动 Gb28181Manager
    std::cout << "\n[Test] 启动 Gb28181Manager..." << std::endl;
    if (!manager.Start()) {
        std::cerr << "[Test] 启动失败!" << std::endl;
        return -1;
    }
    std::cout << "[Test] 启动成功，等待平台注册..." << std::endl;

    // 4. 主循环（等待退出）
    std::cout << "\n[Test] 按 Enter 退出..." << std::endl;
    
    // 等待注册完成（简单轮询）
    for (int i = 0; i < 10; i++) {
        if (manager.IsRegistered()) {
            std::cout << "[Test] 已注册到平台!" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "[Test] 等待注册... (" << (i + 1) << "s)" << std::endl;
    }
    
    if (!manager.IsRegistered()) {
        std::cout << "[Test] 警告：尚未注册到平台，请检查网络和配置" << std::endl;
    }

    // 5. 等待用户退出
    std::cin.get();

    // 6. 停止
    std::cout << "\n[Test] 停止 Gb28181Manager..." << std::endl;
    manager.Stop();
    std::cout << "[Test] 已停止" << std::endl;

    return 0;
}
