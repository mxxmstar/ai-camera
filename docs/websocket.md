# WebSocket 模块文档

本模块基于项目已有的 **standalone Asio 1.36.0** 自行实现了 WebSocket 协议（RFC 6455），
无需引入任何额外的第三方库。

> ## 关于 WebSocket++ 的说明
>
> 原计划引入 WebSocket++ 0.8.2，但经测试发现它与本项目使用的 **standalone Asio 1.36.0
> （2025 年版）不兼容**：WebSocket++ 0.8.2 最后更新于 2018 年，其内部大量使用
> `asio::io_service`、`io_service::strand`、`io_service::work` 等类型，而这些类型
> 在新版 standalone Asio 中已被移除（重命名为 `io_context`、`strand`、
> `executor_work_guard` 等）。强行使用需对 WebSocket++ 打数十处补丁，违背"方便引入"原则。
>
> 因此本模块改为 **纯 standalone Asio 实现**，代码风格与项目现有的 `server/http/`、
> `net/` 模块保持一致，零外部依赖。

---

## 1. 目录结构

```
ai-camera/
├── include/
│   ├── ws/
│   │   └── sha1.h                      # SHA-1 + Base64 密码学原语（握手用）
│   └── server/ws/
│       ├── ws_types.hpp                # 公共类型（OpCode / CloseCode / Message）
│       ├── ws_frame.hpp                # 帧编解码（RFC 6455 第 5 节）
│       ├── ws_handshake.hpp            # HTTP 升级握手（Sec-WebSocket-Accept 计算）
│       ├── ws_session.hpp              # 单连接会话（异步读写 / 分片重组 / 心跳）
│       └── ws_server.hpp               # 服务器（accept / 会话管理 / 广播）
├── src/
│   ├── ws/
│   │   └── sha1.cpp                    # SHA-1 + Base64 实现
│   └── server/ws/
│       ├── ws_session.cpp              # 会话实现
│       └── ws_server.cpp               # 服务器实现
└── docs/
    └── websocket.md                    # 本文档
```

### 模块职责划分

| 文件                  | 职责                                       | 是否纯头文件 |
| --------------------- | ------------------------------------------ | ------------ |
| `ws_types.hpp`        | 枚举与结构体定义，无逻辑                   | 是           |
| `ws_frame.hpp`        | 帧头解析、掩码处理、帧编码                 | 是（inline） |
| `ws_handshake.hpp`    | HTTP 请求解析、握手校验、Accept 计算、响应 | 是（inline） |
| `ws_session.hpp/cpp`  | 异步连接管理、消息重组、收发回调           | 否           |
| `ws_server.hpp/cpp`   | 监听端口、会话生命周期管理、广播           | 否           |
| `sha1.h/cpp`          | SHA-1 摘要与 Base64 编码                   | 否           |

---

## 2. 快速上手

### 2.1 最小示例：回显服务器

```cpp
#include "server/ws/ws_server.hpp"
#include <iostream>

int main() {
    // 传入 nullptr 表示由 WsServer 内部创建并管理 io_context 和后台线程
    ws::WsServer server(nullptr, "0.0.0.0", 9002);

    server.SetOnOpen([](std::shared_ptr<ws::WsSession> s) {
        std::cout << "[open] " << s->GetRemoteAddress()
                  << " (id=" << s->GetId() << ")\n";
    });

    server.SetOnMessage([](std::shared_ptr<ws::WsSession> s, const ws::Message& m) {
        if (m.IsText()) {
            std::cout << "[recv text] " << m.AsString() << "\n";
            s->SendText("echo: " + m.AsString());  // 回显
        } else {
            s->SendBinary(m.payload);
        }
    });

    server.SetOnClose([](std::shared_ptr<ws::WsSession> s, ws::CloseCode code) {
        std::cout << "[close] id=" << s->GetId()
                  << " code=" << static_cast<int>(code) << "\n";
    });

    ws::SessionOptions options;
    options.max_message_bytes = 2 * 1024 * 1024;
    options.heartbeat_interval = std::chrono::seconds(10);
    options.heartbeat_timeout = std::chrono::seconds(30);
    server.SetSessionOptions(options);

    server.Start();          // 非阻塞，内部已启动后台线程

    // 主线程做其它事情...
    std::cout << "WebSocket server running on ws://localhost:9002\n";
    std::cin.get();
    server.Stop();
    return 0;
}
```

### 2.2 集成到现有 io_context（与 HTTP/RTSP 共用）

