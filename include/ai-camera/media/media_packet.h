/**
 * @file media_packet.h
 * @brief C API 媒体包结构体定义
 * 
 * media_packet_t 是可见结构体，包含编码后的媒体数据包
 * 用于传输 H264/H265 码流、AAC 音频等编码数据
 */

#ifndef AI_CAMERA_MEDIA_PACKET_H
#define AI_CAMERA_MEDIA_PACKET_H

#include <stdint.h>
#include <stddef.h>

#include "media_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 媒体类型枚举
 * 
 * 与 rtp::MediaType 保持一致
 */
typedef enum media_type_t {
    MEDIA_TYPE_UNKNOWN = -1,         ///< 未知类型（使用 -1 避免与 PCMU 冲突）
    
    // 视频编码格式
    MEDIA_TYPE_VIDEO_H264 = 96,     ///< H.264 视频（与 RTP payload type 一致）
    MEDIA_TYPE_VIDEO_H265 = 265,    ///< H.265 视频
    MEDIA_TYPE_VIDEO_MJPEG = 26,    ///< Motion JPEG
    
    // 音频编码格式
    MEDIA_TYPE_AUDIO_AAC = 37,      ///< AAC 音频
    MEDIA_TYPE_AUDIO_PCMA = 8,      ///< G.711 A-law
    MEDIA_TYPE_AUDIO_PCMU = 0,      ///< G.711 μ-law（RTP payload type 0）
    MEDIA_TYPE_AUDIO_OPUS = 111,    ///< Opus 音频
    
} media_type_t;

/**
 * @brief 媒体包结构体
 * 
 * 包含编码后的媒体数据，用于传输码流
 * 支持视频帧、音频帧等编码数据
 */
typedef struct media_packet_t {
    media_type_t media_type;          ///< 媒体类型
    
    int64_t timestamp;                ///< 时间戳（微秒）
    int64_t duration;                 ///< 包持续时间（微秒，可选）
    
    int32_t is_key_frame;            ///< 是否关键帧（视频）/ 是否静音（音频）
    int32_t has_aud;                  ///< 是否包含 AUD（Access Unit Delimiter）
    
    // 编码数据缓冲区
    media_buffer_t* buffer;           ///< 编码数据缓冲区
    
    // 视频特定字段
    int32_t width;                    ///< 视频宽度（可选，用于 SPS/PPS 解析）
    int32_t height;                   ///< 视频高度（可选）
    int32_t fps;                      ///< 帧率（可选）
    
    // 音频特定字段
    int32_t sample_rate;              ///< 音频采样率（可选）
    int32_t channels;                 ///< 音频通道数（可选）
    
    // V4L2 后端特定数据（可选）
    int v4l2_fd;                      ///< V4L2 设备文件描述符
    int v4l2_buffer_index;            ///< V4L2 缓冲区索引
    void* v4l2_mmap_ptr;             ///< V4L2 mmap 缓冲区指针（零拷贝）
    size_t v4l2_mmap_size;            ///< V4L2 mmap 缓冲区大小
    
} media_packet_t;

/**
 * @brief 初始化媒体包结构体
 * 
 * @param packet 媒体包指针
 */
void media_packet_init(media_packet_t* packet);

/**
 * @brief 清理媒体包结构体（减少缓冲区引用计数）
 * 
 * @param packet 媒体包指针
 */
void media_packet_clear(media_packet_t* packet);

/**
 * @brief 分配媒体包的缓冲区
 * 
 * @param packet 媒体包指针
 * @param size 缓冲区大小（字节）
 * @return int 0 成功，-1 失败
 */
int media_packet_alloc_buffer(media_packet_t* packet, size_t size);

/**
 * @brief 设置媒体包的外部数据（不拷贝）
 * 
 * @param packet 媒体包指针
 * @param data 外部数据指针
 * @param size 数据大小（字节）
 * @param free_func 释放回调函数（可选）
 * @param user_data 传递给释放回调的用户数据
 * @return int 0 成功，-1 失败
 */
int media_packet_set_data(media_packet_t* packet,
                          uint8_t* data,
                          size_t size,
                          media_buffer_free_func free_func,
                          void* user_data);

/**
 * @brief 复制媒体包（浅拷贝，增加缓冲区引用计数）
 * 
 * @param src 源媒体包
 * @param dst 目标媒体包
 * @return int 0 成功，-1 失败
 */
int media_packet_copy(media_packet_t* src, media_packet_t* dst);

/**
 * @brief 获取媒体类型名称
 * 
 * @param media_type 媒体类型
 * @return const char* 类型名称字符串
 */
const char* media_type_name(media_type_t media_type);

/**
 * @brief 判断媒体类型是否为视频
 * 
 * @param media_type 媒体类型
 * @return int 1 是视频，0 不是
 */
int media_type_is_video(media_type_t media_type);

/**
 * @brief 判断媒体类型是否为音频
 * 
 * @param media_type 媒体类型
 * @return int 1 是音频，0 不是
 */
int media_type_is_audio(media_type_t media_type);

#ifdef __cplusplus
}
#endif

#endif // AI_CAMERA_MEDIA_PACKET_H
