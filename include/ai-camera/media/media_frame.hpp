/**
 * @file media_frame.hpp
 * @brief C++ 视频帧类封装
 * 
 * 提供类型安全的视频帧操作接口
 * 支持与 C API 的 media_frame_t 互转
 * 支持 BackendHandle 扩展后端特定功能
 */

#ifndef AI_CAMERA_MEDIA_FRAME_HPP
#define AI_CAMERA_MEDIA_FRAME_HPP

#include <memory>
#include <cstdint>
#include <cstring>
#include <vector>

#include "i_media_buffer.hpp"
#include "media_frame.h"

namespace ai_camera {
namespace media {

/**
 * @brief 后端句柄基类
 * 
 * 用于扩展支持不同后端的特定功能
 * 例如：V4L2 后端、FFmpeg 后端等
 */
class BackendHandle {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~BackendHandle() = default;

    /**
     * @brief 获取后端类型名称
     * 
     * @return const char* 类型名称
     */
    virtual const char* BackendType() const = 0;
};

/**
 * @brief V4L2 后端句柄
 * 
 * 存储 V4L2 设备相关信息，支持零拷贝采集
 */
class V4L2BackendHandle : public BackendHandle {
public:
    /**
     * @brief 构造函数
     * 
     * @param fd V4L2 设备文件描述符
     * @param buffer_index 缓冲区索引
     * @param mmap_ptr mmap 缓冲区指针
     * @param mmap_size mmap 缓冲区大小
     */
    V4L2BackendHandle(int fd, 
                      int buffer_index, 
                      void* mmap_ptr, 
                      size_t mmap_size)
        : fd_(fd)
        , buffer_index_(buffer_index)
        , mmap_ptr_(mmap_ptr)
        , mmap_size_(mmap_size) {}

    /**
     * @brief 析构函数
     */
    virtual ~V4L2BackendHandle() = default;

    /**
     * @brief 获取后端类型名称
     */
    const char* BackendType() const override {
        return "V4L2";
    }

    /**
     * @brief 获取 V4L2 设备文件描述符
     */
    int GetFd() const { return fd_; }

    /**
     * @brief 获取缓冲区索引
     */
    int GetBufferIndex() const { return buffer_index_; }

    /**
     * @brief 获取 mmap 缓冲区指针
     */
    void* GetMmapPtr() const { return mmap_ptr_; }

    /**
     * @brief 获取 mmap 缓冲区大小
     */
    size_t GetMmapSize() const { return mmap_size_; }

private:
    int fd_;                      ///< V4L2 设备文件描述符
    int buffer_index_;            ///< 缓冲区索引
    void* mmap_ptr_;             ///< mmap 缓冲区指针
    size_t mmap_size_;           ///< mmap 缓冲区大小
};

/**
 * @brief 视频帧类
 * 
 * 封装 media_frame_t，提供类型安全的 C++ 接口
 * 支持多种像素格式和平面布局
 */
class MediaFrame {
public:
    /**
     * @brief 默认构造函数
     */
    MediaFrame();

    /**
     * @brief 析构函数
     */
    ~MediaFrame();

    /**
     * @brief 构造函数（指定宽高和像素格式）
     * 
     * @param width 宽度（像素）
     * @param height 高度（像素）
     * @param pixel_format 像素格式
     */
    MediaFrame(int32_t width, int32_t height, media_pixel_format_t pixel_format);

    /**
     * @brief 禁止拷贝构造
     */
    MediaFrame(const MediaFrame&) = delete;

    /**
     * @brief 禁止拷贝赋值
     */
    MediaFrame& operator=(const MediaFrame&) = delete;

    /**
     * @brief 允许移动构造
     */
    MediaFrame(MediaFrame&& other);

    /**
     * @brief 允许移动赋值
     */
    MediaFrame& operator=(MediaFrame&& other);

    /**
     * @brief 获取宽度
     */
    int32_t Width() const { return frame_.width; }

