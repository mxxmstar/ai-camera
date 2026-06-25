# mDNS 模块使用指南

## 1. 模块概述

mDNS（多播 DNS）模块为 AI Camera 项目提供局域网服务自动发现功能，基于 ASIO 实现轻量级 mDNS 协议栈。

### 1.1 主要功能

- **服务注册**：将摄像头服务注册到局域网（RTSP、HTTP、MQTT 等）
- **服务发现**：自动发现局域网中的其他设备和服务
- **服务解析**：将服务名称解析为具体的 IP 地址和端口
- **零配置**：无需手动配置 IP 地址即可发现设备

### 1.2 模块组成

```
include/ai-camera/mdns/
├── mdns_types.h          # 类型定义（ServiceType、ServiceInfo、回调签名等）
├── mdns_service.h        # 主要服务接口（面向用户的 API）
└── mdns_impl.h           # 内部实现类（封装 mDNS 协议细节）

src/mdns/
├── mdns_service.cpp      # 实现 mdns_service.h 的接口
└── mdns_impl.cpp         # 实现 mDNS 协议（基于 ASIO）
```

---

## 2. 编译与构建

### 2.1 前置条件

- CMake 3.16+
- C++20 编译器
- ASIO（已集成到项目）
- spdlog（已集成到项目）

### 2.2 编译步骤

```bash
# 1. 进入项目根目录
cd e:\project\ai-camera

# 2. 创建构建目录
mkdir build
cd build

# 3. 生成构建文件
cmake ..

# 4. 编译项目（包含 mDNS 模块）
cmake --build . --config Release

# 5. 编译测试程序
cmake --build . --target test_mdns --config Release
```

### 2.3 验证编译

```bash
# 检查是否生成了测试程序
ls -l test_mdns*

# Windows:
dir test_mdns.exe
```

---

## 3. 快速开始

### 3.1 服务端：注册摄像头服务

```cpp
#include "mdns/mdns_service.h"

void RegisterCameraServices() {
    // 1. 获取 mDNS 服务单例
    auto mdns = mdns::MdnsService::Instance();
    
    // 2. 初始化（设置主机名为 "ai-camera"）
    if (!mdns->Init("ai-camera")) {
        std::cerr << "Failed to initialize mDNS" << std::endl;
        return;
    }
    
    // 3. 注册 RTSP 服务
    mdns::ServiceInfo rtsp_service;
    rtsp_service.name = "AI-Camera-001";
    rtsp_service.type = mdns::ServiceType::RTSP;
    rtsp_service.port = 554;
    rtsp_service.txt_records = {
        {"path", "/live/0"},
        {"resolution", "1920x1080"},
        {"codec", "H.264"}
    };
    
    mdns->RegisterService(rtsp_service);
    
    // 4. 注册 HTTP API 服务
    mdns::ServiceInfo http_service;
    http_service.name = "AI-Camera-001";
    http_service.type = mdns::ServiceType::HTTP;
    http_service.port = 8080;
    http_service.txt_records = {
        {"api_version", "v1"},
        {"model", "AI-Cam-1.0"}
    };
    
    mdns->RegisterService(http_service);
    
    std::cout << "Services registered successfully" << std::endl;
    
    // 5. 保持运行（服务会在后台广播）
    // ...
    
    // 6. 程序退出时关闭
    mdns->Shutdown();
}
```

### 3.2 客户端：发现摄像头设备

```cpp
#include "mdns/mdns_service.h"

void DiscoverCameraDevices() {
    // 1. 获取 mDNS 服务单例
    auto mdns = mdns::MdnsService::Instance();
    
    // 2. 初始化
    mdns->Init("test-client");
    
    // 3. 开始浏览 RTSP 服务
    auto on_found = [](const mdns::ServiceInfo& service) {
        std::cout << "Found camera: " << service.name << std::endl;
        std::cout << "  Port: " << service.port << std::endl;
        std::cout << "  Host: " << service.host << std::endl;
        
        // TODO: 连接到摄像头
        // ConnectToCamera(service.ipv4, service.port);
    };
    
    auto on_lost = [](const std::string& service_name) {
        std::cout << "Camera lost: " << service_name << std::endl;
    };
    
    int browse_id = mdns->StartBrowse(
        mdns::ServiceType::RTSP, 
        on_found, 
        on_lost
    );
    
    std::cout << "Browsing for cameras..." << std::endl;
    
    // 4. 保持运行（等待发现设备）
    // ...
    
    // 5. 停止浏览
    mdns->StopBrowse(browse_id);
    
    // 6. 关闭
    mdns->Shutdown();
}
```

---

## 4. API 参考

### 4.1 MdnsService 类

#### 4.1.1 单例访问

```cpp
static std::shared_ptr<MdnsService> Instance();
```

获取全局单例实例。

#### 4.1.2 初始化与销毁

```cpp
bool Init(const std::string& host_name = "ai-camera",
          Protocol protocol = Protocol::IPv4);
```

初始化 mDNS 模块。

**参数**：
- `host_name`：主机名（会自动追加 `.local.` 后缀）
- `protocol`：协议类型（`IPv4`、`IPv6`、`Both`）

**返回**：成功返回 `true`，失败返回 `false`。

```cpp
void Shutdown();
```

关闭 mDNS 模块，释放所有资源。

```cpp
bool IsInitialized() const;
```

查询是否已初始化。

#### 4.1.3 服务注册

```cpp
bool RegisterService(const ServiceInfo& service);
```

注册服务到局域网。

**参数**：
- `service`：服务信息结构体

**返回**：成功返回 `true`。

```cpp
bool UnregisterService(const std::string& service_name, ServiceType type);
```

注销服务。

