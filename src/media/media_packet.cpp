/**
 * @file media_packet.cpp
 * @brief C++ 媒体包类实现
 * 
 * 实现 MediaPacket 类的所有方法
 */

#include "ai-camera/media/media_packet.hpp"
#include "ai-camera/media/media_buffer_adapter.hpp"

#include <algorithm>
#include <stdexcept>

namespace ai_camera {
namespace media {

// ========== MediaPacket 类实现 ==========

MediaPacket::MediaPacket() {
    media_packet_init(&packet_);
}

MediaPacket::~MediaPacket() {
    media_packet_clear(&packet_);
}

MediaPacket::MediaPacket(media_type_t media_type) {
    media_packet_init(&packet_);
    packet_.media_type = media_type;
}

MediaPacket::MediaPacket(MediaPacket&& other) {
    // 移动构造函数
    packet_ = other.packet_;
    buffer_ = std::move(other.buffer_);
    backend_handle_ = std::move(other.backend_handle_);
    
    // 清空源对象
    media_packet_init(&other.packet_);
}

MediaPacket& MediaPacket::operator=(MediaPacket&& other) {
    if (this != &other) {
        // 清理当前对象
        media_packet_clear(&packet_);
        
        // 移动资源
        packet_ = other.packet_;
        buffer_ = std::move(other.buffer_);
        backend_handle_ = std::move(other.backend_handle_);
        
        // 清空源对象
        media_packet_init(&other.packet_);
    }
    
    return *this;
}

std::shared_ptr<IMediaBuffer> MediaPacket::GetBuffer() const {
    return buffer_;
}

void MediaPacket::SetBuffer(std::shared_ptr<IMediaBuffer> buffer) {
    buffer_ = buffer;
    
    // 将 IMediaBuffer 转换回 media_buffer_t
    // 如果 buffer 是 MediaBufferAdapter 的实例，可以提取出 media_buffer_t
    if (buffer) {
        MediaBufferAdapter* adapter = dynamic_cast<MediaBufferAdapter*>(buffer.get());
        if (adapter) {
            packet_.buffer = adapter->GetCBuffer();
        } else {
            // 如果不是 MediaBufferAdapter，暂时设置为 nullptr
            // 实际使用时，应该通过 FromCStruct() 来同步
            packet_.buffer = nullptr;
        }
    } else {
        packet_.buffer = nullptr;
    }
}

bool MediaPacket::AllocBuffer(size_t size) {
    // 分配 C 缓冲区
    if (media_packet_alloc_buffer(&packet_, size) != 0) {
        return false;
    }
    
    // 创建 C++ 缓冲区适配器
    if (packet_.buffer) {
        buffer_ = std::make_shared<MediaBufferAdapter>(packet_.buffer);
        return true;
    }
    
    return false;
}

void MediaPacket::SetBackendHandle(std::shared_ptr<BackendHandle> handle) {
    backend_handle_ = handle;
}

std::shared_ptr<BackendHandle> MediaPacket::GetBackendHandle() const {
    return backend_handle_;
}

void MediaPacket::ToCStruct(media_packet_t* c_packet) const {
    if (!c_packet) {
        return;
    }
    
    // 复制元数据
    c_packet->media_type = packet_.media_type;
    c_packet->timestamp = packet_.timestamp;
    c_packet->duration = packet_.duration;
    c_packet->is_key_frame = packet_.is_key_frame;
    c_packet->has_aud = packet_.has_aud;
    c_packet->width = packet_.width;
    c_packet->height = packet_.height;
    c_packet->fps = packet_.fps;
    c_packet->sample_rate = packet_.sample_rate;
    c_packet->channels = packet_.channels;
    
    // 复制缓冲区指针（浅拷贝，增加引用计数）
    if (packet_.buffer) {
        c_packet->buffer = media_buffer_ref(packet_.buffer);
    } else {
        c_packet->buffer = nullptr;
    }
    
    // 复制 V4L2 后端特定数据
    c_packet->v4l2_fd = packet_.v4l2_fd;
    c_packet->v4l2_buffer_index = packet_.v4l2_buffer_index;
    c_packet->v4l2_mmap_ptr = packet_.v4l2_mmap_ptr;
    c_packet->v4l2_mmap_size = packet_.v4l2_mmap_size;
}

void MediaPacket::FromCStruct(const media_packet_t* c_packet) {
    if (!c_packet) {
        return;
    }
    
    // 清理当前数据
    media_packet_clear(&packet_);
    
    // 复制元数据
    packet_.media_type = c_packet->media_type;
    packet_.timestamp = c_packet->timestamp;
    packet_.duration = c_packet->duration;
    packet_.is_key_frame = c_packet->is_key_frame;
    packet_.has_aud = c_packet->has_aud;
    packet_.width = c_packet->width;
    packet_.height = c_packet->height;
    packet_.fps = c_packet->fps;
    packet_.sample_rate = c_packet->sample_rate;
    packet_.channels = c_packet->channels;
    
    // 复制缓冲区指针（浅拷贝，增加引用计数）
    if (c_packet->buffer) {
        packet_.buffer = media_buffer_ref(c_packet->buffer);
        
        // 创建 C++ 缓冲区适配器
        buffer_ = std::make_shared<MediaBufferAdapter>(packet_.buffer);
    } else {
        buffer_ = nullptr;
    }
    
    // 复制 V4L2 后端特定数据
    packet_.v4l2_fd = c_packet->v4l2_fd;
    packet_.v4l2_buffer_index = c_packet->v4l2_buffer_index;
    packet_.v4l2_mmap_ptr = c_packet->v4l2_mmap_ptr;
    packet_.v4l2_mmap_size = c_packet->v4l2_mmap_size;
}

} // namespace media
} // namespace ai_camera