    /**
     * @brief 设置宽度
     */
    void SetWidth(int32_t width) { frame_.width = width; }

    /**
     * @brief 获取高度
     */
    int32_t Height() const { return frame_.height; }

    /**
     * @brief 设置高度
     */
    void SetHeight(int32_t height) { frame_.height = height; }

    /**
     * @brief 获取像素格式
     */
    media_pixel_format_t PixelFormat() const { return frame_.pixel_format; }

    /**
     * @brief 设置像素格式
     */
    void SetPixelFormat(media_pixel_format_t pixel_format) { 
        frame_.pixel_format = pixel_format; 
    }

    /**
     * @brief 获取时间戳
     */
    int64_t Timestamp() const { return frame_.timestamp; }

    /**
     * @brief 设置时间戳
     */
    void SetTimestamp(int64_t timestamp) { frame_.timestamp = timestamp; }

    /**
     * @brief 获取帧持续时间
     */
    int64_t Duration() const { return frame_.duration; }

    /**
     * @brief 设置帧持续时间
     */
    void SetDuration(int64_t duration) { frame_.duration = duration; }

    /**
     * @brief 获取平面数量
     */
    int32_t NumPlanes() const { return frame_.num_planes; }

    /**
     * @brief 获取指定平面的缓冲区
     * 
     * @param plane_index 平面索引
     * @return std::shared_ptr<IMediaBuffer> 缓冲区指针
     */
    std::shared_ptr<IMediaBuffer> GetPlaneBuffer(int32_t plane_index) const;

    /**
     * @brief 设置指定平面的缓冲区
     * 
     * @param plane_index 平面索引
     * @param buffer 缓冲区指针
     */
    void SetPlaneBuffer(int32_t plane_index, std::shared_ptr<IMediaBuffer> buffer);

    /**
     * @brief 获取指定平面的步长
     * 
     * @param plane_index 平面索引
     * @return int32_t 步长（字节）
     */
    int32_t GetPlaneStride(int32_t plane_index) const;

    /**
     * @brief 设置指定平面的步长
     * 
     * @param plane_index 平面索引
     * @param stride 步长（字节）
     */
    void SetPlaneStride(int32_t plane_index, int32_t stride);

    /**
     * @brief 获取指定平面的偏移量
     * 
     * @param plane_index 平面索引
     * @return int32_t 偏移量（字节）
     */
    int32_t GetPlaneOffset(int32_t plane_index) const;

    /**
     * @brief 设置指定平面的偏移量
     * 
     * @param plane_index 平面索引
     * @param offset 偏移量（字节）
     */
    void SetPlaneOffset(int32_t plane_index, int32_t offset);

    /**
     * @brief 分配像素缓冲区
     * 
     * @return true 成功，false 失败
     */
    bool AllocBuffers();

    /**
     * @brief 设置后端句柄
     * 
     * @param handle 后端句柄指针
     */
    void SetBackendHandle(std::shared_ptr<BackendHandle> handle);

    /**
     * @brief 获取后端句柄
     * 
     * @return std::shared_ptr<BackendHandle> 后端句柄指针
     */
    std::shared_ptr<BackendHandle> GetBackendHandle() const;

    /**
     * @brief 转换为 C API 结构体（浅拷贝）
     * 
     * @param c_frame C API 结构体指针
     */
    void ToCStruct(media_frame_t* c_frame) const;

    /**
     * @brief 从 C API 结构体加载（浅拷贝）
     * 
     * @param c_frame C API 结构体指针
     */
    void FromCStruct(const media_frame_t* c_frame);

private:
    media_frame_t frame_;                              ///< C API 结构体
    std::vector<std::shared_ptr<IMediaBuffer>> plane_buffers_; ///< 平面缓冲区数组
    std::shared_ptr<BackendHandle> backend_handle_;     ///< 后端句柄
};

} // namespace media
} // namespace ai_camera

#endif // AI_CAMERA_MEDIA_FRAME_HPP
