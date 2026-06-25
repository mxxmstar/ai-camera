/**
 * @file logger.h
 * @brief AI Camera 日志模块（基于 spdlog）
 * 
 * 功能：
 *   - 支持同步/异步模式
 *   - 支持多级别日志（trace/debug/info/warn/error/critical）
 *   - 支持控制台 + 文件输出
 *   - 支持日志轮转（按大小/时间）
 *   - 线程安全
 *   - 支持文件名、行号、函数名、线程ID
 *   - 支持彩色日志输出
 * 
 * 日志格式：
 *   控制台：[2026-06-02 19:24:04.233] [1234] [info] [main.cpp>main#61] Message
 *   文件：  [2026-06-02 19:24:04.233] [1234] [info] [main.cpp>main#61] Message
 * 
 * 颜色：
 *   - trace: 灰色
 *   - debug: 绿色
 *   - info:  青色
 *   - warn:  黄色
 *   - error: 红色
 *   - critical: 红色背景
 * 
 * 用法：
 *   - 初始化：Logger::Init("ai-camera", "logs/ai-camera.log");
 *   - 记录日志：LOG_INFO("Hello, {}", "world");
 *   - 关闭：Logger::Shutdown();
 */

#ifndef AI_CAMERA_LOG_LOGGER_H
#define AI_CAMERA_LOG_LOGGER_H

#include <memory>
#include <string>
#include <iostream>

// spdlog 头文件
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"

namespace ai_camera {
namespace log {

/// @brief 日志级别
enum class Level {
    Trace    = SPDLOG_LEVEL_TRACE,
    Debug    = SPDLOG_LEVEL_DEBUG,
    Info     = SPDLOG_LEVEL_INFO,
    Warn     = SPDLOG_LEVEL_WARN,
    Error    = SPDLOG_LEVEL_ERROR,
    Critical = SPDLOG_LEVEL_CRITICAL,
};

/**
 * @brief 初始化日志模块
 * 
 * @param logger_name   日志器名称
 * @param log_file_path 日志文件路径（如 "logs/ai-camera.log"），传空则仅控制台输出
 * @param async_mode    是否启用异步模式（默认 false，同步模式）
 * @param max_file_size 单文件最大大小（字节，默认 5MB）
 * @param max_files     最大轮转文件数（默认 3）
 * 
 * 日志格式：
 *   [2026-06-02 19:24:04.233] [1234] [info] [main.cpp>main#61] Message
 * 
 * 示例：
 *   // 同步模式，输出到控制台 + 文件
 *   Logger::Init("ai-camera", "logs/ai-camera.log");
 *   
 *   // 异步模式，输出到控制台 + 文件
 *   Logger::Init("ai-camera", "logs/ai-camera.log", true);
 *   
 *   // 仅控制台输出
 *   Logger::Init("ai-camera", "");
 */
inline void Init(const std::string& logger_name,
                 const std::string& log_file_path = "",
                 bool async_mode = false,
                 size_t max_file_size = 5 * 1024 * 1024,
                 size_t max_files = 3) {
    try {
        // 0. 设置日志格式（包含线程ID、文件名、函数名、行号）
        // 格式：[2026-06-02 19:24:04.233] [线程ID] [级别] [文件名>函数名#行号] 消息
        // 注意：%! 需要 spdlog 1.9+ 和 C++20 的 std::source_location 支持
        //       如果编译器不支持，可以移除 %! 或替换为 %s
        const std::string console_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] [%s>%!#%#] %v";
        const std::string file_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] [%s>%!#%#] %v";

        // 1. 初始化线程池（异步模式需要）
        if (async_mode) {
            spdlog::init_thread_pool(4096, 1);  // 队列大小 4096，1 个后台线程
        }

        // 2. 创建 sinks（输出目标）
        std::vector<spdlog::sink_ptr> sinks;

        // 2.1 控制台 sink（带颜色）
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(console_pattern);
        // 设置颜色
        console_sink->set_color_mode(spdlog::color_mode::always);
        sinks.push_back(console_sink);

        // 2.2 文件 sink（如果指定了文件路径）
        if (!log_file_path.empty()) {
            // 确保目录存在
            // TODO: 创建目录（跨平台）

            // 轮转文件 sink（按大小）
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file_path, max_file_size, max_files
            );
            file_sink->set_pattern(file_pattern);
            sinks.push_back(file_sink);

            // 或者使用每日轮转 sink（按日期）
            // sinks.push_back(
            //     std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            //         log_file_path, 0, 0  // 每天 0:00 轮转
            //     )
            // );
        }

        // 3. 创建 logger
        std::shared_ptr<spdlog::logger> logger;

        if (async_mode) {
            logger = std::make_shared<spdlog::async_logger>(
                logger_name, sinks.begin(), sinks.end(), spdlog::thread_pool()
            );
        } else {
            logger = std::make_shared<spdlog::logger>(
                logger_name, sinks.begin(), sinks.end()
            );
        }

        // 4. 设置日志级别（默认 Info）
        logger->set_level(spdlog::level::info);

        // 5. 注册到全局
        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);

        // 使用 std::clog 而不是 LOG_INFO（因为宏会展开为 spdlog 调用，但这里正在初始化）
        std::clog << "[INFO] Logger initialized: name=" << logger_name 
                  << ", async=" << (async_mode ? "true" : "false") << std::endl;

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger init failed: " << ex.what() << std::endl;
    }
}

/**
 * @brief 设置日志级别
 * 
 * @param level 日志级别
 * 
 * 示例：
 *   Logger::SetLevel(Level::Debug);  // 显示 Debug 及以上级别
 */
inline void SetLevel(Level level) {
    spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
}

/**
 * @brief 关闭日志模块（刷新缓冲区）
 * 
 * 示例：
 *   Logger::Shutdown();
 */
inline void Shutdown() {
    // 注意：此处不能使用 LOG_INFO，因为宏展开后会调用 spdlog
    // 而 Shutdown 会销毁 logger
    spdlog::shutdown();
}

/**
 * @brief 刷新日志（确保全部写入）
 * 
 * 示例：
 *   Logger::Flush();
 */
inline void Flush() {
    spdlog::default_logger()->flush();
}

} // namespace log
} // namespace ai_camera

// ============================================================
// 日志宏（方便使用）
// ============================================================

#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)    SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

#endif // AI_CAMERA_LOG_LOGGER_H