```cpp
asio::io_context io_ctx;

// HTTP 服务器
http::Server http_srv("0.0.0.0", 8080);

// WebSocket 服务器，共用同一个 io_context
ws::WsServer ws_srv(&io_ctx, "0.0.0.0", 9002);
ws_srv.SetOnMessage([](std::shared_ptr<ws::WsSession> s, const ws::Message& m) {
    s->SendText(m.AsString());
});
ws_srv.Start();

io_ctx.run();  // 阻塞运行事件循环
```

### 2.3 在 main.cpp 中与现有服务并存

```cpp
// src/main.cpp 片段
#include "server/ws/ws_server.hpp"

static std::unique_ptr<ws::WsServer> g_ws_server;

int main() {
    // ... 现有 RTSP / HTTP 初始化 ...

    // 创建 WebSocket 服务器（自管理 io_context）
    g_ws_server = std::make_unique<ws::WsServer>(nullptr, "0.0.0.0", 9002);
    g_ws_server->SetOnMessage([](std::shared_ptr<ws::WsSession> s,
                                  const ws::Message& m) {
        // 例如：将收到的命令转发给 RTSP 模块
        s->SendText("ok");
    });
    g_ws_server->Start();

    // ... 其余代码 ...
}
```

---

## 3. API 参考

### 3.1 `ws::WsServer`

| 方法                                    | 说明                                              |
| --------------------------------------- | ------------------------------------------------- |
| `WsServer(io_ctx*, addr, port)`         | 构造；`io_ctx` 为 `nullptr` 时内部自管理线程      |
| `Start()`                               | 开始接受连接（非阻塞）                            |
| `Stop()`                                | 停止并关闭所有连接                                |
| `SetOnOpen(handler)`                    | 设置连接建立回调                                  |
| `SetOnMessage(handler)`                 | 设置消息接收回调                                  |
| `SetOnPing(handler)`                    | 设置 Ping 回调（默认自动回复 Pong）              |
| `SetOnClose(handler)`                   | 设置连接关闭回调                                  |
| `SetSessionOptions(options)`            | 配置消息大小限制、握手上限与心跳参数              |
| `GetSessionOptions()`                   | 获取当前会话配置                                  |
| `BroadcastText(text)`                   | 向所有连接广播文本                                |
| `BroadcastBinary(data)`                 | 向所有连接广播二进制                              |
| `GetSessionCount()`                     | 获取当前活动连接数                                |

### 3.2 `ws::WsSession`

| 方法                          | 说明                                              |
| ----------------------------- | ------------------------------------------------- |
| `SendText(text)`              | 发送文本消息                                      |
| `SendBinary(data)`            | 发送二进制消息                                    |
| `SendPing(payload)`           | 发送 Ping（对端应回复 Pong）                      |
| `Close(code, reason)`         | 主动关闭连接                                      |
| `GetRemoteAddress()`          | 获取远端 "ip:port"                                |
| `GetId()`                     | 获取唯一会话 ID                                   |
| `IsOpen()`                    | 连接是否活动                                      |

### 3.3 `ws::Message`

| 成员/方法        | 说明                          |
| ---------------- | ----------------------------- |
| `opcode`         | 消息类型（Text / Binary）     |
| `payload`        | 原始载荷字节                  |
| `IsText()`       | 是否文本                      |
| `IsBinary()`     | 是否二进制                    |
| `AsString()`     | 载荷转为 std::string          |

### 3.4 `ws::SessionOptions`

| 字段                     | 默认值    | 说明                         |
| ------------------------ | --------- | ---------------------------- |
| `max_handshake_bytes`    | `16 KB`   | HTTP 握手头部最大累计大小    |
| `max_frame_buffer_bytes` | `2 MB`    | 单连接帧缓冲上限             |
| `max_message_bytes`      | `1 MB`    | 单条重组消息最大大小         |
| `heartbeat_interval`     | `15 s`    | 空闲多久后主动发送 Ping      |
| `heartbeat_timeout`      | `45 s`    | 等待 Pong 的最大超时时间     |

### 3.5 枚举

- `ws::OpCode`：`Text` / `Binary` / `Close` / `Ping` / `Pong` / `Continuation`
- `ws::CloseCode`：`NormalClosure(1000)` / `GoingAway(1001)` / `ProtocolError(1002)` ...

---

## 4. 协议实现要点

### 4.1 握手流程

