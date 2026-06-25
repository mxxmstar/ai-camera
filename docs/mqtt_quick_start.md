# MQTT 模块快速使用指南

## 一分钟上手

### 1. 启动本地 Broker（mosquitto）

```powershell
# 安装 mosquitto 后，创建配置文件 C:\Program Files\mosquitto\mosquitto.conf
# 内容：
# listener 1883
# protocol mqtt
# allow_anonymous true

cd "C:\Program Files\mosquitto"
.\mosquitto.exe -c mosquitto.conf -v
```

### 2. 编译并运行测试程序

```powershell
cd e:\project\ai-camera\build
cmake --build . --config Debug --target test_mqtt
.\Debug\test_mqtt.exe
```

### 3. 用 MQTT Explorer 验证

1. 下载：https://mqtt-explorer.com/
2. 连接：`127.0.0.1:1883`
3. 订阅：`aicamera/AICAM-TEST/#`
4. 发布命令到 `aicamera/AICAM-TEST/command/down`：
   ```json
   {"msg_id":"test-001","cmd":"reboot","params":{}}
   ```

---

## API 速查

### 初始化

```cpp
#include "mqtt/mqtt_manager.h"

mqtt::MqttConfig config;
config.broker_host   = "127.0.0.1";
config.broker_port   = 1883;
config.client_id     = "my-device-001";
config.device_id     = "AICAM-001";
config.keep_alive_seconds = 60;

auto& mgr = mqtt::MqttManager::Instance();
mgr.SetConfig(config);
```

### 设置回调

```cpp
mgr.SetOnConnect([](bool connected) {
    std::cout << (connected ? "MQTT online" : "MQTT offline") << std::endl;
});

mgr.SetOnCommand([](const std::string& msg_id,
                     const std::string& cmd,
                     const std::string& params) {
    std::cout << "Command: " << cmd << std::endl;
    // 回复响应
    mgr.PublishCommandResp(msg_id, 0, "OK");
});

mgr.SetOnOtaNotify([](const std::string& version,
                       const std::string& url,
                       const std::string& md5,
                       uint64_t size) {
    std::cout << "OTA: " << version << std::endl;
});
```

### 启动 / 停止

```cpp
mgr.Start();   // 启动（阻塞，建议在独立线程调用）
mgr.Stop();    // 停止
mgr.IsConnected();  // 查询连接状态
```

### 上报数据

```cpp
// 上报属性
mgr.PublishProperty(R"({"temperature":25.5,"humidity":60.0})");

// 上报事件
mgr.PublishEvent("person_detect", R"({"confidence":0.95,"bbox":[100,200,300,400]})");
```

---

## Topic 速查

| Topic | 方向 | 说明 |
|-------|--------|------|
| `aicamera/{device_id}/property/post` | 设备→云 | 属性上报 |
| `aicamera/{device_id}/event/post` | 设备→云 | 事件上报 |
| `aicamera/{device_id}/status` | 设备→云 | 在线状态 |
| `aicamera/{device_id}/command/down` | 云→设备 | 命令下发 |
| `aicamera/{device_id}/command/resp` | 设备→云 | 命令响应 |
| `aicamera/{device_id}/ota/notify` | 云→设备 | OTA 通知 |
| `aicamera/{device_id}/ota/progress` | 设备→云 | OTA 进度 |
| `aicamera/{device_id}/ota/result` | 设备→云 | OTA 结果 |

---

## 命令 JSON 格式

**云端下发**（到 `command/down`）：
```json
{
  "msg_id": "req-001",
  "cmd": "reboot",
  "params": {}
}
```

**设备响应**（到 `command/resp`）：
```json
{
  "msg_id": "req-001",
  "code": 0,
  "message": "OK"
}
```

---

## OTA 通知 JSON 格式

**云端下发**（到 `ota/notify`）：
```json
{
  "version": "v2.0.0",
  "url": "http://example.com/firmware.bin",
  "md5": "abc123...",
  "size": 1048576
}
```

---

## 故障排除

| 问题 | 原因 | 解决 |
|------|------|------|
| 连接失败 | mosquitto 未启动 | 启动 mosquitto |
| 收不到消息 | Topic 不匹配 | 用 `#` 通配符订阅 |
| 命令无反应 | Publish Topic 错误 | 必须发布到 `.../command/down` |
| 编译报错编码 | 文件非 UTF-8 | CMakeLists 已加 `/utf-8` 选项 |
| JSON 解析为空 | 字段名大小写错误 | 确保用 `"msg_id"` 而非 `"msgId"` |
