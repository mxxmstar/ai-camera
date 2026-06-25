# ai-camera media 层设计文档

## 目录

1. [概述](#1-概述)
2. [设计目标](#2-设计目标)
3. [架构设计](#3-架构设计)
4. [文件结构](#4-文件结构)
5. [C API 使用指南](#5-c-api-使用指南)
6. [C++ API 使用指南](#6-c-api-使用指南)
7. [C/C++ 互操作](#7-cc-互操作)
8. [NALSourceAdapter 使用指南](#8-nalsourceadapter-使用指南)
9. [编译与测试](#9-编译与测试)
10. [常见问题解答](#10-常见问题解答)

---

## 1. 概述

`media` 层是 `ai-camera` 项目的核心媒体数据处理层，提供了一套完整的 C/C++ 混合编程接口，用于：

- **视频帧管理**：处理原始视频数据（如 YUV、RGB 格式）
- **媒体包管理**：处理编码后的媒体数据（如 H.264、H.265、AAC）
- **缓冲区管理**：提供引用计数的零拷贝缓冲区
- **RTP 集成**：通过 `NALSourceAdapter` 与现有 GB28181/RTSP 模块无缝集成

### 1.1 核心特性

- ✅ **C/C++ 混合编程**：提供 C API（供 V4L2 驱动调用）和 C++ API（供上层应用调用）
- ✅ **零拷贝设计**：使用引用计数和不透明指针，避免数据拷贝
- ✅ **类型安全**：C++ API 提供类型安全的接口
- ✅ **线程安全**：`NALSourceAdapter` 支持线程安全的队列操作
- ✅ **后端扩展**：支持 V4L2、FFmpeg 等后端，通过 `BackendHandle` 扩展

---

## 2. 设计目标

### 2.1 分层架构

```
┌──────────────────────────────────────────────────────────┐
│          上层应用（GB28181/RTSP/MQTT）                │
└──────────────────────────────────────────────────────────┘
                         ▲
                         │ 使用 C++ API
┌──────────────────────────────────────────────────────────┐
│              C++ API 层（media_frame.hpp 等）          │
│  - 类型安全接口                                        │
│  - 面向对象设计                                        │
│  - 自动内存管理                                        │
└──────────────────────────────────────────────────────────┘
                         ▲
                         │ 使用 IMediaBuffer 接口
┌──────────────────────────────────────────────────────────┐
│          C/C++ 适配器层（MediaBufferAdapter）           │
└──────────────────────────────────────────────────────────┘
                         ▲
                         │ 使用 C API
┌──────────────────────────────────────────────────────────┐
│              C API 层（media_frame.h 等）              │
│  - 不透明指针                                          │
│  - 引用计数                                            │
│  - 兼容 C99+                                          │
└──────────────────────────────────────────────────────────┘
                         ▲
                         │ 调用
┌──────────────────────────────────────────────────────────┐
│           底层驱动（V4L2/DMA/硬件加速）                │
└──────────────────────────────────────────────────────────┘
```

### 2.2 设计原则

1. **不透明指针模式**：C API 使用不透明指针隐藏实现细节
2. **引用计数**：所有缓冲区使用引用计数管理生命周期
3. **零拷贝**：尽可能使用浅拷贝（增加引用计数）而非深拷贝
4. **互操作性**：C 和 C++ API 可以无缝互操作
5. **可扩展性**：通过 `BackendHandle` 支持不同后端

---

## 3. 架构设计

### 3.1 核心类/结构体关系

```
IMediaBuffer (接口)
    ↑
    |
MediaBufferAdapter (适配器)
    - 包装 media_buffer_t
    - 实现 IMediaBuffer 接口


media_buffer_t (C 缓冲区)
    - data: 数据指针
    - size: 数据大小
    - ref_count: 引用计数
    - free_func: 释放回调函数


MediaFrame (C++ 视频帧类)
    - frame_: media_frame_t
    - plane_buffers_: vector<shared_ptr<IMediaBuffer>>
    - backend_handle_: shared_ptr<BackendHandle>

media_frame_t (C 视频帧结构体)
    - width, height: 分辨率
    - pixel_format: 像素格式
    - buffers[4]: 缓冲区指针数组
    - strides[4]: 步长数组
    - v4l2_*: V4L2 后端数据


MediaPacket (C++ 媒体包类)
    - packet_: media_packet_t
    - buffer_: shared_ptr<IMediaBuffer>
    - backend_handle_: shared_ptr<BackendHandle>

media_packet_t (C 媒体包结构体)
    - media_type: 媒体类型
    - buffer: 缓冲区指针
    - timestamp: 时间戳
    - v4l2_*: V4L2 后端数据


NALSourceAdapter (NAL 单元适配器)
    - 继承 rtp::NALSource
    - 将 MediaPacket 转换为 NALUnitList
    - 线程安全的队列管理
```

### 3.2 内存管理策略

```
创建缓冲区：
1. C API: media_buffer_create() → 分配内存，ref_count = 1
2. C++ API: MediaPacket::AllocBuffer() → 调用 C API，然后创建 MediaBufferAdapter

引用缓冲区：
1. C API: media_buffer_ref() → ref_count++
2. C++ API: shared_ptr 自动管理

释放缓冲区：
1. C API: media_buffer_unref() → ref_count--，如果 ref_count == 0 则释放
2. C++ API: shared_ptr 引用计数为 0 时自动释放 MediaBufferAdapter，
    MediaBufferAdapter 析构时调用 media_buffer_unref()
```

---

## 4. 文件结构

### 4.1 头文件（include/ai-camera/media/）

| 文件 | 说明 |
|------|------|
| `media_buffer.h` | C API 不透明缓冲区定义 |
| `media_frame.h` | C API 视频帧结构体定义 |
| `media_packet.h` | C API 媒体包结构体定义 |
| `i_media_buffer.hpp` | C++ 缓冲区抽象接口 |
| `media_buffer_adapter.hpp` | C++ 缓冲区适配器（连接 C/C++） |
| `media_frame.hpp` | C++ 视频帧类封装 |
| `media_packet.hpp` | C++ 媒体包类封装 |
| `nalsource_adapter.hpp` | NALSource 适配器类定义 |

### 4.2 实现文件（src/media/）

| 文件 | 说明 |
|------|------|
| `media_buffer.c` | C API 缓冲区实现 |
| `media_frame.c` | C API 视频帧操作实现 |
| `media_packet.c` | C API 媒体包操作实现 |
| `media_frame.cpp` | C++ 视频帧类实现 |
| `media_packet.cpp` | C++ 媒体包类实现 |
| `nalsource_adapter.cpp` | NALSource 适配器实现 |

### 4.3 测试文件（tests/）

| 文件 | 说明 |
|------|------|
| `test_media.cpp` | media 层功能测试 |

---

## 5. C API 使用指南

### 5.1 缓冲区操作

```c
#include "ai-camera/media/media_buffer.h"

// 创建缓冲区
media_buffer_t* buffer = media_buffer_create(1024);
if (!buffer) {
    // 处理错误
}

// 获取数据指针
uint8_t* data = media_buffer_data(buffer);

// 写入数据
memcpy(data, source_data, 1024);

// 增加引用计数（浅拷贝）
media_buffer_t* buffer2 = media_buffer_ref(buffer);

// 使用 buffer2...
// 减少引用计数
media_buffer_unref(buffer2);  // buffer2 不再可用
media_buffer_unref(buffer);   // buffer 不再可用

// 使用外部数据（不拷贝）
void* my_data = malloc(1024);
media_buffer_t* buffer3 = media_buffer_create_with_data(
    my_data, 
    1024,
    my_free_callback,  // 可选的释放回调
    my_user_data
);
```

### 5.2 视频帧操作

```c
#include "ai-camera/media/media_frame.h"

// 初始化视频帧
media_frame_t frame;
media_frame_init(&frame);

// 设置属性
frame.width = 1920;
frame.height = 1080;
frame.pixel_format = MEDIA_PIXEL_FORMAT_I420;
frame.timestamp = get_current_timestamp();

// 分配缓冲区
int ret = media_frame_alloc_buffers(&frame);
if (ret != 0) {
    // 处理错误
}

// 访问缓冲区数据
uint8_t* y_plane = media_buffer_data(frame.buffers[0]);
uint8_t* u_plane = media_buffer_data(frame.buffers[1]);
uint8_t* v_plane = media_buffer_data(frame.buffers[2]);

// 清理
media_frame_clear(&frame);
```

### 5.3 媒体包操作

```c
#include "ai-camera/media/media_packet.h"

// 初始化媒体包
media_packet_t packet;
media_packet_init(&packet);

// 设置属性
packet.media_type = MEDIA_TYPE_VIDEO_H264;
packet.timestamp = get_current_timestamp();
packet.is_key_frame = 1;

// 分配缓冲区
int ret = media_packet_alloc_buffer(&packet, 4096);
if (ret != 0) {
    // 处理错误
}

// 写入数据
uint8_t* data = media_buffer_data(packet.buffer);
// ... 填充数据 ...

// 使用外部数据（零拷贝）
void* h264_data = get_h264_data();
media_packet_set_data(
    &packet,
    h264_data,
    h264_data_size,
    my_free_callback,
    my_user_data
);

// 清理
media_packet_clear(&packet);
```

---

## 6. C++ API 使用指南

### 6.1 视频帧操作

```cpp
#include "ai-camera/media/media_frame.hpp"
#include "ai-camera/media/media_buffer_adapter.hpp"

using namespace ai_camera::media;

// 创建视频帧
MediaFrame frame(1920, 1080, MEDIA_PIXEL_FORMAT_I420);

// 设置属性
frame.SetTimestamp(123456789);
frame.SetDuration(33333);  // 33.333 ms (30 FPS)

// 分配缓冲区
bool ret = frame.AllocBuffers();
if (!ret) {
    // 处理错误
}

// 访问缓冲区
auto buffer = frame.GetPlaneBuffer(0);  // Y 平面
if (buffer) {
    uint8_t* y_data = buffer->Data();
    size_t y_size = buffer->Size();
    // ... 处理 Y 数据 ...
}

// 使用自定义缓冲区
auto custom_buffer = std::make_shared<MediaBufferAdapter>(
    media_buffer_create_with_data(my_data, my_size, nullptr, nullptr)
);
frame.SetPlaneBuffer(0, custom_buffer);

// 移动语义（高效）
MediaFrame frame2 = std::move(frame);
```

### 6.2 媒体包操作

```cpp
#include "ai-camera/media/media_packet.hpp"
#include "ai-camera/media/media_buffer_adapter.hpp"

using namespace ai_camera::media;

// 创建媒体包
MediaPacket packet(MEDIA_TYPE_VIDEO_H264);

// 设置属性
packet.SetTimestamp(123456789);
packet.SetKeyFrame(true);
packet.SetWidth(1920);
packet.SetHeight(1080);

// 分配缓冲区
bool ret = packet.AllocBuffer(4096);
if (!ret) {
    // 处理错误
}

// 访问缓冲区
auto buffer = packet.GetBuffer();
if (buffer) {
    uint8_t* data = buffer->Data();
    size_t size = buffer->Size();
    // ... 填充数据 ...
}

// 使用自定义缓冲区
auto custom_buffer = std::make_shared<MediaBufferAdapter>(
    media_buffer_create_with_data(h264_data, h264_size, nullptr, nullptr)
);
packet.SetBuffer(custom_buffer);

// 移动语义（高效）
MediaPacket packet2 = std::move(packet);
```

### 6.3 后端句柄（BackendHandle）

```cpp
#include "ai-camera/media/media_frame.hpp"

using namespace ai_camera::media;

// 创建 V4L2 后端句柄
auto v4l2_handle = std::make_shared<V4L2BackendHandle>(
    3,                      // V4L2 文件描述符
    0,                      // 缓冲区索引
    mmap_ptr,              // mmap 指针
    mmap_size              // mmap 大小
);

// 设置到视频帧
MediaFrame frame(1920, 1080, MEDIA_PIXEL_FORMAT_MJPEG);
frame.SetBackendHandle(v4l2_handle);

// 获取后端句柄
auto handle = frame.GetBackendHandle();
if (handle && strcmp(handle->BackendType(), "V4L2") == 0) {
    auto* v4l2 = dynamic_cast<V4L2BackendHandle*>(handle.get());
    int fd = v4l2->GetFd();
    // ... 使用 V4L2 特定功能 ...
}
```

---

## 7. C/C++ 互操作

### 7.1 从 C++ 到 C（ToCStruct）

```cpp
#include "ai-camera/media/media_frame.hpp"
#include "ai-camera/media/media_frame.h"

using namespace ai_camera::media;

// C++ 视频帧
MediaFrame cpp_frame(1920, 1080, MEDIA_PIXEL_FORMAT_I420);
cpp_frame.SetTimestamp(123456789);
cpp_frame.AllocBuffers();

// 转换为 C 结构体
media_frame_t c_frame;
cpp_frame.ToCStruct(&c_frame);

// 现在 c_frame 包含了 cpp_frame 的所有数据（浅拷贝）
// 可以传递给 C API 或 V4L2 驱动

// 使用后需要清理
media_frame_clear(&c_frame);
```

### 7.2 从 C 到 C++（FromCStruct）

```cpp
#include "ai-camera/media/media_frame.hpp"
#include "ai-camera/media/media_frame.h"

using namespace ai_camera::media;

// C 视频帧（可能来自 V4L2 驱动）
media_frame_t c_frame;
media_frame_init(&c_frame);
c_frame.width = 1920;
c_frame.height = 1080;
c_frame.pixel_format = MEDIA_PIXEL_FORMAT_I420;
media_frame_alloc_buffers(&c_frame);

// 转换为 C++ 对象
MediaFrame cpp_frame;
cpp_frame.FromCStruct(&c_frame);

// 现在 cpp_frame 包含了 c_frame 的所有数据（浅拷贝）
// 可以安全地使用 C++ API

// 清理 C 结构体（不会影响 C++ 对象，因为引用计数）
media_frame_clear(&c_frame);
```

### 7.3 缓冲区适配器（MediaBufferAdapter）

```cpp
#include "ai-camera/media/media_buffer_adapter.hpp"

using namespace ai_camera::media;

// 从 C 缓冲区创建 C++ 适配器
media_buffer_t* c_buffer = media_buffer_create(1024);
auto adapter = std::make_shared<MediaBufferAdapter>(c_buffer);

// 使用 C++ 接口
uint8_t* data = adapter->Data();
size_t size = adapter->Size();

// 提取底层 C 缓冲区
media_buffer_t* underlying = adapter->GetCBuffer();

// 从 IMediaBuffer 提取 MediaBufferAdapter
std::shared_ptr<IMediaBuffer> buffer = adapter;
MediaBufferAdapter* extracted = MediaBufferAdapter::FromIMediaBuffer(buffer);
```

---

## 8. NALSourceAdapter 使用指南

### 8.1 概述

`NALSourceAdapter` 继承了 `rtp::NALSource`，用于将 `MediaPacket` 转换为 `NALUnitList`，实现与现有 GB28181/RTSP 模块的无缝集成。

### 8.2 基本用法

```cpp
#include "ai-camera/media/nalsource_adapter.hpp"

using namespace ai_camera::media;

// 创建适配器
auto adapter = std::make_shared<NALSourceAdapter>();

// 配置适配器
adapter->SetMediaType(rtp::MediaType::H264);
adapter->SetCodecName("H264");
adapter->SetFrameRate(30);
adapter->SetWidth(1920);
adapter->SetHeight(1080);

// 打开适配器
bool ret = adapter->Open("adapter1");
if (!ret) {
    // 处理错误
}

// 推送媒体包（从 V4L2 采集线程调用）
auto packet = std::make_shared<MediaPacket>(MEDIA_TYPE_VIDEO_H264);
packet->SetTimestamp(get_timestamp());
packet->SetKeyFrame(true);
packet->AllocBuffer(4096);
// ... 填充 H264 数据 ...
adapter->PushPacket(packet);

// 读取 NAL 单元（从 RTP 发送线程调用）
rtp::NALSource::FrameNAL frame_nals = adapter->ReadNextFrame();
if (frame_nals) {
    for (const auto& nal_unit : *frame_nals) {
        // 处理 NAL 单元
        // nal_unit.data: NAL 数据
        // nal_unit.size: NAL 大小
        // nal_unit.type: NAL 类型
        // nal_unit.pts: 时间戳
    }
}

// 标记数据源结束
adapter->SetEndOfStream(true);

// 关闭适配器
adapter->Close();
```

### 8.3 线程安全

`NALSourceAdapter` 是线程安全的：

- **PushPacket()**：可以从任何线程调用（例如 V4L2 采集线程）
- **ReadNextFrame()**：可以从任何线程调用（例如 RTP 发送线程），会阻塞等待数据
- **TryReadNextFrame()**：非阻塞版本，立即返回

```cpp
// 生产者线程（V4L2 采集）
void capture_thread() {
    while (capturing) {
        // 采集数据
        auto packet = capture_packet();
        // 推送到队列
        adapter->PushPacket(packet);
    }
    adapter->SetEndOfStream(true);
}

// 消费者线程（RTP 发送）
void rtp_thread() {
    while (true) {
        auto frame_nals = adapter->ReadNextFrame();
        if (!frame_nals) {
            break;  // 数据源结束
        }
        // 发送 RTP 包
        send_rtp_packets(*frame_nals);
    }
}
```

### 8.4 队列管理

```cpp
// 设置队列最大大小（防止内存溢出）
adapter->SetMaxQueueSize(200);  // 最多缓存 200 个包

// 获取当前队列大小
size_t queue_size = adapter->GetQueueSize();

// 清空队列
adapter->ClearQueue();

// 检查是否还有数据
bool has_more = adapter->HasMoreData();
```

---

## 9. 编译与测试

### 9.1 编译项目

```bash
cd /path/to/ai-camera
mkdir build
cd build
cmake ..
make
```

### 9.2 运行测试

```bash
cd build
./test_media
```

预期输出：

```
=== media layer test ===
Testing C API buffer...
C API buffer test passed!
Testing C API frame...
C API frame test passed!
Testing C API packet...
C API packet test passed!
Testing C++ API frame...
C++ API frame test passed!
Testing C++ API packet...
C++ API packet test passed!
=== all tests passed! ===
```

### 9.3 集成到项目

在 `CMakeLists.txt` 中添加：

```cmake
# 添加 C 语言支持
enable_language(C)

# 添加头文件搜索路径
include_directories(include)

# 添加源文件
set(MEDIA_SOURCES
    src/media/media_buffer.c
    src/media/media_frame.c
    src/media/media_frame.cpp
    src/media/media_packet.c
    src/media/media_packet.cpp
    src/media/nalsource_adapter.cpp
)

# 添加可执行文件
add_executable(test_media tests/test_media.cpp ${MEDIA_SOURCES})
target_link_libraries(test_media ${LIBRARIES})
```

---

## 10. 常见问题解答

### 10.1 为什么使用不透明指针？

**答**：不透明指针（opaque pointer）隐藏了实现细节，使得：

1. **ABI 稳定性**：C API 的 ABI 不会改变，即使内部实现改变
2. **封装性**：调用者无法直接访问内部结构，只能通过提供的函数操作
3. **兼容性**：C API 可以被 C 和 C++ 代码调用

### 10.2 引用计数是如何工作的？

**答**：引用计数用于管理缓冲区的生命周期：

1. **创建时**：`ref_count = 1`
2. **引用时**：`ref_count++`（调用 `media_buffer_ref()`）
3. **释放时**：`ref_count--`（调用 `media_buffer_unref()`）
4. **销毁时**：当 `ref_count == 0` 时，释放内存

这种方式实现了**零拷贝**的浅拷贝，多个对象可以共享同一份数据。

### 10.3 C 和 C++ API 可以混用吗？

**答**：可以。`MediaBufferAdapter` 提供了 C 和 C++ 之间的桥梁：

- **C++ → C**：使用 `ToCStruct()` 方法
- **C → C++**：使用 `FromCStruct()` 方法
- **缓冲区共享**：`MediaBufferAdapter` 包装 `media_buffer_t`，实现无缝互操作

### 10.4 如何支持新的后端（如 FFmpeg）？

**答**：继承 `BackendHandle` 类：

```cpp
class FFmpegBackendHandle : public BackendHandle {
public:
    FFmpegBackendHandle(AVFrame* frame)
        : frame_(frame) {}
    
    const char* BackendType() const override {
        return "FFmpeg";
    }
    
    AVFrame* GetAVFrame() const { return frame_; }
    
private:
    AVFrame* frame_;
};

// 使用
auto ffmpeg_handle = std::make_shared<FFmpegBackendHandle>(av_frame);
frame.SetBackendHandle(ffmpeg_handle);
```

### 10.5 如何处理 V4L2 的零拷贝？

**答**：使用 `v4l2_mmap_ptr` 和 `v4l2_mmap_size` 字段：

```cpp
// V4L2 采集
void* mmap_ptr = mmap(...);
size_t mmap_size = ...;

// 创建缓冲区（使用外部数据，不拷贝）
media_buffer_t* buffer = media_buffer_create_with_data(
    mmap_ptr,
    mmap_size,
    v4l2_unmap_callback,  // 释放时调用 munmap
    v4l2_user_data
);

// 设置到视频帧
media_frame_t frame;
media_frame_init(&frame);
frame.buffers[0] = buffer;
frame.v4l2_fd = fd;
frame.v4l2_mmap_ptr = mmap_ptr;
frame.v4l2_mmap_size = mmap_size;
```

### 10.6 为什么 `media_types.h` 被删除了？

**答**：`media_types.h` 定义了一套完整的媒体类型系统，但与现有的 `media_frame.h` 和 `media_packet.h` 不一致，且未被实际使用。为了保持代码一致性，决定删除该文件。

如果需要更完整的类型系统，建议在未来的版本中统一规划。

---

## 附录 A：枚举值参考

### A.1 像素格式（media_pixel_format_t）

| 枚举值 | 说明 |
|--------|------|
| `MEDIA_PIXEL_FORMAT_UNKNOWN` | 未知格式 |
| `MEDIA_PIXEL_FORMAT_I420` | YUV 4:2:0 平面格式 |
| `MEDIA_PIXEL_FORMAT_YV12` | YVU 4:2:0 平面格式 |
| `MEDIA_PIXEL_FORMAT_NV12` | YUV 4:2:0 半平面格式 |
| `MEDIA_PIXEL_FORMAT_YUYV` | YUV 4:2:2 打包格式 |
| `MEDIA_PIXEL_FORMAT_RGB24` | RGB 24-bit 打包格式 |
| `MEDIA_PIXEL_FORMAT_H264` | H.264 编码帧 |

### A.2 媒体类型（media_type_t）

| 枚举值 | 说明 |
|--------|------|
| `MEDIA_TYPE_UNKNOWN` | 未知类型（-1） |
| `MEDIA_TYPE_VIDEO_H264` | H.264 视频（96） |
| `MEDIA_TYPE_VIDEO_H265` | H.265 视频（265） |
| `MEDIA_TYPE_AUDIO_AAC` | AAC 音频（37） |
| `MEDIA_TYPE_AUDIO_PCMU` | G.711 μ-law（0） |

---

## 附录 B：性能优化建议

### B.1 使用移动语义

```cpp
// 低效：拷贝构造（会调用 media_frame_copy）
MediaFrame frame2 = frame;

// 高效：移动构造（不拷贝数据）
MediaFrame frame2 = std::move(frame);
```

### B.2 避免不必要的深拷贝

```cpp
// 低效：深拷贝
media_frame_t frame2;
memcpy(&frame2, &frame1, sizeof(media_frame_t));
for (int i = 0; i < frame1.num_planes; i++) {
    frame2.buffers[i] = media_buffer_create(frame1.strides[i] * ...);
    memcpy(media_buffer_data(frame2.buffers[i]), 
           media_buffer_data(frame1.buffers[i]), 
           ...);
}

// 高效：浅拷贝（引用计数）
media_frame_copy(&frame1, &frame2);
```

### B.3 使用 V4L2 零拷贝

参考 [10.5 如何处理 V4L2 的零拷贝？](#105-如何处理-v4l2-的零拷贝)

---

## 附录 C：更新日志

### Version 1.0 (2026-06-25)

- ✅ 实现 C API 层（缓冲区、视频帧、媒体包）
- ✅ 实现 C++ API 层（类型安全的封装）
- ✅ 实现 `MediaBufferAdapter`（C/C++ 互操作）
- ✅ 实现 `NALSourceAdapter`（与 RTP 模块集成）
- ✅ 添加单元测试
- ✅ 添加设计文档

---

**文档结束**

如有问题，请联系项目维护者或提交 Issue。
