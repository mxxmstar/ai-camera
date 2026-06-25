# 串口模块实现指导文档

## 目录

- [1. 模块概述](#1-模块概述)
- [2. 架构设计](#2-架构设计)
- [3. 文件结构](#3-文件结构)
- [4. 核心组件详解](#4-核心组件详解)
- [5. API 参考手册](#5-api-参考手册)
- [6. HTTP API 接口](#6-http-api-接口)
- [7. 使用示例](#7-使用示例)
- [8. 扩展开发指南](#8-扩展开发指南)
- [9. 编译与集成](#9-编译与集成)
- [10. 故障排查](#10-故障排查)

---

## 1. 模块概述

### 1.1 功能特性

串口模块（`serial`）为 AI 摄像头项目提供通用的异步串口通信能力，支持：

- **多串口并发管理**：同时管理多个串口设备（如云台 + 传感器）
- **ASIO 异步 I/O**：基于 `asio::serial_port` 实现非阻塞读写，与项目现有模块风格一致
- **可扩展协议框架**：策略模式，内置三种协议（Line/Length/Raw），支持自定义扩展
- **线程安全**：写队列互斥保护，支持多线程调用
- **HTTP API**：提供 RESTful 接口远程管理串口
- **跨平台支持**：Windows（COM1 等）和 Linux（`/dev/ttyUSB0` 等）

### 1.2 设计目标

- 与项目现有模块（`OnvifManager`、`Gb28181Manager`、`MqttManager`）保持一致的代码风格
- 模块解耦，不依赖其他业务模块
- 预留 PTZ 云台控制扩展点（未来可添加 Pelco-D/P 协议）

---

## 2. 架构设计

### 2.1 分层架构

```
┌─────────────────────────────────────────────────────────┐
│                 应用层（main.cpp）                      │
│  SerialManager::Instance().Open(...)                   │
│  SerialManager::Instance().Send(...)                   │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────┐
│            HTTP API 层（serial_http.cpp）               │
│  RegisterSerialRoutes(router)                          │
│  GET/POST/DELETE /serial/ports?name=xxx               │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────┐
│         管理面层（serial_manager.cpp）                  │
│  SerialManager 单例                                     │
│  ports_ map<string, unique_ptr<SerialPort>>            │
│  独立 io_context + 专用 IO 线程                        │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────┐
│         底层 I/O 层（serial_port.cpp）                 │
│  SerialPort（封装 asio::serial_port）                  │
│  异步读写、写队列、协议回调                            │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────┐
│       协议解析层（serial_protocol.cpp）                 │
│  SerialProtocol 策略接口                               │
│  LineProtocol / LengthProtocol / RawProtocol            │
└─────────────────────────────────────────────────────────┘
```

### 2.2 线程模型

```
┌─────────────────┐     post()      ┌─────────────────────┐
│   调用线程       │ ──────────────> │   SerialManager     │
│   (任意线程)     │    (线程安全)    │   IO 线程           │
└─────────────────┘                 └─────────────────────┘
        │                                    │
        │                                    │ async_read
        │ Send()                            │ async_write
        └──> write_queue_ ───────────────> │
             (mutex 保护)                   │
                                          asio::serial_port
```

**关键点：**
- `SerialManager` 拥有独立的 `asio::io_context` 和专用工作线程
- 所有串口 I/O 操作均在 IO 线程中执行
- `Send()` 线程安全：数据加入写队列后，通过 `asio::post()` 派发到 IO 线程
- 写队列机制防止并发 `async_write` 导致数据交错

---

## 3. 文件结构

### 3.1 头文件（`include/ai-camera/serial/`）

| 文件 | 说明 |
|------|------|
| `serial_config.h` | 串口配置结构体 `SerialConfig` |
| `serial_protocol.h` | 协议解析策略接口 `SerialProtocol` 及内置协议声明 |
| `serial_port.h` | 底层异步读写封装 `SerialPort` 类 |
| `serial_manager.h` | 多串口管理单例 `SerialManager` 类 |
| `serial_http.h` | HTTP API 路由注册接口 |

### 3.2 源文件（`src/serial/`）

| 文件 | 说明 |
|------|------|
| `serial_protocol.cpp` | LineProtocol、LengthProtocol、RawProtocol 实现 |
| `serial_port.cpp` | SerialPort 异步读写、写队列、协议回调实现 |
| `serial_manager.cpp` | SerialManager 单例、多串口管理、回调分发实现 |
| `serial_http.cpp` | HTTP API 路由注册及端点处理实现 |

### 3.3 依赖关系

```
serial_http.h  --> serial_manager.h
serial_manager.h --> serial_port.h
serial_port.h --> serial_protocol.h, serial_config.h
serial_protocol.h --> (无依赖)
```

---

## 4. 核心组件详解

### 4.1 SerialConfig（串口配置）

**文件：** `include/ai-camera/serial/serial_config.h`

```cpp
struct SerialConfig {
    std::string device;          // 设备路径: "COM1" (Windows) 或 "/dev/ttyUSB0" (Linux)
    unsigned int baud_rate = 9600;   // 波特率
    unsigned int data_bits = 8;      // 数据位 (5/6/7/8)
    char         parity    = 'N';    // 校验位: 'N'=无, 'O'=奇, 'E'=偶
    unsigned int stop_bits = 1;     // 停止位 (1/2)
    std::string protocol = "line";  // 协议类型: "line" | "length" | "raw"
    std::string line_delimiter = "\n";  // LineProtocol 行分隔符
    
    // LengthProtocol 配置
    unsigned int length_field_size = 2;   // 长度字段字节数 (1/2/4)
    bool         length_big_endian = false; // 长度字段字节序
    unsigned int length_offset     = 0;    // 长度字段偏移
    unsigned int max_frame_size    = 4096; // 最大帧长度
};
```

### 4.2 SerialProtocol（协议解析策略）

**文件：** `include/ai-camera/serial/serial_protocol.h`

#### 策略接口

```cpp
class SerialProtocol {
public:
    virtual ~SerialProtocol() = default;
    
    // 输入原始字节流，输出解析出的完整帧列表
    virtual std::vector<std::string> OnData(const char* data, size_t len) = 0;
    
    // 帧封装（发送时调用，子类可重写添加帧头/帧尾/校验等）
    virtual std::string WrapFrame(const std::string& payload) { return payload; }
};
```

#### 内置协议

**LineProtocol（按行分帧）**

```cpp
// 适用于文本协议（AT 指令、NMEA 等）
auto proto = std::make_unique<LineProtocol>("\n");  // 自定义分隔符
```

- 以指定分隔符作为帧边界
- 默认分隔符：`\n`
- 未满一行的数据缓存在内部 `buffer_`

**LengthProtocol（长度字段分帧）**

```cpp
// 适用于二进制协议
auto proto = std::make_unique<LengthProtocol>(
    2,      // 长度字段字节数
    false,  // 字节序 (false=小端)
    0,      // 长度字段偏移
    4096    // 最大帧长度
);
```

- 帧格式：`[长度字段(1/2/4字节)][payload...]`
- 支持大端/小端配置
- `max_frame_size` 防止异常数据导致内存耗尽

**RawProtocol（透传）**

```cpp
// 所有收到的数据立即作为一帧回调
auto proto = std::make_unique<RawProtocol>();
```

#### 工厂函数

```cpp
std::unique_ptr<SerialProtocol> CreateProtocol(
    const std::string&  protocol_name,  // "line" | "length" | "raw"
    const SerialConfig& config = {}
);
```

### 4.3 SerialPort（底层异步读写）

**文件：** `include/ai-camera/serial/serial_port.h`

#### 类接口

```cpp
class SerialPort {
public:
    SerialPort(asio::io_context& io_ctx,
               const std::string& name,
               const SerialConfig& config);
    
    bool Open();   // 打开串口并启动异步读取
    void Close();  // 关闭串口（停止读写）
    bool IsOpen() const;
    
    bool Send(const std::string& data);  // 线程安全，立即返回
    
    void SetRecvHandler(RecvHandler h);  // 注册接收回调
    void SetErrorHandler(ErrorHandler h); // 注册错误回调
    
    SerialConfig GetConfig() const;
    std::string  GetName() const;
};
```

#### 回调类型

```cpp
using RecvHandler = std::function<void(
    const std::string& port_name,  // 串口名称（如 "sensor1"）
    const std::string& frame       // 解析出的完整帧数据
)>;

using ErrorHandler = std::function<void(
    const std::string& port_name,
    const std::string& error_msg
)>;
```

#### 内部实现要点

1. **读缓冲区**：固定 512 字节（`READ_BUF_SIZE`）
2. **写队列**：`std::queue<std::string> write_queue_`，防止并发 `async_write`
3. **异步读取流程**：
   ```
   Open() --> DoRead() --> async_read_some() --> OnRead()
       ^                                              |
       |                                              v
       +<------ DoRead() <------ 协议解析 <------ 完整帧
   ```
4. **异步写入流程**：
   ```
   Send() --> post(DoWrite) --> async_write() --> OnWrite()
                  |
                  v
           write_queue_.pop()
           若队列非空 --> DoWrite()
   ```

### 4.4 SerialManager（多串口管理单例）

**文件：** `include/ai-camera/serial/serial_manager.h`

#### 单例模式

```cpp
class SerialManager {
public:
    static SerialManager& Instance();  // 全局访问点
    
    // 禁用拷贝
    SerialManager(const SerialManager&) = delete;
    SerialManager& operator=(const SerialManager&) = delete;
    
private:
    SerialManager();  // 私有构造函数
};
```

#### 生命周期管理

```cpp
bool Start();  // 启动（创建 io_context 和专用 IO 线程）
void Stop();   // 停止所有串口并退出 IO 线程
bool IsRunning() const;
```

#### 串口管理接口

```cpp
// 打开/关闭串口
bool Open(const std::string& name, const SerialConfig& cfg);
void Close(const std::string& name);
bool IsOpen(const std::string& name) const;

// 发送数据（线程安全）
bool Send(const std::string& name, const std::string& data);

// 回调注册（每个串口可独立注册）
void RegisterRecvCallback(const std::string& name, RecvHandler cb);
void RegisterErrorCallback(const std::string& name, ErrorHandler cb);
```

#### 查询接口

```cpp
std::vector<std::string> GetPortNames() const;  // 获取所有已注册串口名称
SerialConfig GetConfig(const std::string& name) const;  // 获取指定串口配置
std::string GetAllStatusJson() const;  // 获取所有串口状态 JSON
std::string GetPortStatusJson(const std::string& name) const;  // 获取指定串口状态 JSON
```

---

## 5. API 参考手册

### 5.1 C++ API

#### 初始化流程

```cpp
// 1. 启动串口管理模块
serial::SerialManager::Instance().Start();

// 2. 打开串口
serial::SerialConfig cfg;
cfg.device = "COM3";        // Windows: "COM3", Linux: "/dev/ttyUSB0"
cfg.baud_rate = 115200;
cfg.data_bits = 8;
cfg.parity = 'N';
cfg.stop_bits = 1;
cfg.protocol = "line";      // "line" | "length" | "raw"

bool ok = serial::SerialManager::Instance().Open("sensor1", cfg);
if (!ok) {
    std::cerr << "Failed to open serial port" << std::endl;
}

// 3. 注册回调
serial::SerialManager::Instance().RegisterRecvCallback(
    "sensor1",
    [](const std::string& name, const std::string& frame) {
        std::cout << "Received from " << name << ": " << frame << std::endl;
    }
);

serial::SerialManager::Instance().RegisterErrorCallback(
    "sensor1",
    [](const std::string& name, const std::string& error_msg) {
        std::cerr << "Error on " << name << ": " << error_msg << std::endl;
    }
);

// 4. 发送数据
serial::SerialManager::Instance().Send("sensor1", "AT\r\n");

// 5. 关闭串口
serial::SerialManager::Instance().Close("sensor1");

// 6. 停止模块（程序退出前）
serial::SerialManager::Instance().Stop();
```

#### 配置示例

**Line 协议（文本协议）**

```cpp
serial::SerialConfig cfg;
cfg.device = "/dev/ttyUSB0";
cfg.baud_rate = 9600;
cfg.protocol = "line";
// line_delimiter 默认为 "\n"，可在 LineProtocol 构造函数中指定
```

**Length 协议（二进制协议）**

```cpp
serial::SerialConfig cfg;
cfg.device = "COM1";
cfg.baud_rate = 115200;
cfg.protocol = "length";
cfg.length_field_size = 2;     // 长度字段 2 字节
cfg.length_big_endian = false;  // 小端
cfg.length_offset = 1;          // 长度字段从第 2 字节开始
cfg.max_frame_size = 1024;      // 最大帧 1KB
```

**Raw 协议（透传）**

```cpp
serial::SerialConfig cfg;
cfg.device = "COM2";
cfg.baud_rate = 57600;
cfg.protocol = "raw";
```

### 5.2 回调机制

#### 接收回调

```cpp
serial::SerialManager::Instance().RegisterRecvCallback(
    "sensor1",
    [](const std::string& name, const std::string& frame) {
        // frame 是协议解析出的完整帧
        // 对于 LineProtocol：一行文本（不含分隔符）
        // 对于 LengthProtocol：包含长度字段的完整二进制帧
        // 对于 RawProtocol：收到的原始数据块
        
        // 处理数据...
    }
);
```

#### 错误回调

```cpp
serial::SerialManager::Instance().RegisterErrorCallback(
    "sensor1",
    [](const std::string& name, const std::string& error_msg) {
        // 错误处理：
        // - 读写出错时触发
        // - 通常需要重新打开串口
        
        std::cerr << "Serial error on " << name << ": " << error_msg << std::endl;
        
        // 示例：自动重连
        serial::SerialConfig cfg;  // 需要保存之前的配置
        // ... 重新打开
    }
);
```

---

## 6. HTTP API 接口

### 6.1 路由注册

在 `main.cpp` 中注册路由：

```cpp
#include "ai-camera/serial/serial_http.h"

// 在路由注册部分添加：
serial::RegisterSerialRoutes(router);
```

### 6.2 API 端点

#### GET /serial/ports

获取所有串口状态。

**请求示例：**
```bash
curl http://localhost:8080/serial/ports
```

**响应示例：**
```json
{
  "ports": [
    {
      "name": "sensor1",
      "device": "COM3",
      "baud_rate": 115200,
      "protocol": "line",
      "open": true
    },
    {
      "name": "ptz",
      "device": "COM2",
      "baud_rate": 9600,
      "protocol": "raw",
      "open": false
    }
  ]
}
```

#### GET /serial/ports?name=xxx

获取指定串口状态。

**请求示例：**
```bash
curl http://localhost:8080/serial/ports?name=sensor1
```

**响应示例：**
```json
{
  "name": "sensor1",
  "device": "COM3",
  "baud_rate": 115200,
  "data_bits": 8,
  "parity": "N",
  "stop_bits": 1,
  "protocol": "line",
  "open": true
}
```

#### POST /serial/ports

打开新串口。

**请求示例：**
```bash
curl -X POST http://localhost:8080/serial/ports \
  -H "Content-Type: application/json" \
  -d '{
    "name": "sensor1",
    "device": "COM3",
    "baud_rate": 115200,
    "data_bits": 8,
    "parity": "N",
    "stop_bits": 1,
    "protocol": "line"
  }'
```

**响应示例：**
```json
{"name":"sensor1","status":"opened"}
```

**参数说明：**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | 否 | 串口逻辑名称，不提供则自动生成 |
| `device` | string | 是 | 设备路径（如 "COM3" 或 "/dev/ttyUSB0"） |
| `baud_rate` | int | 否 | 波特率，默认 9600 |
| `data_bits` | int | 否 | 数据位，默认 8 |
| `parity` | string | 否 | 校验位（"N"/"O"/"E"），默认 "N" |
| `stop_bits` | int | 否 | 停止位（1/2），默认 1 |
| `protocol` | string | 否 | 协议类型（"line"/"length"/"raw"），默认 "line" |

#### DELETE /serial/ports?name=xxx

关闭指定串口。

**请求示例：**
```bash
curl -X DELETE "http://localhost:8080/serial/ports?name=sensor1"
```

**响应示例：**
```json
{"name":"sensor1","status":"closed"}
```

#### POST /serial/ports/send?name=xxx

向指定串口发送数据。

**请求示例：**
```bash
curl -X POST "http://localhost:8080/serial/ports/send?name=sensor1" \
  -H "Content-Type: application/json" \
  -d '{"data":"AT\r\n"}'
```

**响应示例：**
```json
{"name":"sensor1","bytes":4}
```

**参数说明：**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | 是 | 串口名称（查询参数） |
| `data` | string | 是 | 要发送的数据（JSON body） |

---

## 7. 使用示例

### 7.1 基础用法

```cpp
#include "ai-camera/serial/serial_manager.h"
#include "ai-camera/serial/serial_config.h"
#include "ai-camera/log/logger.h"

void ExampleBasicUsage() {
    // 启动串口管理模块
    auto& mgr = serial::SerialManager::Instance();
    if (!mgr.Start()) {
        LOG_ERROR("Failed to start SerialManager");
        return;
    }
    
    // 配置串口
    serial::SerialConfig cfg;
    cfg.device = "COM3";
    cfg.baud_rate = 115200;
    cfg.protocol = "line";
    
    // 打开串口
    if (!mgr.Open("sensor1", cfg)) {
        LOG_ERROR("Failed to open serial port");
        return;
    }
    
    // 注册接收回调
    mgr.RegisterRecvCallback("sensor1",
        [](const std::string& name, const std::string& frame) {
            LOG_INFO("Received from {}: {}", name, frame);
        }
    );
    
    // 发送数据
    mgr.Send("sensor1", "AT+VERSION\r\n");
    
    // ... 程序运行 ...
    
    // 关闭串口
    mgr.Close("sensor1");
    
    // 停止模块
    mgr.Stop();
}
```

### 7.2 多串口并发

```cpp
void ExampleMultiPort() {
    auto& mgr = serial::SerialManager::Instance();
    mgr.Start();
    
    // 打开多个串口
    serial::SerialConfig cfg_sensor;
    cfg_sensor.device = "COM3";
    cfg_sensor.baud_rate = 115200;
    cfg_sensor.protocol = "line";
    mgr.Open("sensor", cfg_sensor);
    
    serial::SerialConfig cfg_ptz;
    cfg_ptz.device = "COM2";
    cfg_ptz.baud_rate = 9600;
    cfg_ptz.protocol = "raw";  // 未来可改为 "pelco-d"
    mgr.Open("ptz", cfg_ptz);
    
    // 分别注册回调
    mgr.RegisterRecvCallback("sensor",
        [](const std::string& name, const std::string& frame) {
            // 处理传感器数据
            LOG_INFO("[Sensor] {}", frame);
        }
    );
    
    mgr.RegisterRecvCallback("ptz",
        [](const std::string& name, const std::string& frame) {
            // 处理云台返回数据
            LOG_INFO("[PTZ] {} bytes received", frame.size());
        }
    );
    
    // 向不同串口发送数据
    mgr.Send("sensor", "MEASURE\n");
    mgr.Send("ptz", "\x55\x01\x00\x00\x00\x56");  // Pelco-D 指令（示例）
}
```

### 7.3 自定义协议

```cpp
// 自定义协议：固定帧头 + 固定长度
class FixedFrameProtocol : public serial::SerialProtocol {
public:
    FixedFrameProtocol(uint8_t header, size_t frame_size)
        : header_(header), frame_size_(frame_size) {}
    
    std::vector<std::string> OnData(const char* data, size_t len) override {
        std::vector<std::string> frames;
        buffer_.append(data, len);
        
        // 查找帧头
        while (buffer_.size() >= frame_size_) {
            if (static_cast<uint8_t>(buffer_[0]) != header_) {
                buffer_.erase(0, 1);  // 丢弃错误字节
                continue;
            }
            
            // 提取完整帧
            frames.push_back(buffer_.substr(0, frame_size_));
            buffer_.erase(0, frame_size_);
        }
        
        return frames;
    }
    
    std::string WrapFrame(const std::string& payload) override {
        // 添加帧头
        std::string frame;
        frame.push_back(static_cast<char>(header_));
        frame += payload;
        return frame;
    }
    
private:
    uint8_t     header_;
    size_t      frame_size_;
    std::string buffer_;
};

// 使用自定义协议
void ExampleCustomProtocol() {
    auto& mgr = serial::SerialManager::Instance();
    mgr.Start();
    
    serial::SerialConfig cfg;
    cfg.device = "COM4";
    cfg.baud_rate = 57600;
    cfg.protocol = "custom";  // 标记为自定义协议
    
    mgr.Open("custom_device", cfg);
    
    // 获取 SerialPort 并设置自定义协议
    // 注意：当前实现通过 config.protocol 自动创建协议
    // 若要使用自定义协议，需修改 SerialPort 或扩展 CreateProtocol
}
```

### 7.4 与 HTTP API 集成

```cpp
// main.cpp 中集成串口模块

#include "ai-camera/serial/serial_http.h"
#include "ai-camera/serial/serial_manager.h"

static void signal_handler(int signum) {
    std::cout << "\n[Main] Caught signal, shutting down..." << std::endl;
    // ... 停止其他模块 ...
    
    // 停止串口模块
    serial::SerialManager::Instance().Stop();
}

int main(int argc, char* argv[]) {
    // ... 初始化其他模块 ...
    
    // 创建 HTTP server
    g_http_server = std::make_unique<http::Server>("0.0.0.0", 8080);
    auto& router = g_http_server->router();
    
    // 注册 ONVIF 路由
    onvif::OnvifManager::Instance().RegisterRoutes(router);
    
    // +++ 注册串口模块 HTTP API +++
    serial::RegisterSerialRoutes(router);
    
    // ... 启动服务器 ...
    
    return 0;
}
```

---

## 8. 扩展开发指南

### 8.1 添加新协议

**步骤 1：继承 `SerialProtocol`**

```cpp
// include/ai-camera/serial/serial_protocol.h

class PelcoDProtocol : public SerialProtocol {
public:
    PelcoDProtocol() = default;
    
    std::vector<std::string> OnData(const char* data, size_t len) override {
        std::vector<std::string> frames;
        buffer_.append(data, len);
        
        // Pelco-D 帧格式：
        // [0xFF][地址][命令1][命令2][数据1][数据2][校验和]
        while (buffer_.size() >= 7) {
            if (static_cast<uint8_t>(buffer_[0]) != 0xFF) {
                buffer_.erase(0, 1);
                continue;
            }
            
            // 提取完整帧（7 字节）
            frames.push_back(buffer_.substr(0, 7));
            buffer_.erase(0, 7);
        }
        
        return frames;
    }
    
    std::string WrapFrame(const std::string& payload) override {
        // Pelco-D 帧封装
        // payload 应包含除帧头和校验和外的字节
        std::string frame;
        frame.push_back('\xFF');  // 帧头
        frame += payload;
        
        // 计算校验和
        uint8_t checksum = 0;
        for (size_t i = 1; i < frame.size(); ++i) {
            checksum += static_cast<uint8_t>(frame[i]);
        }
        frame.push_back(static_cast<char>(checksum));
        
        return frame;
    }
    
private:
    std::string buffer_;
};
```

**步骤 2：注册到工厂函数**

```cpp
// src/serial/serial_protocol.cpp

std::unique_ptr<SerialProtocol> CreateProtocol(
    const std::string&  protocol_name,
    const SerialConfig& config)
{
    if (protocol_name == "line") {
        return std::make_unique<LineProtocol>();
    }
    
    if (protocol_name == "length") {
        return std::make_unique<LengthProtocol>(/*...*/);
    }
    
    if (protocol_name == "raw") {
        return std::make_unique<RawProtocol>();
    }
    
    // +++ 添加新协议 +++
    if (protocol_name == "pelco-d") {
        return std::make_unique<PelcoDProtocol>();
    }
    
    return nullptr;
}
```

**步骤 3：使用新协议**

```cpp
serial::SerialConfig cfg;
cfg.device = "COM2";
cfg.baud_rate = 9600;
cfg.protocol = "pelco-d";  // 使用新协议

serial::SerialManager::Instance().Open("ptz", cfg);
```

### 8.2 添加 HTTP API 端点

```cpp
// src/serial/serial_http.cpp

void RegisterSerialRoutes(http::Router& router) {
    // ... 现有端点 ...
    
    // +++ 添加新的端点：获取串口配置 +++
    router.get("/serial/ports/:name/config", [](const http::Request& req) {
        std::string name = /* 提取名称 */;
        
        auto& mgr = SerialManager::Instance();
        serial::SerialConfig cfg = mgr.GetConfig(name);
        
        std::string json = /* 构建 JSON */;
        return http::Response::ok(json, "application/json");
    });
}
```

### 8.3 对接 ONVIF/GB28181 PTZ

未来可将 ONVIF/GB28181 的 PTZ 命令转换为串口协议：

```cpp
// 示例：ONVIF PTZ 命令 -> 串口指令
void OnvifPtzToSerial(const std::string& cmd, float pan, float tilt) {
    std::string serial_cmd;
    
    if (cmd == "ContinuousMove") {
        // 转换为 Pelco-D 指令
        serial_cmd = BuildPelcoDFrame(pan, tilt);
    }
    
    serial::SerialManager::Instance().Send("ptz", serial_cmd);
}
```

---

## 9. 编译与集成

### 9.1 编译要求

- **编译器**：支持 C++20（GCC 10+, Clang 10+, MSVC 2019+）
- **ASIO**：项目已内置 `third_party/asio-1.36.0`（standalone 模式）
- **依赖**：仅依赖 ASIO 和 C++ 标准库，无额外第三方库

### 9.2 CMake 配置

项目 `CMakeLists.txt` 使用 `file(GLOB_RECURSE SOURCES src/*.cpp)` 自动拾取源文件，因此新增的 `src/serial/*.cpp` 会自动被添加到构建目标，**无需修改 CMakeLists.txt**。

### 9.3 编译步骤

```bash
# 创建构建目录
mkdir build && cd build

# 配置（Linux）
cmake ..

# 配置（Windows，使用 MinGW）
cmake .. -G "MinGW Makefiles"

# 配置（Windows，使用 Visual Studio）
cmake .. -G "Visual Studio 17 2022"

# 编译
cmake --build .

# 或使用 make（Linux/MinGW）
make -j4
```

### 9.4 集成到 main.cpp

**完整示例：**

```cpp
#include "server/http/server.hpp"
#include "onvif/onvif_manager.h"
#include "gb28181/gb28181_manager.h"
#include "ai-camera/serial/serial_http.h"  // +++ 添加 +++

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

static std::unique_ptr<http::Server> g_http_server;

static void signal_handler(int signum) {
    std::cout << "\n[Main] Caught signal, shutting down..." << std::endl;
    if (g_http_server) g_http_server->stop();
    
    // 停止各模块
    onvif::OnvifManager::Instance().Stop();
    gb28181::Gb28181Manager::Instance().Stop();
    serial::SerialManager::Instance().Stop();  // +++ 添加 +++
}

int main(int argc, char* argv[]) {
    // ... 解析命令行参数 ...
    
    // 创建 HTTP server
    g_http_server = std::make_unique<http::Server>("0.0.0.0", 8080);
    auto& router = g_http_server->router();
    
    // 注册各模块路由
    onvif::OnvifManager::Instance().RegisterRoutes(router);
    serial::RegisterSerialRoutes(router);  // +++ 添加 +++
    
    // 注册信号处理器
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // 启动 HTTP server（阻塞）
    g_http_server->run();
    
    return 0;
}
```

---

## 10. 故障排查

### 10.1 常见问题

#### 问题 1：串口无法打开

**现象：**
```cpp
bool ok = mgr.Open("port1", cfg);
// ok == false
```

**排查步骤：**

1. **检查设备路径是否正确**
   - Windows：`"COM1"`, `"COM2"`, ...
   - Linux：`"/dev/ttyUSB0"`, `"/dev/ttyS0"`, ...

2. **检查权限（Linux）**
   ```bash
   ls -l /dev/ttyUSB0
   sudo chmod 666 /dev/ttyUSB0  # 临时授权
   sudo usermod -aG dialout $USER  # 永久授权（需注销重登录）
   ```

3. **检查端口是否被占用**
   - Windows：设备管理器查看
   - Linux：`lsof /dev/ttyUSB0`

4. **查看日志**
   ```
   [SerialPort] Open port1 failed: The system cannot find the file specified
   ```

#### 问题 2：数据接收不完整

**现象：** 回调函数未被触发，或数据不完整。

**可能原因：**

1. **协议配置错误**
   - 确认 `config.protocol` 与实际设备协议匹配
   - Line 协议：确认行分隔符正确
   - Length 协议：确认长度字段配置正确

2. **缓冲区溢出**
   - 默认读缓冲区 512 字节，可通过修改 `READ_BUF_SIZE` 调整

3. **串口参数不匹配**
   - 确认波特率、数据位、校验位、停止位与设备一致

#### 问题 3：发送数据失败

**现象：** `Send()` 返回 `false`，或数据未到达设备。

**排查步骤：**

1. **检查串口是否打开**
   ```cpp
   if (!mgr.IsOpen("port1")) {
       std::cerr << "Port not open" << std::endl;
   }
   ```

2. **检查写队列是否阻塞**
   - 写队列深度过大可能导致延迟
   - 检查错误回调是否触发

3. **使用串口调试工具验证**
   - Windows：使用 `putty` 或 `串口调试助手`
   - Linux：使用 `minicom` 或 `screen`

#### 问题 4：编译错误

**常见错误：**

1. **`asio::serial_port` 未定义**
   - 确认 ASIO 版本支持串口（1.36.0 支持）
   - 确认 `#include <asio.hpp>` 正确

2. **C++20 特性错误**
   - 确认编译器支持 C++20
   - 检查 `CMAKE_CXX_STANDARD 20` 是否设置

3. **链接错误**
   - 确认源文件被正确添加到构建目标
   - 检查 `file(GLOB_RECURSE ...)` 是否拾取新文件

### 10.2 调试技巧

#### 启用详细日志

```cpp
// 在 SerialPort::OnRead 中添加日志
void SerialPort::OnRead(const asio::error_code& ec, size_t bytes_transferred) {
    if (ec) {
        LOG_ERROR("[SerialPort] {} read error: {}", name_, ec.message());
        return;
    }
    
    // 打印原始十六进制数据
    std::string hex;
    for (size_t i = 0; i < bytes_transferred; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02X ", static_cast<uint8_t>(read_buf_[i]));
        hex += buf;
    }
    LOG_DEBUG("[SerialPort] {} received {} bytes: {}", name_, bytes_transferred, hex);
    
    // ... 协议解析 ...
}
```

#### 使用环回测试

1. **硬件环回**：短接 TX 和 RX 引脚，发送数据应能接收
2. **虚拟串口**：使用 `com0com`（Windows）或 `socat`（Linux）创建虚拟串口对

#### 单元测试

```cpp
// tests/test_serial.cpp

#include "ai-camera/serial/serial_protocol.h"
#include <cassert>

void TestLineProtocol() {
    serial::LineProtocol proto("\n");
    
    // 测试完整行
    auto frames = proto.OnData("Hello\n", 6);
    assert(frames.size() == 1);
    assert(frames[0] == "Hello");
    
    // 测试不完整行
    frames = proto.OnData("World", 5);
    assert(frames.empty());  // 未满一行，不触发回调
    
    // 测试剩余数据 + 新数据
    frames = proto.OnData("\n", 1);
    assert(frames.size() == 1);
    assert(frames[0] == "World");
    
    std::cout << "LineProtocol test passed" << std::endl;
}
```

---

## 附录

### A. 串口参数参考

| 波特率 | 说明 |
|--------|------|
| 9600 | 常用，稳定 |
| 19200 | 较快 |
| 38400 | 更快 |
| 57600 | 高速 |
| 115200 | 很高速 |
| 921600 | 极高速（需硬件支持） |

| 数据位 | 校验位 | 停止位 | 说明 |
|--------|--------|--------|------|
| 8 | N | 1 | 最常见（8N1） |
| 7 | E | 1 | 旧设备 |
| 8 | O | 1 | 较少见 |
| 8 | N | 2 | 较少见 |

### B. 协议选择指南

| 场景 | 推荐协议 | 说明 |
|------|----------|------|
| AT 指令 | Line | 文本协议，以 `\n` 或 `\r\n` 结尾 |
| NMEA 0183（GPS） | Line | 以 `$` 开头，`\n` 结尾 |
| Modbus RTU | Length | 固定帧结构，含 CRC 校验 |
| 自定义二进制 | Length | 需配置长度字段位置和字节序 |
| 原始数据流 | Raw | 如音频、固件升级数据流 |
| Pelco-D（云台） | 自定义 | 未来可添加 `PelcoDProtocol` |

### C. 性能优化建议

1. **读缓冲区大小**
   - 默认 512 字节，可通过修改 `SerialPort::READ_BUF_SIZE` 调整
   - 高频数据场景建议增大（如 2048 或 4096）

2. **写队列深度**
   - 当前无限制，高频发送场景建议添加最大深度限制
   - 修改 `SerialPort::Send()` 添加队列深度检查

3. **协议解析优化**
   - 零拷贝：当前实现在确认完整帧时才拷贝数据
   - 若需更高性能，可使用 `std::string_view` 或自定义缓冲区管理

4. **IO 线程优先级**
   - 实时性要求高的场景，可提高 IO 线程优先级
   ```cpp
   // 在 SerialManager::IoThreadFunc() 开头添加
   #include <pthread.h>
   pthread_setschedprio(pthread_self(), SCHED_RR);
   ```

---

## 文档版本

- **版本：** 1.0
- **日期：** 2026-06-25
- **作者：** AI Camera Development Team
- **对应代码版本：** ai-camera main branch
