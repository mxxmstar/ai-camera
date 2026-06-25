/**
 * @file i_media_buffer.hpp
 * @brief C++ 缓冲区抽象接口
 * 
 * 定义 IMediaBuffer 接口，提供类型安全的缓冲区访问
 * 所有媒体缓冲区应实现此接口
 */

#ifndef AI_CAMERA_I_MEDIA_BUFFER_HPP
#define AI_CAMERA_I_MEDIA_BUFFER_HPP

#include <cstdint>
#include <cstddef>

namespace ai_camera {
namespace media {

/**
 * @brief 缓冲区抽象接口
 * 
 * 提供统一的缓冲区访问接口，支持不同类型的缓冲区实现
 */
class IMediaBuffer {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~IMediaBuffer() = default;

    /**
     * @brief 获取缓冲区数据指针
     * 
     * @return uint8_t* 数据指针
     */
    virtual uint8_t* Data() = 0;

    /**
     * @brief 获取缓冲区数据指针（const 版本）
     * 
     * @return const uint8_t* 数据指针
     */
    virtual const uint8_t* Data() const = 0;

    /**
     * @brief 获取缓冲区大小
     * 
     * @return size_t 缓冲区大小（字节）
     */
    virtual size_t Size() const = 0;

    /**
     * @brief 获取缓冲区引用计数
     * 
     * @return int 引用计数
     */
    virtual int RefCount() const = 0;

    /**
     * @brief 增加引用计数
     */
    virtual void Ref() = 0;

    /**
     * @brief 减少引用计数
     */
    virtual void Unref() = 0;
};

} // namespace media
} // namespace ai_camera

#endif // AI_CAMERA_I_MEDIA_BUFFER_HPP