```cpp
bool UpdateServiceTxt(const std::string& service_name,
                     ServiceType type,
                     const std::map<std::string, std::string>& txt_records);
```

更新服务的 TXT 记录。

#### 4.1.4 服务发现

```cpp
int StartBrowse(ServiceType type,
                 ServiceFoundHandler on_found,
                 ServiceLostHandler  on_lost);
```

开始浏览指定类型的服务。

**参数**：
- `type`：服务类型
- `on_found`：发现服务时的回调
- `on_lost`：服务消失时的回调

**返回**：浏览会话 ID（用于停止浏览），失败返回 `-1`。

```cpp
void StopBrowse(int browse_id);
```

停止浏览。

#### 4.1.5 服务解析

```cpp
void ResolveService(const std::string& service_name,
                   ServiceType type,
                   ServiceResolvedHandler handler);
```

解析服务（获取 IP 地址和端口）。

### 4.2 ServiceInfo 结构体

```cpp
struct ServiceInfo {
    std::string name;       // 服务实例名称
    ServiceType type;        // 服务类型
    std::string type_str;    // 自定义服务类型（当 type=Custom 时使用）
    uint16_t    port;       // 服务端口
    std::string host;        // 主机名
    std::string ipv4;       // IPv4 地址
    std::string ipv6;       // IPv6 地址
    std::map<std::string, std::string> txt_records;  // TXT 记录
};
```

### 4.3 服务类型枚举

```cpp
enum class ServiceType {
    RTSP,       // RTSP 流媒体服务（_rtsp._tcp）
    HTTP,       // HTTP API 服务（_http._tcp）
    HTTPS,      // HTTPS API 服务（_https._tcp）
    MQTT,       // MQTT 服务（_mqtt._tcp）
    ONVIF,      // ONVIF 服务（_onvif._tcp）
    Custom      // 自定义服务类型
};
```

---

## 5. 测试程序使用

### 5.1 编译测试程序

```bash
cd build
cmake --build . --target test_mdns
```

### 5.2 运行测试（服务端）

在摄像头设备上运行：

```bash
# Windows:
test_mdns.exe register

# Linux:
./test_mdns register
```

**预期输出**：
```
========================================
       mDNS Module Test Program         
========================================

========================================
Test: Service Registration
========================================

[INFO] mDNS initialized successfully
[INFO] Host name: ai-camera.local.
[INFO] RTSP service registered successfully
[INFO] HTTP service registered successfully

[INFO] Waiting for 10 seconds...
[INFO] Other devices should be able to discover this camera now
..........
[INFO] Services unregistered
[INFO] mDNS shutdown complete
```

### 5.3 运行测试（客户端）

在另一台设备上运行：

```bash
# Windows:
test_mdns.exe discover

# Linux:
./test_mdns discover
```

**预期输出**：
```
========================================
Test: Service Discovery
========================================

[INFO] mDNS initialized successfully
[INFO] Host name: test-client.local.

[INFO] Browsing for RTSP services...
[INFO] Browse started (browse_id=1)
[INFO] Waiting for 30 seconds to discover services...

[FOUND] Service discovered:
  Name: AI-Camera-001
  Type: _rtsp._tcp
  Port: 554
  Host: ai-camera.local.
  TXT Records:
    path = /live/0
    resolution = 1920x1080
    codec = H.264
    fps = 30

..........
[INFO] Browse stopped
[INFO] mDNS shutdown complete
```

---

## 6. 常见问题

### 6.1 服务无法被发现

**可能原因**：
1. 防火墙阻止了 UDP 5353 端口
2. 设备不在同一子网
3. 多播未启用

**解决方法**：
1. 开放防火墙端口：
   ```bash
   # Windows (PowerShell 管理员权限)
   New-NetFirewallRule -DisplayName "mDNS" -Direction Inbound -Protocol UDP -LocalPort 5353 -Action Allow
   ```
2. 确保设备在同一局域网
3. 检查网络设备是否支持多播

### 6.2 主机名冲突

**现象**：日志显示主机名冲突。

**解决方法**：使用唯一的主机名，例如包含设备序列号：
```cpp
mdns->Init("ai-camera-" + GetDeviceSerialNumber());
```

### 6.3 编译错误

**错误**：`mdns_impl.h: No such file or directory`

**原因**：头文件路径未正确配置。

**解决方法**：确保 `CMakeLists.txt` 中包含：
```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ai-camera
)
```

---

## 7. 性能优化建议

### 7.1 减少广播频率

默认情况下，mDNS 会定期广播服务信息。如果网络中有大量设备，可以适当降低频率：

```cpp
// TODO: 在实现中添加保活间隔配置
```

### 7.2 使用 TXT 记录缓存

对于不经常变化的信息，可以使用 TXT 记录缓存，减少解析次数。

### 7.3 限制服务数量

每个注册的服务都会产生网络流量，建议仅注册必要的服务。

---

## 8. 安全注意事项

### 8.1 mDNS 安全风险

- mDNS 是无认证的协议，任何设备都可以注册同名服务（中间人攻击风险）
- 服务信息在网络中明文传输

### 8.2 建议措施

1. **验证服务身份**：在应用层验证设备身份（如通过证书）
2. **使用加密通信**：优先使用 HTTPS、TLS 等加密协议
3. **网络隔离**：将摄像头设备放在独立的 VLAN 中

---

## 9. 参考资料

- [RFC 6762 - mDNS](https://tools.ietf.org/html/rfc6762)
- [RFC 6763 - DNS-SD](https://tools.ietf.org/html/rfc6763)
- [ASIO 文档](https://think-async.com/)
- [项目 README](../README.md)

---

**文档版本**：v1.0  
**最后更新**：2026-06-25  
**维护者**：AI Camera 开发团队
