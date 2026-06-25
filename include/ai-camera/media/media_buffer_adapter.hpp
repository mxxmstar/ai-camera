/**
 * @file media_buffer_adapter.hpp
 * @brief C++ 缓冲区适配器
 * 
 * 将 C API 的 media_buffer_t 包装为 C++ API 的 IMediaBuffer 接口
 * 供 MediaFrame 和 MediaPacket 共同使用
 */

#ifndef AI_CAMERA_MEDIA_BUFFER_ADAPTER_HPP
#define AI_CAMERA_MEDIA_BUFFER_ADAPTER_HPP

#include "i_media_buffer.hpp"
#include "media_buffer.h"

namespace ai_camera {
namespace media {

/**
 * @brief C API 缓冲区适配器
 * 
 * 将 media_buffer_t 包装为 IMediaBuffer 接口
 * 实现 C/C++ 缓冲区的无缝互操作
 */
class MediaBufferAdapter : public IMediaBuffer {
public:
    /**
     * @brief 构造函数
     * 
     * @param buffer C API 缓冲区指针
     */
    explicit MediaBufferAdapter(media_buffer_t* buffer)
        : buffer_(buffer) {
        if (buffer_) {
            media_buffer_ref(buffer_);
        }
    }
    
    /**
     * @brief 析构函数
     */
    virtual ~MediaBufferAdapter() {
        if (buffer_) {
            media_buffer_unref(buffer_);
        }
    }
    
    /**
     * @brief 获取缓冲区数据指针（非 const 版本）
     * 
     * @return uint8_t* 数据指针
     */
    uint8_t* Data() override {
        return buffer_ ? media_buffer_data(buffer_) : nullptr;
    }
    
    /**
     * @brief 获取缓冲区数据指针（const 版本）
     * 
     * @return const uint8_t* 数据指针
     */
    const uint8_t* Data() const override {
        return buffer_ ? media_buffer_data(buffer_) : nullptr;
    }
    
    /**
     * @brief 获取缓冲区大小
     * 
     * @return size_t 缓冲区大小（字节）
     */
    size_t Size() const override {
        return buffer_ ? media_buffer_size(buffer_) : 0;
    }
    
    /**
     * @brief 获取缓冲区引用计数
     * 
     * @return int 引用计数
     */
    int RefCount() const override {
        return buffer_ ? media_buffer_ref_count(buffer_) : 0;
    }
    
    /**
     * @brief 增加引用计数
     */
    void Ref() override {
        if (buffer_) {
            media_buffer_ref(buffer_);
        }
    }
    
    /**
     * @brief 减少引用计数
     */
    void Unref() override {
        if (buffer_) {
            media_buffer_unref(buffer_);
        }
    }
    
    /**
     * @brief 获取底层 C API 缓冲区指针
     * 
     * @return media_buffer_t* C API 缓冲区指针
     */
    media_buffer_t* GetCBuffer() const {
        return buffer_;
    }
    
    /**
     * @brief 从 IMediaBuffer 指针提取 MediaBufferAdapter
     * 
     * @param buffer IMediaBuffer 指针
     * @return MediaBufferAdapter* 适配器指针，失败返回 nullptr
     */
    static MediaBufferAdapter* FromIMediaBuffer(std::shared_ptr<IMediaBuffer> buffer) {
        return dynamic_cast<MediaBufferAdapter*>(buffer.get());
    }
    
private:
    media_buffer_t* buffer_;  ///< C API 缓冲区指针
};

} // namespace media
} // namespace ai_camera

#endif // AI_CAMERA_MEDIA_BUFFER_ADAPTER_HPP
