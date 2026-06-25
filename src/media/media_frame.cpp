/**
 * @file media_frame.cpp
 * @brief C++ 视频帧类实现
 * 
 * 实现 MediaFrame 类的所有方法
 */

#include "ai-camera/media/media_frame.hpp"
#include "ai-camera/media/media_buffer_adapter.hpp"

#include <algorithm>
#include <stdexcept>

namespace ai_camera {
namespace media {

// ========== MediaFrame 类实现 ==========

MediaFrame::MediaFrame() {
    media_frame_init(&frame_);
}

MediaFrame::~MediaFrame() {
    media_frame_clear(&frame_);
}

MediaFrame::MediaFrame(int32_t width, int32_t height, media_pixel_format_t pixel_format) {
    media_frame_init(&frame_);
    frame_.width = width;
    frame_.height = height;
    frame_.pixel_format = pixel_format;
}

MediaFrame::MediaFrame(MediaFrame&& other) {
    // 移动构造函数
    frame_ = other.frame_;
    plane_buffers_ = std::move(other.plane_buffers_);
    backend_handle_ = std::move(other.backend_handle_);
    
    // 清空源对象
    media_frame_init(&other.frame_);
}

MediaFrame& MediaFrame::operator=(MediaFrame&& other) {
    if (this != &other) {
        // 清理当前对象
        media_frame_clear(&frame_);
        
        // 移动资源
        frame_ = other.frame_;
        plane_buffers_ = std::move(other.plane_buffers_);
        backend_handle_ = std::move(other.backend_handle_);
        
        // 清空源对象
        media_frame_init(&other.frame_);
    }
    
    return *this;
}

std::shared_ptr<IMediaBuffer> MediaFrame::GetPlaneBuffer(int32_t plane_index) const {
    if (plane_index < 0 || plane_index >= static_cast<int32_t>(plane_buffers_.size())) {
        return nullptr;
    }
    
    return plane_buffers_[plane_index];
}

void MediaFrame::SetPlaneBuffer(int32_t plane_index, std::shared_ptr<IMediaBuffer> buffer) {
    if (plane_index < 0 || plane_index >= 4) {
        return;
    }
    
    // 确保 plane_buffers_ 足够大
    if (static_cast<int32_t>(plane_buffers_.size()) <= plane_index) {
        plane_buffers_.resize(plane_index + 1);
    }
    
    plane_buffers_[plane_index] = buffer;
    
    // 同时更新 C 结构体
    if (buffer) {
        // 将 IMediaBuffer 转换回 media_buffer_t
        MediaBufferAdapter* adapter = dynamic_cast<MediaBufferAdapter*>(buffer.get());
        if (adapter) {
            frame_.buffers[plane_index] = adapter->GetCBuffer();
        } else {
            // 如果不是 MediaBufferAdapter，暂时设置为 nullptr
            frame_.buffers[plane_index] = nullptr;
        }
    } else {
        frame_.buffers[plane_index] = nullptr;
    }
}

int32_t MediaFrame::GetPlaneStride(int32_t plane_index) const {
    if (plane_index < 0 || plane_index >= 4) {
        return 0;
    }
    
    return frame_.strides[plane_index];
}

void MediaFrame::SetPlaneStride(int32_t plane_index, int32_t stride) {
    if (plane_index < 0 || plane_index >= 4) {
        return;
    }
    
    frame_.strides[plane_index] = stride;
}

int32_t MediaFrame::GetPlaneOffset(int32_t plane_index) const {
    if (plane_index < 0 || plane_index >= 4) {
        return 0;
    }
    
    return frame_.offsets[plane_index];
}

void MediaFrame::SetPlaneOffset(int32_t plane_index, int32_t offset) {
    if (plane_index < 0 || plane_index >= 4) {
        return;
    }
    
    frame_.offsets[plane_index] = offset;
}

bool MediaFrame::AllocBuffers() {
    return media_frame_alloc_buffers(&frame_) == 0;
}

void MediaFrame::SetBackendHandle(std::shared_ptr<BackendHandle> handle) {
    backend_handle_ = handle;
}

std::shared_ptr<BackendHandle> MediaFrame::GetBackendHandle() const {
    return backend_handle_;
}

void MediaFrame::ToCStruct(media_frame_t* c_frame) const {
    if (!c_frame) {
        return;
    }
    
    // 复制元数据
    c_frame->width = frame_.width;
    c_frame->height = frame_.height;
    c_frame->pixel_format = frame_.pixel_format;
    c_frame->timestamp = frame_.timestamp;
    c_frame->duration = frame_.duration;
    c_frame->num_planes = frame_.num_planes;
    
    // 复制步长和偏移量
    for (int i = 0; i < 4; i++) {
        c_frame->strides[i] = frame_.strides[i];
        c_frame->offsets[i] = frame_.offsets[i];
    }
    
    // 复制缓冲区指针（浅拷贝，增加引用计数）
    for (int i = 0; i < frame_.num_planes; i++) {
        if (frame_.buffers[i]) {
            c_frame->buffers[i] = media_buffer_ref(frame_.buffers[i]);
        } else {
            c_frame->buffers[i] = nullptr;
        }
    }
    
    // 复制 V4L2 后端特定数据
    c_frame->v4l2_fd = frame_.v4l2_fd;
    c_frame->v4l2_buffer_index = frame_.v4l2_buffer_index;
    c_frame->v4l2_mmap_ptr = frame_.v4l2_mmap_ptr;
    c_frame->v4l2_mmap_size = frame_.v4l2_mmap_size;
}

void MediaFrame::FromCStruct(const media_frame_t* c_frame) {
    if (!c_frame) {
        return;
    }
    
    // 清理当前数据
    media_frame_clear(&frame_);
    
    // 复制元数据
    frame_.width = c_frame->width;
    frame_.height = c_frame->height;
    frame_.pixel_format = c_frame->pixel_format;
    frame_.timestamp = c_frame->timestamp;
    frame_.duration = c_frame->duration;
    frame_.num_planes = c_frame->num_planes;
    
    // 复制步长和偏移量
    for (int i = 0; i < 4; i++) {
        frame_.strides[i] = c_frame->strides[i];
        frame_.offsets[i] = c_frame->offsets[i];
    }
    
    // 复制缓冲区指针（浅拷贝，增加引用计数）
    for (int i = 0; i < c_frame->num_planes; i++) {
        if (c_frame->buffers[i]) {
            frame_.buffers[i] = media_buffer_ref(c_frame->buffers[i]);
            
            // 创建适配器并添加到 plane_buffers_
            plane_buffers_.push_back(
                std::make_shared<MediaBufferAdapter>(frame_.buffers[i])
            );
        }
    }
    
    // 复制 V4L2 后端特定数据
    frame_.v4l2_fd = c_frame->v4l2_fd;
    frame_.v4l2_buffer_index = c_frame->v4l2_buffer_index;
    frame_.v4l2_mmap_ptr = c_frame->v4l2_mmap_ptr;
    frame_.v4l2_mmap_size = c_frame->v4l2_mmap_size;
}

} // namespace media
} // namespace ai_camera