```
客户端                              服务器
  |  GET /path HTTP/1.1               |
  |  Upgrade: websocket               |
  |  Connection: Upgrade              |
  |  Sec-WebSocket-Key: <base64>      |
  |  Sec-WebSocket-Version: 13        |
  |---------------------------------->|
  |                                   | 校验请求头
  |                                   | 计算 Accept = Base64(SHA1(Key + GUID))
  |  HTTP/1.1 101 Switching Protocols |
  |  Upgrade: websocket               |
  |  Connection: Upgrade              |
  |  Sec-WebSocket-Accept: <base64>   |
  |<----------------------------------|
  |          WebSocket 双工通信        |
```

- GUID 固定为 `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`（RFC 6455 规定）
- SHA-1 + Base64 由 `ws/sha1.h` 自带实现，不依赖 OpenSSL

### 4.2 帧格式

见 `ws_frame.hpp` 文件头注释的详细图示。关键点：

- **客户端→服务器的帧必须掩码**（MASK=1），服务器→客户端的帧不得掩码
- 载荷长度三种编码：≤125 直接编码、126 后接 2 字节、127 后接 8 字节
- 控制帧（Close/Ping/Pong）不可分片，载荷 ≤125 字节
- 数据帧可分片：首片 Text/Binary + 若干 Continuation + FIN=1 结束

### 4.3 分片重组

`WsSession` 内部维护 `msg_payload_` 累积缓冲，收到 FIN=1 时才触发
`MessageHandler`，对上层屏蔽分片细节。

### 4.4 大小限制与心跳

- 握手阶段会累计读取 HTTP 头，超过 `max_handshake_bytes` 会返回 `400 Bad Request`
- 帧读取阶段会限制 `frame_buf_` 的累计大小，超过 `max_frame_buffer_bytes` 会以 `1009` 关闭
- 消息分片重组时会检查累计长度，超过 `max_message_bytes` 会以 `1009` 关闭
- 会话空闲达到 `heartbeat_interval` 时自动发送 `Ping`
- 发送 `Ping` 后若在 `heartbeat_timeout` 内未收到 `Pong`，连接会被关闭

### 4.5 关闭流程

```
主动关闭方                  被动关闭方
  |  Close 帧 (code, reason)     |
  |----------------------------->|
  |                              | 收到 Close，回复 Close 帧
  |  Close 帧 (相同 code)        |
  |<-----------------------------|
  |  关闭 TCP 连接               | 关闭 TCP 连接
```

本实现中，`Close(code)` 会先发送 Close 帧再关闭 socket；
收到对端 Close 时也会自动回复后再关闭。

### 4.6 线程安全

- 每个 `WsSession` 使用独立的 `asio::strand` 串行化发送操作
- `Send*()` 可在任意线程调用
- `WsServer` 的会话表用互斥锁保护，`Broadcast*()` 线程安全

---

## 5. 测试方法

### 5.1 使用浏览器 JS 测试

```html
<script>
const ws = new WebSocket("ws://localhost:9002");
ws.onopen = () => ws.send("Hello from browser!");
ws.onmessage = (e) => console.log("recv:", e.data);
ws.onclose = (e) => console.log("closed:", e.code, e.reason);
</script>
```

### 5.2 使用 wscat（需 Node.js）

```bash
npx wscat -c ws://localhost:9002
```

### 5.3 使用 curl 测试握手

```bash
curl -i -N -H "Connection: Upgrade" -H "Upgrade: websocket" \
     -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
     -H "Sec-WebSocket-Version: 13" \
     http://localhost:9002
```

预期返回：
```
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
```

---

## 6. 构建说明

本模块的源文件位于 `src/ws/` 和 `src/server/ws/` 下，已被 `CMakeLists.txt`
中的 `file(GLOB_RECURSE SOURCES src/*.cpp)` 自动包含，**无需修改构建脚本**。

头文件搜索路径 `include/` 已在 `target_include_directories` 中配置，
因此可直接 `#include "server/ws/ws_server.hpp"` 和 `#include "ws/sha1.h"`。

```bash
cd build
cmake ..
cmake --build .
```

---

## 7. 扩展建议

| 需求                     | 实现位置                          |
| ------------------------ | --------------------------------- |
| WSS（TLS 加密）          | 在 socket 外包一层 `asio::ssl`    |
| 消息大小限制             | `ProcessFrameBuffer` 中校验长度   |
| 心跳超时检测             | 为每个会话添加 `steady_timer`     |
| 子协议协商               | 握手时解析 `Sec-WebSocket-Protocol`|
| 压缩扩展（permessage-deflate） | 需引入 zlib，较复杂          |
