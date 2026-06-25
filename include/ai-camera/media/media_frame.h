/**
 * @file media_frame.h
 * @brief C API 视频帧结构体定义
 * 
 * media_frame_t 是可见结构体，包含视频帧的所有元数据
 * 使用 media_buffer_t 存储实际像素数据
 */

#ifndef AI_CAMERA_MEDIA_FRAME_H
#define AI_CAMERA_MEDIA_FRAME_H

#include <stdint.h>
#include <stddef.h>

#include "media_buffer.h"  // 同一目录下的头文件可以直接引用

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 像素格式枚举
 */
typedef enum media_pixel_format_t {
    MEDIA_PIXEL_FORMAT_UNKNOWN = 0,  ///< 未知格式
    
    // YUV 平面格式
    MEDIA_PIXEL_FORMAT_I420 = 1,     ///< YUV 4:2:0 平面格式 (YYYYUV)
    MEDIA_PIXEL_FORMAT_YV12 = 2,     ///< YVU 4:2:0 平面格式 (YYYYVU)
    MEDIA_PIXEL_FORMAT_I422 = 3,      ///< YUV 4:2:2 平面格式
    MEDIA_PIXEL_FORMAT_I444 = 4,      ///< YUV 4:4:4 平面格式
    
    // YUV 打包格式
    MEDIA_PIXEL_FORMAT_NV12 = 5,     ///< YUV 4:2:0 半平面格式 (YYYYUVUV)
    MEDIA_PIXEL_FORMAT_NV21 = 6,     ///< YVU 4:2:0 半平面格式 (YYYYVUVU)
    MEDIA_PIXEL_FORMAT_YUYV = 7,     ///< YUV 4:2:2 打包格式 (YUYVYUYV)
    MEDIA_PIXEL_FORMAT_UYVY = 8,     ///< YUV 4:2:2 打包格式 (UYVYUYVY)
    
    // RGB 格式
    MEDIA_PIXEL_FORMAT_RGB24 = 20,    ///< RGB 24-bit 打包格式
    MEDIA_PIXEL_FORMAT_BGR24 = 21,    ///< BGR 24-bit 打包格式
    MEDIA_PIXEL_FORMAT_RGBA = 22,     ///< RGBA 32-bit 打包格式
    MEDIA_PIXEL_FORMAT_BGRA = 23,     ///< BGRA 32-bit 打包格式
    
    // 压缩格式
    MEDIA_PIXEL_FORMAT_MJPEG = 30,   ///< Motion JPEG
    MEDIA_PIXEL_FORMAT_H264 = 31,     ///< H.264 编码帧
    MEDIA_PIXEL_FORMAT_H265 = 32,     ///< H.265 编码帧
    
} media_pixel_format_t;

/**
 * @brief 视频帧结构体
 * 
 * 包含视频帧的所有元数据，使用 media_buffer_t 存储像素数据
 * 支持平面格式（多个缓冲区）和打包格式（单个缓冲区）
 */
typedef struct media_frame_t {
    int32_t width;                    ///< 宽度（像素）
    int32_t height;                   ///< 高度（像素）
    media_pixel_format_t pixel_format; ///< 像素格式
    
    int64_t timestamp;                ///< 时间戳（微秒）
    int64_t duration;                 ///< 帧持续时间（微秒，可选）
    
    // 像素数据缓冲区（最多支持 4 个平面）
    media_buffer_t* buffers[4];       ///< 像素数据缓冲区数组
    int32_t strides[4];               ///< 每个平面的步长（字节）
    int32_t offsets[4];               ///< 每个平面的偏移量（字节）
    int32_t num_planes;               ///< 实际平面数量
    
    // V4L2 后端特定数据（可选）
    int v4l2_fd;                      ///< V4L2 设备文件描述符
    int v4l2_buffer_index;            ///< V4L2 缓冲区索引
    void* v4l2_mmap_ptr;             ///< V4L2 mmap 缓冲区指针（零拷贝）
    size_t v4l2_mmap_size;            ///< V4L2 mmap 缓冲区大小
    
} media_frame_t;

/**
 * @brief 初始化视频帧结构体
 * 
 * @param frame 视频帧指针
 */
void media_frame_init(media_frame_t* frame);

/**
 * @brief 清理视频帧结构体（减少缓冲区引用计数）
 * 
 * @param frame 视频帧指针
 */
void media_frame_clear(media_frame_t* frame);

/**
 * @brief 计算像素格式的每个平面大小
 * 
 * @param pixel_format 像素格式
 * @param width 宽度
 * @param height 高度
 * @param plane_sizes 输出：每个平面大小数组（至少 4 个元素）
 * @param plane_strides 输出：每个平面步长数组（至少 4 个元素）
 * @return int 实际平面数量，失败返回 -1
 */
int media_frame_calc_plane_sizes(media_pixel_format_t pixel_format,
                                 int32_t width,
                                 int32_t height,
                                 size_t plane_sizes[4],
                                 int32_t plane_strides[4]);

/**
 * @brief 分配视频帧的像素缓冲区
 * 
 * @param frame 视频帧指针
 * @return int 0 成功，-1 失败
 * 
 * @note 根据 width、height、pixel_format 自动计算并分配缓冲区
 */
int media_frame_alloc_buffers(media_frame_t* frame);

/**
 * @brief 复制视频帧（浅拷贝，增加缓冲区引用计数）
 * 
 * @param src 源视频帧
 * @param dst 目标视频帧
 * @return int 0 成功，-1 失败
 */
int media_frame_copy(media_frame_t* src, media_frame_t* dst);

/**
 * @brief 获取像素格式名称
 * 
 * @param pixel_format 像素格式
 * @return const char* 格式名称字符串
 */
const char* media_pixel_format_name(media_pixel_format_t pixel_format);

#ifdef __cplusplus
}
#endif

#endif // AI_CAMERA_MEDIA_FRAME_H
