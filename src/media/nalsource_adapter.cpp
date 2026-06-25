/**
 * @file nalsource_adapter.cpp
 * @brief NALSource 适配器实现
 * 
 * 实现 NALSourceAdapter 类的所有方法
 * 将 MediaPacket 转换为 NALUnitList
 */

#include "ai-camera/media/nalsource_adapter.hpp"
#include "ai-camera/rtp/nalsource.h"

#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace ai_camera {
namespace media {

// ========== NALSourceAdapter 类实现 ==========

NALSourceAdapter::NALSourceAdapter()
    : media_type_(rtp::MediaType::NONE)
    , codec_name_("unknown")
    , frame_rate_(0)
    , width_(0)
    , height_(0)
    , duration_(0)
    , max_queue_size_(100)  // 默认队列最大大小
    , end_of_stream_(false)
    , is_open_(false) {
}

NALSourceAdapter::~NALSourceAdapter() {
    Close();
}

NALSourceAdapter::NALSourceAdapter(NALSourceAdapter&& other) {
    // 移动构造函数
    std::lock_guard<std::mutex> lock(other.queue_mutex_);
    
    packet_queue_ = std::move(other.packet_queue_);
    media_type_ = other.media_type_;
    codec_name_ = std::move(other.codec_name_);
    frame_rate_ = other.frame_rate_;
    width_ = other.width_;
    height_ = other.height_;
    duration_ = other.duration_;
    max_queue_size_ = other.max_queue_size_.load();
    end_of_stream_ = other.end_of_stream_.load();
    is_open_ = other.is_open_.load();
    
    // 清空源对象
    other.packet_queue_ = std::queue<std::shared_ptr<MediaPacket>>();
    other.media_type_ = rtp::MediaType::NONE;
    other.codec_name_ = "unknown";
    other.frame_rate_ = 0;
    other.width_ = 0;
    other.height_ = 0;
    other.duration_ = 0;
    other.max_queue_size_ = 100;
    other.end_of_stream_ = false;
    other.is_open_ = false;
}

NALSourceAdapter& NALSourceAdapter::operator=(NALSourceAdapter&& other) {
    if (this != &other) {
        std::lock_guard<std::mutex> lock1(queue_mutex_);
        std::lock_guard<std::mutex> lock2(other.queue_mutex_);
        
        // 清理当前对象
        Close();
        
        // 移动资源
        packet_queue_ = std::move(other.packet_queue_);
        media_type_ = other.media_type_;
        codec_name_ = std::move(other.codec_name_);
        frame_rate_ = other.frame_rate_;
        width_ = other.width_;
        height_ = other.height_;
        duration_ = other.duration_;
        max_queue_size_ = other.max_queue_size_.load();
        end_of_stream_ = other.end_of_stream_.load();
        is_open_ = other.is_open_.load();
        
        // 清空源对象
        other.packet_queue_ = std::queue<std::shared_ptr<MediaPacket>>();
        other.media_type_ = rtp::MediaType::NONE;
        other.codec_name_ = "unknown";
        other.frame_rate_ = 0;
        other.width_ = 0;
        other.height_ = 0;
        other.duration_ = 0;
        other.max_queue_size_ = 100;
        other.end_of_stream_ = false;
        other.is_open_ = false;
    }
    
    return *this;
}

bool NALSourceAdapter::Open(const std::string& source) {
    // 适配器不需要打开文件，只需要初始化状态
    (void)source;  // 避免未使用参数警告
    
    is_open_ = true;
    end_of_stream_ = false;
    ClearQueue();
    
    return true;
}

void NALSourceAdapter::Close() {
    ClearQueue();
    is_open_ = false;
    end_of_stream_ = false;
}

rtp::NALSource::FrameNAL NALSourceAdapter::ReadNextFrame() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // 等待队列中有数据或数据源结束
    queue_cv_.wait(lock, [this]() {
        return !packet_queue_.empty() || end_of_stream_.load();
    });
    
    // 检查是否到达数据源结尾
    if (packet_queue_.empty() && end_of_stream_.load()) {
        return std::nullopt;
    }
    
    // 取出一个 MediaPacket
    std::shared_ptr<MediaPacket> packet = packet_queue_.front();
    packet_queue_.pop();
    lock.unlock();
    
    // 解析 NAL 单元
    NALUnitList nal_units = ParseNALUnitsFromPacket(packet);
    
    return nal_units;
}

bool NALSourceAdapter::TryReadNextFrame(FrameNAL& frame_nals) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // 检查队列是否为空
    if (packet_queue_.empty()) {
        return false;
    }
    
    // 取出一个 MediaPacket
    std::shared_ptr<MediaPacket> packet = packet_queue_.front();
    packet_queue_.pop();
    
    // 解析 NAL 单元
    frame_nals = ParseNALUnitsFromPacket(packet);
    
    return true;
}

bool NALSourceAdapter::HasMoreData() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !packet_queue_.empty() || !end_of_stream_.load();
}

rtp::MediaType NALSourceAdapter::GetMediaType() const {
    return media_type_;
}

std::string NALSourceAdapter::GetCodecName() const {
    return codec_name_;
}

uint32_t NALSourceAdapter::GetFrameRate() const {
    return frame_rate_;
}

int NALSourceAdapter::GetWidth() const {
    return width_;
}

int NALSourceAdapter::GetHeight() const {
    return height_;
}

int64_t NALSourceAdapter::GetDuration() const {
    return duration_;
}

bool NALSourceAdapter::Seek(int64_t timestamp_ms) {
    // 适配器不支持跳转
    (void)timestamp_ms;
    return false;
}

void NALSourceAdapter::Reset() {
    ClearQueue();
}

