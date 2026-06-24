# GB28181 模块实现指南

## 目录

1. [概述](#1-概述)
2. [架构设计](#2-架构设计)
3. [SIP 信令实现](#3-sip-信令实现)
4. [配置说明](#4-配置说明)
5. [使用示例](#5-使用示例)
6. [API 参考](#6-api-参考)
7. [故障排除](#7-故障排除)
8. [附录](#8-附录)

---

## 1. 概述

### 1.1 项目背景

本项目实现了一个轻量级的 GB28181 SIP 信令模块，用于视频监控设备与平台之间的信令交互。GB28181 是中国国家标准《安全防范视频监控联网系统信息传输、交换、控制技术要求》的简称。

### 1.2 技术特点

- **轻量级实现**：不依赖第三方 SIP 库（如 PJSIP、eXosip2），基于 asio 自主实现
- **零依赖**：仅需 asio（已包含在 third_party）和 Windows CryptAPI/OpenSSL（用于 MD5 计算）
- **C++20**：使用现代 C++20 标准实现
- **跨平台**：支持 Windows 和 Linux（asio 提供跨平台支持）
- **完整功能**：支持 GB28181 所需的基本 SIP 操作

### 1.3 主要功能

- ✅ SIP REGISTER（注册与保活）
- ✅ SIP MESSAGE（XML 报文传输）
- ✅ SIP INVITE（实时点播）
- ✅ SIP BYE（结束会话）
- ✅ SIP SUBSCRIBE/NOTIFY（订阅通知）
- ✅ SIP 认证（Digest MD5）
- ✅ 保活机制（Keepalive）

---

## 2. 架构设计

### 2.1 模块结构

```
include/ai-camera/gb28181/
├── gb28181_config.h    # GB28181 配置结构体
├── gb28181_types.h     # GB28181 类型定义
├── sip_message.h       # SIP 消息基础结构
└── sip_agent.h         # SIP 代理接口

src/gb28181/
├── sip_message.cpp     # SIP 消息解析和序列化
└── sip_agent.cpp       # SIP 代理实现
```

### 2.2 类关系图

```
┌─────────────────────────────────────────────────────────┐
│                     SipAgent                            │
│  - 核心 SIP 代理类，处理所有 SIP 信令逻辑                │
│  - 使用 SipTransport 进行 UDP 传输                       │
│  - 提供事件回调接口（SipEventCallbacks）                 │
└──────────────────┬──────────────────────────────────────┘
                   │ 使用
                   ▼
┌─────────────────────────────────────────────────────────┐
│                  SipTransport                           │
│  - SIP 传输层，基于 asio UDP 套接字                      │
│  - 负责发送和接收 SIP 消息                               │
│  - 异步接收，通过回调通知上层                            │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                   SipMessage                            │
│  - SIP 消息基类                                         │
│  - 提供解析和序列化功能                                  │
├─────────────────────────────────────────────────────────┤
│  SipRequest    - SIP 请求消息                           │
│  SipResponse   - SIP 响应消息                           │
└─────────────────────────────────────────────────────────┘
```

### 2.3 数据流

```
平台/SIP服务器
     │
     │  SIP 消息（UDP）
     ▼
┌─────────────────┐
│  SipTransport   │  ← 异步接收（asio）
└────────┬────────┘
         │  回调
         ▼
┌─────────────────┐
│    SipAgent     │  ← 消息处理
└────────┬────────┘
         │  回调
         ▼
┌─────────────────┐
│ 上层应用（如     │
│ Gb28181Manager） │
└─────────────────┘
```

---

## 3. SIP 信令实现

### 3.1 SIP 消息结构

#### 3.1.1 请求消息格式

```
METHOD SP Request-URI SP SIP/2.0\r\n
Via: SIP/2.0/UDP 192.168.1.100:5060;rport;branch=z9hG4bK123456\r\n
From: <sip:device_id@realm>;tag=123456\r\n
To: <sip:platform_id@realm>\r\n
Call-ID: 123456@192.168.1.100\r\n
CSeq: 1 METHOD\r\n
Contact: <sip:device_id@192.168.1.100:5060>\r\n
Content-Length: 0\r\n
\r\n
```

#### 3.1.2 响应消息格式

```
SIP/2.0 SP Status-Code SP Reason-Phrase\r\n
Via: SIP/2.0/UDP ...\r\n
From: ...\r\n
To: ...;tag=789012\r\n
Call-ID: ...\r\n
CSeq: 1 METHOD\r\n
Content-Length: 0\r\n
\r\n
```

### 3.2 关键功能实现

#### 3.2.1 REGISTER 注册流程

```
设备                        平台/SIP服务器
  │                             │
  │──── REGISTER (无认证) ────→│
  │←─── 401 Unauthorized ─────│
  │──── REGISTER (带认证) ───→│
  │←─── 200 OK ──────────────│
  │                             │
  │──── REGISTER (保活) ─────→│
  │←─── 200 OK ──────────────│
  │                             │
```

**实现要点**：
1. 首次 REGISTER 不包含 Authorization 头
2. 收到 401 响应后，解析 WWW-Authenticate 头获取认证信息
3. 计算 Digest 响应：`MD5(HA1:nonce:HA2)`
4. 重新发送带 Authorization 头的 REGISTER
5. 注册成功后，启动保活定时器定期发送 REGISTER

#### 3.2.2 MESSAGE 消息传输

用于传输 GB28181 XML 报文（如目录查询、设备控制等）。

**消息示例**：
```
MESSAGE sip:platform_id@realm SIP/2.0
Content-Type: Application/MANSCDP+xml
Content-Length: 123

<?xml version="1.0" encoding="GB2312"?>
<CmdType>Catalog</CmdType>
...
```

#### 3.2.3 INVITE 实时点播

```
设备                        平台
  │                             │
  │←─── INVITE (带 SDP) ──────│
  │──── 200 OK (带 SDP) ─────→│
  │←─── ACK ─────────────────│
  │                             │
  │  (开始推流...)              │
  │                             │
  │←─── BYE ──────────────────│
  │──── 200 OK ──────────────→│
```

**SDP 格式**：
```
v=0
o=device_id 0 0 IN IP4 192.168.1.100
s=Play
c=IN IP4 192.168.1.100
t=0 0
m=video 50000 RTP/AVP 96
a=rtpmap:96 PS/90000
a=sendonly
y=1234567890
```

### 3.3 SIP 认证实现

#### 3.3.1 Digest 认证流程

1. 服务器返回 401 Unauthorized，包含 WWW-Authenticate 头：
   ```
   WWW-Authenticate: Digest realm="3402000000", nonce="abc123", algorithm=MD5
   ```

2. 客户端计算响应：
   ```
   HA1 = MD5(username:realm:password)
   HA2 = MD5(method:uri)
   response = MD5(HA1:nonce:HA2)
   ```

3. 客户端发送带 Authorization 头的请求：
   ```
   Authorization: Digest username="device_id", realm="3402000000", 
                  nonce="abc123", uri="sip:server", response="..."
   ```

#### 3.3.2 MD5 计算

**Windows 平台**：使用 CryptAPI
```cpp
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
```

**Linux 平台**：使用 OpenSSL
```cpp
#include <openssl/md5.h>
```

---

## 4. 配置说明

### 4.1 Gb28181Config 配置结构体

```cpp
struct Gb28181Config {
    // ---- 设备标识 ----
    std::string device_id;      // 20 位国标编码，如 "34020000001320000001"
    
    // ---- SIP 服务器（平台）信息 ----
    std::string server_ip;      // 平台 SIP 服务器 IP
    uint16_t    server_port = 5060;  // 平台 SIP 端口
    
    // ---- 本地 SIP 信息 ----
    uint16_t    local_sip_port = 5060;  // 本地 SIP 监听端口
    
    // ---- SIP 认证 ----
    std::string sip_realm;      // SIP 域，如 "3402000000"
    std::string username;       // SIP 用户名（通常为 device_id）
    std::string password;       // SIP 密码
    
    // ---- 媒体参数 ----
    std::string video_file;     // H.264 文件路径（用于推流）
    uint32_t    frame_rate = 25;  // 帧率
    
    // ---- 注册与心跳 ----
    uint32_t    expires = 3600;            // 注册有效期（秒）
    uint32_t    keepalive_interval = 30;   // 心跳间隔（秒）
    uint32_t    keepalive_retry_count = 3; // 心跳失败重试次数
};
```

### 4.2 配置示例

```cpp
gb28181::Gb28181Config config;
config.device_id = "34020000001320000001";  // 20 位设备编码
config.server_ip = "192.168.1.10";          // SIP 服务器 IP
config.server_port = 5060;                   // SIP 服务器端口
config.local_sip_port = 5060;                // 本地监听端口
config.sip_realm = "3402000000";            // SIP 域
config.username = "34020000001320000001";   // 用户名
config.password = "123456";                  // 密码
config.expires = 3600;                       // 注册有效期
config.keepalive_interval = 30;              // 保活间隔
```

---

## 5. 使用示例

### 5.1 基本使用流程

```cpp
#include "gb28181/sip_agent.h"

int main() {
    // 1. 创建 SipAgent 实例
    gb28181::SipAgent agent;
    
    // 2. 配置 GB28181 参数
    gb28181::Gb28181Config config;
    config.device_id = "34020000001320000001";
    config.server_ip = "192.168.1.10";
    config.server_port = 5060;
    config.local_sip_port = 5060;
    config.sip_realm = "3402000000";
    config.username = "34020000001320000001";
    config.password = "123456";
    
    // 3. 设置事件回调
    gb28181::SipEventCallbacks callbacks;
    callbacks.on_register_success = []() {
        std::cout << "注册成功!" << std::endl;
    };
    callbacks.on_invite = [](const std::string& from, const std::string& to,
                              const std::string& call_id, const std::string& sdp) {
        std::cout << "收到 INVITE: " << call_id << std::endl;
        // 处理点播请求...
    };
    callbacks.on_message = [](const std::string& from, const std::string& xml_body) {
        std::cout << "收到 MESSAGE: " << xml_body << std::endl;
        // 处理 XML 报文...
    };
    
    agent.SetCallbacks(callbacks);
    
    // 4. 初始化并启动
    if (!agent.Init(config)) {
        std::cerr << "初始化失败!" << std::endl;
        return -1;
    }
    
    if (!agent.Start()) {
        std::cerr << "启动失败!" << std::endl;
        return -1;
    }
    
    // 5. 运行...
    std::cout << "SIP 代理已启动，按 Enter 退出..." << std::endl;
    std::cin.get();
    
    // 6. 停止
    agent.Stop();
    
    return 0;
}
```

### 5.2 发送 MESSAGE 示例

```cpp
// 构造 Catalog 查询响应 XML
std::string xml_body = R"(<?xml version="1.0" encoding="GB2312"?>
<Response>
  <CmdType>Catalog</CmdType>
  <SN>1234</SN>
  <DeviceID>34020000001320000001</DeviceID>
  <SumNum>1</SumNum>
  <Cmd>
    <Info>
      <CmdItem>
        <DeviceID>34020000001320000001</DeviceID>
        <Name>AI Camera</Name>
        <Status>ON</Status>
      </CmdItem>
    </Info>
  </Cmd>
</Response>)";

// 发送到平台
std::string platform_uri = "sip:34020000002000000001@3402000000";
agent.SendMessage(platform_uri, xml_body);
```

---

## 6. API 参考

### 6.1 SipAgent 类

#### 6.1.1 构造函数和析构函数

```cpp
SipAgent();   // 构造函数
~SipAgent();  // 析构函数（自动调用 Stop()）
```

#### 6.1.2 初始化和启动

```cpp
// 初始化 SIP 代理
// config: GB28181 配置
// 返回: 是否初始化成功
bool Init(const Gb28181Config& config);

// 启动 SIP 代理（开始监听和注册）
// 返回: 是否启动成功
bool Start();

// 停止 SIP 代理
void Stop();

// 是否正在运行
bool IsRunning() const;
```

#### 6.1.3 发送消息

```cpp
// 发送 SIP MESSAGE（XML 报文）
// to: 目标 URI
// xml_body: XML 消息体
// 返回: 是否发送成功
bool SendMessage(const std::string& to, const std::string& xml_body);

// 发送 SIP REGISTER（保活注册）
// 返回: 是否发送成功
bool SendRegister();

// 发送 INVITE 响应（200 OK with SDP）
// call_id: 呼叫 ID
// to: 接收方 URI
// sdp: SDP 描述
// 返回: 是否发送成功
bool SendInviteResponse(const std::string& call_id, const std::string& to,
                        const std::string& sdp);

// 发送 BYE 响应（200 OK）
// call_id: 呼叫 ID
// to: 接收方 URI
// 返回: 是否发送成功
bool SendByeResponse(const std::string& call_id, const std::string& to);

// 发送 MESSAGE 响应（200 OK）
// call_id: 呼叫 ID
// to: 接收方 URI
// 返回: 是否发送成功
bool SendMessageResponse(const std::string& call_id, const std::string& to);
```

#### 6.1.4 回调设置

```cpp
// 设置事件回调
void SetCallbacks(const SipEventCallbacks& cb);
```

### 6.2 SipEventCallbacks 结构体

```cpp
struct SipEventCallbacks {
    // 收到 INVITE（实时点播请求）
    std::function<void(const std::string& from, const std::string& to,
                       const std::string& call_id, const std::string& sdp)>
        on_invite;
    
    // 收到 BYE（结束会话请求）
    std::function<void(const std::string& call_id)> on_bye;
    
    // 收到 MESSAGE（XML 报文请求）
    std::function<void(const std::string& from, const std::string& xml_body)>
        on_message;
    
    // 注册成功
    std::function<void()> on_register_success;
    
    // 注册失败
    std::function<void(int status_code, const std::string& reason)> on_register_failed;
    
    // 收到 SUBSCRIBE（订阅请求）
    std::function<void(const std::string& from, const std::string& call_id,
                       const std::string& event_type)> on_subscribe;
};
```

### 6.3 SipMessage 类

#### 6.3.1 静态工厂方法

```cpp
// 从字符串解析 SIP 消息
static SipMessage* Parse(const std::string& raw_message);

// 生成唯一的 Call-ID
static std::string GenerateCallId(const std::string& local_ip);

// 生成分支参数
static std::string GenerateBranch();

// 生成标签（tag）
static std::string GenerateTag();
```

#### 6.3.2 序列化和反序列化

```cpp
// 序列化为 SIP 消息字符串
std::string Serialize() const;
```

---

## 7. 故障排除

### 7.1 编译错误

#### 错误：无法找到 asio

**解决方法**：确保 `third_party/asio-1.36.0` 目录存在。

#### 错误：MD5 计算相关错误

**Windows**：确保链接了 `advapi32.lib`
```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE advapi32)
```

**Linux**：确保安装了 OpenSSL
```bash
sudo apt-get install libssl-dev
```

### 7.2 运行时错误

#### 错误：注册失败（401 Unauthorized）

**可能原因**：
1. 用户名或密码错误
2. 认证域（realm）错误
3. 服务器不支持 Digest MD5 认证

**解决方法**：
1. 检查配置中的 `username` 和 `password`
2. 检查 `sip_realm` 是否正确
3. 使用抓包工具（如 Wireshark）查看 SIP 消息

#### 错误：无法接收 SIP 消息

**可能原因**：
1. 防火墙阻止了 SIP 端口
2. 本地端口已被占用
3. IP 地址配置错误

**解决方法**：
1. 检查防火墙设置
2. 更换本地端口
3. 检查本地 IP 地址配置

### 7.3 调试技巧

#### 启用 SIP 消息日志

在 `sip_agent.cpp` 中取消注释以下行：
```cpp
// std::cout << "[SIP] 发送消息:\n" << message << std::endl;
// std::cout << "[SIP] 接收消息:\n" << message << std::endl;
```

#### 使用 Wireshark 抓包

过滤 SIP 消息：
```
sip
```

---

## 8. 附录

### 8.1 GB28181 常用 XML 报文

#### 8.1.1 Catalog 查询

**请求**：
```xml
<?xml version="1.0" encoding="GB2312"?>
<CmdType>Catalog</CmdType>
<SN>1234</SN>
<DeviceID>34020000002000000001</DeviceID>
```

**响应**：
```xml
<?xml version="1.0" encoding="GB2312"?>
<Response>
  <CmdType>Catalog</CmdType>
  <SN>1234</SN>
  <DeviceID>34020000001320000001</DeviceID>
  <SumNum>1</SumNum>
  <Cmd>
    <Info>
      <CmdItem>
        <DeviceID>34020000001320000001</DeviceID>
        <Name>AI Camera</Name>
        <Manufacturer>ai-camera</Manufacturer>
        <Model>AI-Cam-1.0</Model>
        <Owner>owner</Owner>
        <CivilCode>340200</CivilCode>
        <Address>Shanghai</Address>
        <Parental>0</Parental>
        <SafetyWay>0</SafetyWay>
        <RegisterWay>1</RegisterWay>
        <Secrecy>0</Secrecy>
        <Status>ON</Status>
      </CmdItem>
    </Info>
  </Cmd>
</Response>
```

#### 8.1.2 DeviceInfo 查询

**请求**：
```xml
<?xml version="1.0" encoding="GB2312"?>
<CmdType>DeviceInfo</CmdType>
<SN>1234</SN>
<DeviceID>34020000001320000001</DeviceID>
```

**响应**：
```xml
<?xml version="1.0" encoding="GB2312"?>
<Response>
  <CmdType>DeviceInfo</CmdType>
  <SN>1234</SN>
  <DeviceID>34020000001320000001</DeviceID>
  <DeviceInfo>
    <DeviceName>AI Camera</DeviceName>
    <Manufacturer>ai-camera</Manufacturer>
    <Model>AI-Cam-1.0</Model>
    <Firmware>1.0.0</Firmware>
    <SerialNumber>123456</SerialNumber>
  </DeviceInfo>
</Response>
```

### 8.2 SIP 状态码

| 状态码 | 原因短语 | 说明 |
|--------|----------|------|
| 100    | Trying | 正在尝试 |
| 180    | Ringing | 响铃 |
| 200    | OK | 成功 |
| 401    | Unauthorized | 未授权 |
| 403    | Forbidden | 禁止 |
| 404    | Not Found | 未找到 |
| 408    | Request Timeout | 请求超时 |
| 480    | Temporarily Unavailable | 暂时不可用 |
| 500    | Internal Server Error | 内部错误 |
| 503    | Service Unavailable | 服务不可用 |

### 8.3 参考文档

1. **GB/T 28181-2016**：《安全防范视频监控联网系统信息传输、交换、控制技术要求》
2. **RFC 3261**：SIP: Session Initiation Protocol
3. **RFC 2617**：HTTP Authentication: Basic and Digest Access Authentication
4. **RFC 4566**：SDP: Session Description Protocol

---

## 结束语

本实现提供了轻量级的 GB28181 SIP 信令功能，适合嵌入式设备和资源受限环境。如有问题或建议，欢迎反馈。

**作者**：AI Assistant
**日期**：2026-06-24
**版本**：1.0
