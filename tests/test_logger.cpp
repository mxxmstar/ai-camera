/**
 * @file test_logger.cpp
 * @brief 日志模块测试程序
 * 
 * 编译：
 *   cd e:\project\ai-camera\build
 *   cmake --build . --config Debug --target test_logger
 * 
 * 运行：
 *   .\Debug\test_logger.exe
 */

#include "log/logger.h"

#include <iostream>
#include <thread>
#include <vector>

void TestBasicLogging() {
    std::cout << "\n=== 基础日志测试 ===" << std::endl;
    
    LOG_TRACE("This is a trace message");
    LOG_DEBUG("This is a debug message");
    LOG_INFO("This is an info message");
    LOG_WARN("This is a warning message");
    LOG_ERROR("This is an error message");
    LOG_CRITICAL("This is a critical message");
}

void TestFormattedLogging() {
    std::cout << "\n=== 格式化日志测试 ===" << std::endl;
    
    int value = 42;
    double pi = 3.1415926;
    std::string name = "AI Camera";
    
    LOG_INFO("Integer: {}, Float: {:.2f}, String: {}", value, pi, name);
    LOG_INFO("Hex: {0:x}, Oct: {0:o}, Bin: {0:b}", 42);
}

void TestLogLevel() {
    std::cout << "\n=== 日志级别测试 ===" << std::endl;
    
    LOG_INFO("Current level: info");
    
    // 设置级别为 Debug
    ai_camera::log::SetLevel(ai_camera::log::Level::Debug);
    LOG_INFO("Level changed to debug");
    LOG_DEBUG("This debug message should appear now");
    
    // 设置级别为 Warn
    ai_camera::log::SetLevel(ai_camera::log::Level::Warn);
    LOG_INFO("This info message should NOT appear");
    LOG_WARN("This warning message should appear");
}

void TestMultiThread() {
    std::cout << "\n=== 多线程日志测试 ===" << std::endl;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([i]() {
            for (int j = 0; j < 10; ++j) {
                LOG_INFO("Thread {} - Message {}", i, j);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== AI Camera 日志模块测试 ===" << std::endl;
    
    // 解析命令行参数
    bool async_mode = false;
    std::string log_file = "logs/test_logger.log";
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--async") {
            async_mode = true;
        } else if (std::string(argv[i]) == "--no-file") {
            log_file = "";
        }
    }
    
    // 初始化日志
    std::cout << "Initializing logger (async=" << (async_mode ? "true" : "false")
              << ", file=" << (log_file.empty() ? "none" : log_file) << ")..." << std::endl;
    
    ai_camera::log::Init("test_logger", log_file, async_mode);
    
    // 运行测试
    TestBasicLogging();
    TestFormattedLogging();
    TestLogLevel();
    TestMultiThread();
    
    // 关闭日志
    LOG_INFO("Test completed, shutting down...");
    ai_camera::log::Flush();
    ai_camera::log::Shutdown();
    
    std::cout << "Test passed!" << std::endl;
    return 0;
}