bool NALSourceAdapter::PushPacket(std::shared_ptr<MediaPacket> packet) {
    if (!packet) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // 检查队列是否已满
    if (packet_queue_.size() >= max_queue_size_) {
        return false;
    }
    
    // 添加到队列
    packet_queue_.push(packet);
    
    // 通知等待的线程
    queue_cv_.notify_one();
    
    return true;
}

void NALSourceAdapter::SetMediaType(rtp::MediaType media_type) {
    media_type_ = media_type;
}

void NALSourceAdapter::SetCodecName(const std::string& codec_name) {
    codec_name_ = codec_name;
}

void NALSourceAdapter::SetFrameRate(uint32_t fps) {
    frame_rate_ = fps;
}

void NALSourceAdapter::SetWidth(int width) {
    width_ = width;
}

void NALSourceAdapter::SetHeight(int height) {
    height_ = height;
}

void NALSourceAdapter::SetDuration(int64_t duration) {
    duration_ = duration;
}

void NALSourceAdapter::ClearQueue() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    while (!packet_queue_.empty()) {
        packet_queue_.pop();
    }
    
    queue_cv_.notify_all();
}

size_t NALSourceAdapter::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return packet_queue_.size();
}

void NALSourceAdapter::SetMaxQueueSize(size_t max_size) {
    max_queue_size_ = max_size;
}

void NALSourceAdapter::SetEndOfStream(bool end_of_stream) {
    end_of_stream_ = end_of_stream;
    
    // 通知所有等待的线程
    queue_cv_.notify_all();
}

rtp::NALSource::NALUnitList NALSourceAdapter::ParseNALUnitsFromPacket(std::shared_ptr<MediaPacket> packet) {
    rtp::NALSource::NALUnitList nal_units;
    
    if (!packet || !packet->GetBuffer()) {
        return nal_units;
    }
    
    // 获取缓冲区数据
    std::shared_ptr<IMediaBuffer> buffer = packet->GetBuffer();
    const uint8_t* data = buffer->Data();
    size_t size = buffer->Size();
    
    if (!data || size == 0) {
        return nal_units;
    }
    
    // 解析 NAL 单元
    size_t start_pos = 0;
    size_t nal_start = 0;
    size_t nal_end = 0;
    
    while (start_pos < size) {
        // 查找 NAL 起始码
        size_t start_code_pos = FindStartCode(data, size, start_pos);
        
        if (start_code_pos == std::string::npos) {
            // 没有找到更多起始码
            break;
        }
        
        // 计算 NAL 单元起始位置（跳过起始码）
        nal_start = start_code_pos;
        if (size - start_code_pos >= 4 && 
            data[start_code_pos] == 0 && data[start_code_pos + 1] == 0 &&
            data[start_code_pos + 2] == 0 && data[start_code_pos + 3] == 1) {
            nal_start += 4;  // 4 字节起始码
        } else if (size - start_code_pos >= 3 &&
                   data[start_code_pos] == 0 && data[start_code_pos + 1] == 0 &&
                   data[start_code_pos + 2] == 1) {
            nal_start += 3;  // 3 字节起始码
        }
        
        // 查找下一个起始码
        start_pos = nal_start;
        size_t next_start_code = FindStartCode(data, size, start_pos);
        
        if (next_start_code != std::string::npos) {
            nal_end = next_start_code;
        } else {
            nal_end = size;  // 最后一个 NAL 单元
        }
        
        // 创建 NALUnit
        size_t nal_size = nal_end - nal_start;
        if (nal_size > 0) {
            rtp::NALUnit nal_unit(static_cast<uint32_t>(nal_size));
            
            // 复制 NAL 数据
            memcpy(nal_unit.data.get(), data + nal_start, nal_size);
            
            // 设置 NAL 单元类型
            if (nal_size > 0) {
                nal_unit.type = data[nal_start] & 0x1F;  // H.264 NAL unit type
            }
            
            // 设置时间戳
            nal_unit.pts = packet->Timestamp();
            nal_unit.dts = packet->Timestamp();
            
            // 设置是否关键帧
            nal_unit.is_keyframe = packet->IsKeyFrame();
            
            nal_units.push_back(std::move(nal_unit));
        }
        
        // 移动到下一个位置
        start_pos = nal_end;
    }
    
    return nal_units;
}

size_t NALSourceAdapter::FindStartCode(const uint8_t* data, size_t size, size_t start_pos) {
    if (!data || start_pos >= size) {
        return std::string::npos;
    }
    
    for (size_t i = start_pos; i < size; i++) {
        // 检查 4 字节起始码 (0x00000001)
        if (i + 3 < size &&
            data[i] == 0 && data[i + 1] == 0 &&
            data[i + 2] == 0 && data[i + 3] == 1) {
            return i;
        }
        
        // 检查 3 字节起始码 (0x000001)
        if (i + 2 < size &&
            data[i] == 0 && data[i + 1] == 0 &&
            data[i + 2] == 1) {
            return i;
        }
    }
    
    return std::string::npos;
}

rtp::MediaType NALSourceAdapter::ToRtpMediaType(media_type_t media_type) {
    switch (media_type) {
        case MEDIA_TYPE_VIDEO_H264:
            return rtp::MediaType::H264;
        case MEDIA_TYPE_VIDEO_H265:
            return rtp::MediaType::H265;
        default:
            return rtp::MediaType::NONE;
    }
}

media_type_t NALSourceAdapter::FromRtpMediaType(rtp::MediaType media_type) {
    switch (media_type) {
        case rtp::MediaType::H264:
            return MEDIA_TYPE_VIDEO_H264;
        case rtp::MediaType::H265:
            return MEDIA_TYPE_VIDEO_H265;
        default:
            return MEDIA_TYPE_UNKNOWN;
    }
}

} // namespace media
} // namespace ai_camera
