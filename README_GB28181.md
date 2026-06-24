# GB28181 模块使用指南

## 快速开始

### 1. 配置

编辑 `src/main.cpp` 中的 GB28181 配置：

```cpp
gb28181::Gb28181Config gb28181_cfg;
gb28181_cfg.device_id = "34020000001320000001";  // 你的 20 位设备编码
gb28181_cfg.server_ip = "192.168.1.10";          // SIP 服务器 IP
gb28181_cfg.server_port = 5060;                   // SIP 服务器端口
gb28181_cfg.local_sip_port = 5060;                // 本地端口
gb28181_cfg.sip_realm = "3402000000";            // SIP 域
gb28181_cfg.username = "34020000001320000001";   // 用户名
gb28181_cfg.password = "123456";                  // 密码
```

### 2. 编译

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### 3. 运行

```bash
./ai-camera
```

## 功能列表

| 功能 | 状态 | 说明 |
|------|------|------|
| SIP REGISTER | ✅ | 注册与保活 |
| SIP MESSAGE | ✅ | XML 报文传输 |
| SIP INVITE | ✅ | 实时点播 |
| SIP BYE | ✅ | 结束会话 |
| SIP SUBSCRIBE | ✅ | 订阅通知 |
| SIP 认证 | ✅ | Digest MD5 |
| Catalog 查询 | ✅ | 目录查询响应 |
| DeviceInfo 查询 | ✅ | 设备信息查询响应 |
| 心跳保活 | ✅ | 自动保活 |
| SDP 处理 | 🚧 | 待完善 |
| RTP 推流 | 🚧 | 待实现 |

## 项目结构

```
include/ai-camera/gb28181/
├── gb28181_config.h      # 配置
├── gb28181_types.h       # 类型定义
├── sip_message.h         # SIP 消息
├── sip_agent.h           # SIP 代理
└── gb28181_manager.h     # 管理器单例

src/gb28181/
├── sip_message.cpp
├── sip_agent.cpp
└── gb28181_manager.cpp
```

## 依赖

- **asio** (header-only) - 已包含在 `third_party/asio-1.36.0`
- **tinyxml2** - 已包含在 `third_party/tinyxml2`
- **Windows CryptAPI** / **OpenSSL** - 用于 MD5 计算

## 文档

- [实现指南](docs/gb28181_implementation_guide.md) - 详细的实现说明
- [设置指南](docs/gb28181_setup_guide.md) - 任务完成报告

## 常见问题

### Q: 编译报错找不到 asio？

确保 `third_party/asio-1.36.0` 目录存在。

### Q: 注册失败（401 Unauthorized）？

检查用户名、密码、SIP 域是否正确。

### Q: 无法接收 SIP 消息？

检查防火墙设置，确保 SIP 端口（默认 5060）已开放。

## 许可

MIT License

---

**作者**: AI Assistant  
**日期**: 2026-06-24
