# mDNS 模块测试计划

## 1. 测试概述

本文档详细描述 mDNS 模块的测试计划，包括单元测试、集成测试和实际网络测试。

## 2. 测试环境

### 2.1 开发环境
- **操作系统**: Windows 10/11 或 Linux (Ubuntu 20.04+)
- **编译器**: MSVC 2019+ 或 GCC 9+
- **构建工具**: CMake 3.16+
- **依赖库**: ASIO 1.36.0, spdlog 1.17.0

### 2.2 测试网络环境
- **局域网**: 支持 mDNS 的局域网环境
- **测试设备**:
  - 摄像头设备（运行 ai-camera）
  - 客户端设备（Windows PC、Linux PC、手机等）
  - 网络设备（支持多播路由）

### 2.3 测试工具
- **Wireshark**: 抓包分析 mDNS 报文
- **avahi-browse** (Linux): 验证服务发现
- **dns-sd** (macOS): 验证服务发现
- **Bonjour Browser** (Windows): 查看 mDNS 服务

## 3. 测试分类

### 3.1 单元测试

#### 3.1.1 DNS 报文编解码测试

**测试文件**: `tests/test_dns_message.cpp`

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-DNS-001 | 编码简单域名 "example.com" | 编码结果符合 DNS 标签格式 |
| TC-DNS-002 | 解码 DNS 标签格式域名 | 正确解码为 "example.com" |
| TC-DNS-003 | 编码带压缩的域名 | 正确生成压缩指针 |
| TC-DNS-004 | 解码带压缩的域名 | 正确解压缩指针 |
| TC-DNS-005 | 编码 A 记录 | 生成正确的 A 记录数据 |
| TC-DNS-006 | 解码 A 记录 | 正确解析 IPv4 地址 |
| TC-DNS-007 | 编码 PTR 记录 | 生成正确的 PTR 记录 |
| TC-DNS-008 | 解码 PTR 记录 | 正确解析指针名称 |
| TC-DNS-009 | 编码 SRV 记录 | 生成正确的 SRV 记录 |
| TC-DNS-010 | 解码 SRV 记录 | 正确解析优先级、权重、端口、目标 |
| TC-DNS-011 | 编码 TXT 记录 | 生成正确的 TXT 记录 |
| TC-DNS-012 | 解码 TXT 记录 | 正确解析键值对 |
| TC-DNS-013 | 编码完整 DNS 消息 | 生成符合 RFC 的 DNS 报文 |
| TC-DNS-014 | 解码完整 DNS 消息 | 正确解析所有记录 |
| TC-DNS-015 | 编码后解码（往返测试） | 解码结果与原始数据一致 |

#### 3.1.2 类型定义测试

**测试文件**: `tests/test_mdns_types.cpp`

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-TYPE-001 | `ServiceTypeToString()` 转换 | 返回正确的 DNS-SD 字符串 |
| TC-TYPE-002 | `StringToServiceType()` 转换 | 返回正确的枚举值 |
| TC-TYPE-003 | `ServiceInfo::GetFullTypeString()` | 返回完整的服务类型字符串 |
| TC-TYPE-004 | `ServiceInfo::SetTxtRecord()` | 正确设置 TXT 记录 |
| TC-TYPE-005 | `ServiceInfo::GetTxtRecord()` | 正确获取 TXT 记录 |

### 3.2 集成测试

#### 3.2.1 mDNS 服务初始化测试

**测试文件**: `tests/test_mdns_integration.cpp`

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-INIT-001 | 初始化 mDNS 服务 | 返回 true，状态为已初始化 |
| TC-INIT-002 | 重复初始化 | 返回 false |
| TC-INIT-003 | 未初始化时调用其他 API | 返回 false 或 -1 |
| TC-INIT-004 | 初始化后关闭 | 状态为未初始化 |
| TC-INIT-005 | 关闭后重新初始化 | 返回 true |
| TC-INIT-006 | 获取主机名 | 返回正确格式的主机名（如 "ai-camera.local."） |

#### 3.2.2 服务注册测试

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-REG-001 | 注册 RTSP 服务 | 返回 true，服务出现在已注册列表 |
| TC-REG-002 | 注册 HTTP 服务 | 返回 true |
| TC-REG-003 | 注册带 TXT 记录的服务 | TXT 记录正确存储 |
| TC-REG-004 | 注册相同名称的服务 | 覆盖旧服务或返回 false |
| TC-REG-005 | 注销已注册的服务 | 返回 true，服务从列表移除 |
| TC-REG-006 | 注销未注册的服务 | 返回 false |
| TC-REG-007 | 更新 TXT 记录 | TXT 记录正确更新 |
| TC-REG-008 | 在网络中验证注册 | 其他设备能看到该服务 |

#### 3.2.3 服务发现测试

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-BROWSE-001 | 开始浏览 RTSP 服务 | 返回有效的 browse_id |
| TC-BROWSE-002 | 停止浏览 | 成功停止，不再接收回调 |
| TC-BROWSE-003 | 发现服务时触发回调 | `on_found` 回调被调用 |
| TC-BROWSE-004 | 服务消失时触发回调 | `on_lost` 回调被调用 |
| TC-BROWSE-005 | 同时浏览多种服务类型 | 所有浏览会话正常工作 |
| TC-BROWSE-006 | 在网络中发现其他设备服务 | 能发现局域网中的服务 |

