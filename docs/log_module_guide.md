# 日志模块使用指南

## 目录

1. [概述](#1-概述)
2. [快速上手](#2-快速上手)
3. [API 参考](#3-api-参考)
4. [配置选项](#4-配置选项)
5. [使用示例](#5-使用示例)
6. [高级功能](#6-高级功能)
7. [性能优化](#7-性能优化)
8. [故障排除](#8-故障排除)

---

## 1. 概述

### 1.1 模块简介

AI Camera 日志模块基于 **spdlog 1.17.0** 实现，提供：

- ✅ **同步/异步模式**（可选）
- ✅ **多级别日志**（Trace / Debug / Info / Warn / Error / Critical）
- ✅ **控制台 + 文件输出**（同时输出）
- ✅ **日志轮转**（按大小或按日期）
- ✅ **线程安全**（多线程环境下安全使用）
- ✅ **高性能**（基于 fmt 格式化，零拷贝）

### 1.2 文件结构

```
third_party/
└── spdlog-1.17.0/
    └── include/           # header-only 库

include/ai-camera/
└── log/
    └── logger.h          # 日志模块封装（本文件）

tests/
└── test_logger.cpp       # 测试程序

CMakeLists.txt            # 已添加 spdlog include 路径
```

---

## 2. 快速上手

### 2.1 初始化（在 main.cpp 中）

```cpp
#include "log/logger.h"

int main() {
    // 初始化日志（控制台 + 文件）
    ai_camera::log::Init("ai-camera", "logs/ai-camera.log");
    
    // 记录日志
    LOG_INFO("Application started");
    
    // 业务逻辑...
    
    // 关闭日志
    ai_camera::log::Shutdown();
    return 0;
}
```

### 2.2 记录日志

```cpp
LOG_TRACE("Trace message");
LOG_DEBUG("Debug message: value={}", 42);
LOG_INFO("Info message: pi={:.2f}", 3.14159);
LOG_WARN("Warning message");
LOG_ERROR("Error: {}", "something wrong");
LOG_CRITICAL("Critical error!");
```

### 2.3 编译运行

```bash
cd e:\project\ai-camera\build
cmake ..
cmake --build . --config Debug

# 运行测试程序
.\Debug\test_logger.exe

# 查看日志文件
type logs\test_logger.log
```

---

## 3. API 参考

### 3.1 初始化

```cpp
/**
 * @brief 初始化日志模块
 * 
 * @param logger_name   日志器名称
 * @param log_file_path 日志文件路径（传空则仅控制台输出）
 * @param async_mode    是否启用异步模式（默认 false）
 * @param max_file_size 单文件最大大小（默认 5MB）
 * @param max_files    最大轮转文件数（默认 3）
 */
void Init(const std::string& logger_name,
           const std::string& log_file_path = "",
           bool async_mode = false,
           size_t max_file_size = 5 * 1024 * 1024,
           size_t max_files = 3);
```

**示例**：

```cpp
// 1. 仅控制台输出
ai_camera::log::Init("ai-camera", "");

// 2. 控制台 + 文件（同步模式）
ai_camera::log::Init("ai-camera", "logs/ai-camera.log");

// 3. 控制台 + 文件（异步模式）
ai_camera::log::Init("ai-camera", "logs/ai-camera.log", true);

// 4. 自定义轮转参数
ai_camera::log::Init("ai-camera", "logs/ai-camera.log",
                        false,       // 同步模式
                        10 * 1024 * 1024,  // 10MB
                        5);          // 保留 5 个文件
```

### 3.2 设置日志级别

```cpp
/**
 * @brief 设置日志级别
 * 
 * @param level 日志级别枚举
 * 
 * 级别说明：
 *   - Trace:    最详细，记录所有日志
 *   - Debug:    调试信息
 *   - Info:     一般信息（默认）
 *   - Warn:     警告信息
 *   - Error:    错误信息
 *   - Critical: 严重错误
 */
void SetLevel(Level level);
```

**示例**：

```cpp
using namespace ai_camera::log;

SetLevel(Level::Trace);   // 显示所有日志
SetLevel(Level::Debug);   // 显示 Debug 及以上
SetLevel(Level::Info);    // 显示 Info 及以上（默认）
SetLevel(Level::Warn);    // 只显示 Warn 及以上
```

### 3.3 关闭和刷新

```cpp
/**
 * @brief 刷新日志（确保全部写入文件）
 */
void Flush();

/**
 * @brief 关闭日志模块（刷新缓冲区）
 */
void Shutdown();
```

**示例**：

```cpp
// 在程序退出前调用
LOG_INFO("Shutting down...");
ai_camera::log::Flush();
ai_camera::log::Shutdown();
```

---

## 4. 配置选项

### 4.1 同步 vs 异步

| 模式 | 特点 | 适用场景 |
|------|------|----------|
| **同步** | 日志写入时阻塞，直到写入完成 | 一般场景，日志量不大 |
| **异步** | 日志先入队列，后台线程写入，不阻塞主线程 | 高频日志，性能敏感场景 |

**异步模式初始化**：

```cpp
ai_camera::log::Init("ai-camera", "logs/ai-camera.log",
                        true);  // async_mode = true
```

### 4.2 日志格式

当前格式：`[2026-06-25 15:30:45.123] [info] [main.cpp:123] Message`

**自定义格式**：

修改 `logger.h` 中的 `set_pattern()` 调用：

```cpp
// 当前格式
logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");

// 更多格式选项：
// %Y-%m-%d: 日期
// %H:%M:%S.%e: 时间（精确到毫秒）
// %l: 日志级别（小写）
// %s: 源文件名
// %#: 行号
// %v: 日志消息
// %t: 线程 ID
// %P: 进程 ID
```

### 4.3 日志轮转

**按大小轮转**（当前默认）：

```cpp
// 单个文件最大 5MB，保留最近 3 个文件
ai_camera::log::Init("ai-camera", "logs/ai-camera.log",
                        false,
                        5 * 1024 * 1024,  // 5MB
                        3);                 // 3 个文件
```

轮转后的文件名示例：
```
ai-camera.log        # 当前日志
ai-camera.log.1      # 最近的轮转文件
ai-camera.log.2      # 次近的轮转文件
```

**按日期轮转**（需要修改代码）：

在 `logger.h` 中，将 `rotating_file_sink_mt` 替换为 `daily_file_sink_mt`：

```cpp
// 按日期轮转（每天 0:00 创建新文件）
sinks.push_back(
    std::make_shared<spdlog::sinks::daily_file_sink_mt>(
        log_file_path, 0, 0  // 0:00 轮转
    )
);
```

---

## 5. 使用示例

### 5.1 基础日志

```cpp
#include "log/logger.h"

void ExampleBasic() {
    LOG_TRACE("Trace level");
    LOG_DEBUG("Debug level: x={}", 10);
    LOG_INFO("Info level: name={}", "AI Camera");
    LOG_WARN("Warning level");
    LOG_ERROR("Error level: code={}", -1);
    LOG_CRITICAL("Critical level");
}
```

### 5.2 格式化日志

```cpp
void ExampleFormat() {
    int value = 42;
    double pi = 3.1415926;
    std::string name = "AI Camera";
    
    // 格式化（类似 Python 的 f-string）
    LOG_INFO("Integer: {}, Float: {:.2f}, String: {}", value, pi, name);
    
    // 十六进制、八进制、二进制
    LOG_INFO("Hex: {0:x}, Oct: {0:o}, Bin: {0:b}", 42);
    
    // 宽度和对齐
    LOG_INFO("Left aligned: {:<10}", "hello");
    LOG_INFO("Right aligned: {:>10}", "hello");
}
```

### 5.3 条件日志

```cpp
void ExampleConditional() {
    int error_code = -1;
    
    if (error_code != 0) {
        LOG_ERROR("Error occurred: code={}", error_code);
    }
    
    // 或者使用宏（零开销，条件不满足时不计算参数）
    int x = 10;
    LOG_DEBUG("x = {}", x);  // 如果日志级别 > Debug，不计算 x
}
```

### 5.4 多线程日志

```cpp
#include <thread>
#include <vector>

void WorkerThread(int id) {
    for (int i = 0; i < 100; ++i) {
        LOG_INFO("Thread {} - Message {}", id, i);
    }
}

void ExampleMultiThread() {
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(WorkerThread, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
}
```

---

## 6. 高级功能

### 6.1 不同模块使用不同日志器

```cpp
#include "spdlog/spdlog.h"

void ExampleMultipleLoggers() {
    // 创建名为 "mqtt" 的日志器
    auto mqtt_logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("mqtt");
    mqtt_logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] [MQTT] %v");
    
    // 创建名为 "gb28181" 的日志器
    auto gb_logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("gb28181");
    gb_logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] [GB28181] %v");
    
    // 使用
    mqtt_logger->info("MQTT connected");
    gb_logger->info("SIP registered");
}
```

### 6.2 运行时修改日志级别

```cpp
void ExampleDynamicLevel() {
    LOG_INFO("Current level: Info");
    
    // 改为 Debug（显示更多日志）
    ai_camera::log::SetLevel(ai_camera::log::Level::Debug);
    LOG_DEBUG("This debug message now appears");
    
    // 改为 Warn（减少日志输出）
    ai_camera::log::SetLevel(ai_camera::log::Level::Warn);
    LOG_INFO("This info message will NOT appear");
    LOG_WARN("This warning message appears");
}
```

### 6.3 日志回调（发送到网络/数据库）

```cpp
#include "spdlog/sinks/base_sink.h"

// 自定义 Sink：将日志发送到 MQTT
template<typename Mutex>
class MqttSink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        
        // 发送到 MQTT
        std::string payload(formatted.begin(), formatted.end());
        // mqtt::MqttManager::Instance().PublishLog(payload);
    }
    
    void flush_() override {}
};

using MqttSink_mt = MqttSink<std::mutex>;
```

---

## 7. 性能优化

### 7.1 编译期日志级别过滤

在 `CMakeLists.txt` 或源文件中定义：

```cpp
// 定义后，低于该级别的日志在编译期被移除（零开销）
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO

// 然后使用宏（而不是函数）
LOG_INFO("This will be compiled");
LOG_DEBUG("This will NOT be compiled (removed at compile time)");
```

### 7.2 异步模式

对于高频日志场景，使用异步模式：

```cpp
// 初始化异步模式
ai_camera::log::Init("ai-camera", "logs/ai-camera.log", true);

// 可选：调整线程池大小
spdlog::init_thread_pool(8192, 2);  // 8KB 队列，2 个线程
```

### 7.3 避免昂贵操作

```cpp
// ❌ 不好：即使日志级别不满足，也要计算参数
LOG_DEBUG("Value: {}", ExpensiveCalculation());

// ✅ 好：使用宏 + SPDLOG_ACTIVE_LEVEL，编译期移除
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
LOG_DEBUG("Value: {}", ExpensiveCalculation());  // 不会调用

// ✅ 更好：运行时判断
if (logger->should_log(spdlog::level::debug)) {
    LOG_DEBUG("Value: {}", ExpensiveCalculation());
}
```

---

## 8. 故障排除

### 8.1 编译错误

#### 错误：`spdlog/spdlog.h: No such file or directory`

**原因**：spdlog 的 include 路径未添加到 CMakeLists.txt。

**解决**：确认 `CMakeLists.txt` 中有：

```cmake
set(SPDLOG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/spdlog-1.17.0/include)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SPDLOG_INCLUDE_DIR}
)
```

#### 错误：`C2065: 'SPDLOG_LEVEL_TRACE': undeclared identifier`

**原因**：spdlog 版本不匹配或未正确包含。

**解决**：检查 `third_party/spdlog-1.17.0/` 是否存在，并确认版本。

### 8.2 运行错误

#### 错误：日志文件未生成

**原因**：目录不存在。

**解决**：确保日志目录存在：

```cpp
// 在 Init() 之前创建目录
#include <filesystem>
std::filesystem::create_directories("logs");
```

#### 错误：日志文件为空

**原因**：程序异常退出，缓冲区未刷新。

**解决**：

```cpp
// 在程序退出前调用
ai_camera::log::Flush();
ai_camera::log::Shutdown();
```

### 8.3 性能问题

#### 问题：日志写入拖慢主线程

**解决**：启用异步模式

```cpp
ai_camera::log::Init("ai-camera", "logs/ai-camera.log",
                        true);  // 异步模式
```

#### 问题：日志文件过大

**解决**：调小 `max_file_size` 或改为按日期轮转

```cpp
// 每个文件 1MB
ai_camera::log::Init("ai-camera", "logs/ai-camera.log",
                        false,
                        1 * 1024 * 1024,  // 1MB
                        5);
```

---

## 附录

### A. 日志级别选择指南

| 级别 | 用途 | 示例 |
|------|------|------|
| Trace | 最详细的信息，通常只在调试时启用 | 函数入口/出口、变量值 |
| Debug | 调试信息 | 变量状态、中间结果 |
| Info | 一般信息（默认级别） | 程序启动/停止、关键操作 |
| Warn | 警告信息 | 配置缺失、性能下降 |
| Error | 错误信息 | 操作失败、连接断开 |
| Critical | 严重错误 | 程序崩溃、数据丢失 |

### B. 格式化语法（fmt 库）

| 语法 | 说明 | 示例 |
|------|------|------|
| `{}` | 占位符 | `"Hello, {}"`, `"World"` → `"Hello, World"` |
| `{:d}` | 整数 | `"Value: {:d}"`, `42` → `"Value: 42"` |
| `{:.2f}` | 浮点数（2 位小数） | `"Pi: {:.2f}"`, `3.14159` → `"Pi: 3.14"` |
| `{:x}` | 十六进制 | `"Hex: {:x}"`, `255` → `"Hex: ff"` |
| `{:<10}` | 左对齐（宽度 10） | `"{:<<10}"`, `"hello"` → `"hello     "` |
| `{:>10}` | 右对齐（宽度 10） | `"{:>10}"`, `"hello"` → `"     hello"` |

### C. 参考资料

- **spdlog GitHub**：https://github.com/gabime/spdlog
- **spdlog Wiki**：https://github.com/gabime/spdlog/wiki
- **fmt 文档**：https://fmt.dev/latest/syntax.html

---

**作者**：AI Assistant  
**日期**：2026-06-25  
**版本**：1.0