#### 3.2.4 服务解析测试

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-RESOLVE-001 | 解析已注册的服务 | 返回正确的 IP 和端口 |
| TC-RESOLVE-002 | 解析发现的服务的 | 返回正确的 IP 和端口 |
| TC-RESOLVE-003 | 解析不存在的服务 | 回调返回空信息或失败 |

### 3.3 网络集成测试

#### 3.3.1 多播通信测试

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-NET-001 | 发送 mDNS 报文 | Wireshark 能抓到报文 |
| TC-NET-002 | 接收 mDNS 报文 | 能收到其他设备的 mDNS 报文 |
| TC-NET-003 | 多播组加入 | socket 成功加入 224.0.0.251 |
| TC-NET-004 | 端口绑定 | 成功绑定到 5353 端口 |

#### 3.3.2 与其他 mDNS 实现互操作测试

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-INTEROP-001 | 与 avahi-daemon 互操作 | 能互相发现服务 |
| TC-INTEROP-002 | 与 Bonjour 互操作 | 能互相发现服务 |
| TC-INTEROP-003 | 与 Android NSD 互操作 | Android 设备能发现摄像头服务 |
| TC-INTEROP-004 | 与 macOS mDNSResponder 互操作 | macOS 能发现摄像头服务 |

### 3.4 性能和压力测试

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-PERF-001 | 注册 100 个服务 | 内存占用合理，无泄漏 |
| TC-PERF-002 | 同时浏览 10 种服务类型 | CPU 占用合理 |
| TC-PERF-003 | 高频率服务注册/注销 | 无崩溃或死锁 |
| TC-PERF-004 | 长时间运行（24 小时） | 无内存泄漏，功能正常 |
| TC-PERF-005 | 网络延迟/丢包环境 | 能正确处理超时和重传 |

### 3.5 异常和边界测试

| 测试用例 | 描述 | 预期结果 |
|---------|------|-----------|
| TC-ERR-001 | 网络接口不可用 | 优雅失败，返回错误 |
| TC-ERR-002 | 端口被占用 | 返回初始化失败 |
| TC-ERR-003 | 无效的服务名称 | 返回 false |
| TC-ERR-004 | 无效的端口号（0 或 >65535） | 返回 false |
| TC-ERR-005 | 超长 TXT 记录 | 返回 false 或截断 |
| TC-ERR-006 | 恶意构造的 DNS 报文 | 不崩溃，返回 nullopt |
| TC-ERR-007 | 多线程并发调用 | 无竞态条件或死锁 |

## 4. 测试执行计划

### 4.1 第一阶段：单元测试（1-2 天）
- 实现 `test_dns_message.cpp`
- 实现 `test_mdns_types.cpp`
- 运行测试并修复问题

### 4.2 第二阶段：集成测试（2-3 天）
- 实现 `test_mdns_integration.cpp`
- 测试服务初始化、注册、发现、解析
- 修复发现的问题

### 4.3 第三阶段：网络测试（2-3 天）
- 在真实网络中测试
- 使用 Wireshark 验证报文
- 与其他 mDNS 实现互操作测试

### 4.4 第四阶段：性能和压力测试（1-2 天）
- 长时间运行测试
- 高负载测试
- 内存泄漏检测

## 5. 测试代码组织

### 5.1 目录结构
```
tests/
├── test_dns_message.cpp      # DNS 报文编解码测试
├── test_mdns_types.cpp      # 类型定义测试
├── test_mdns_integration.cpp # 集成测试
├── test_mdns_network.cpp    # 网络测试
└── test_mdns_performance.cpp # 性能测试
```

### 5.2 构建配置
在 `CMakeLists.txt` 中添加：
```cmake
# DNS 报文编解码测试
add_executable(test_dns_message tests/test_dns_message.cpp)
target_link_libraries(test_dns_message PRIVATE ...)

# mDNS 集成测试
add_executable(test_mdns_integration tests/test_mdns_integration.cpp
    src/mdns/mdns_service.cpp
    src/mdns/mdns_impl.cpp
    src/mdns/dns_message.cpp
)
target_link_libraries(test_mdns_integration PRIVATE ...)
```

## 6. 测试自动化

### 6.1 CI/CD 集成
- 在每次提交时运行单元测试
- 定期运行集成测试

### 6.2 测试脚本
创建 `scripts/run_tests.sh`:
```bash
#!/bin/bash
cd build
cmake .. -DBUILD_TESTS=ON
make -j4

# 运行单元测试
./test_dns_message
./test_mdns_types

# 运行集成测试
./test_mdns_integration
```

## 7. 测试报告

每次测试后生成报告，包括：
- 测试用例总数
- 通过/失败数量
- 代码覆盖率
- 性能基线

## 8. 已知问题和限制

### 8.1 当前实现限制
1. **仅支持 IPv4**: 当前仅实现 IPv4 多播，IPv6 待实现
2. **简单冲突处理**: 服务名称冲突处理较简单
3. **无持久化**: 服务信息不持久化，重启后丢失

### 8.2 未来改进
1. 实现完整的 mDNS 冲突解决
2. 支持 IPv6
3. 实现服务 TTL 过期处理
4. 支持多网络接口

## 9. 附录：测试检查清单

- [ ] 所有单元测试通过
- [ ] 所有集成测试通过
- [ ] 网络测试通过
- [ ] 性能测试通过
- [ ] 异常测试通过
- [ ] 代码覆盖率 > 80%
- [ ] 无内存泄漏
- [ ] 与其他 mDNS 实现互操作正常

---

**文档版本**: 1.0  
**创建日期**: 2026-06-25  
**作者**: AI Assistant
