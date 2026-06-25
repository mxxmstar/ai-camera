/**
 * @file media_packet.c
 * @brief C API 媒体包操作实现
 * 
 * 实现 media_packet_t 的初始化、清理、缓冲区管理等操作
 */

#include "ai-camera/media/media_packet.h"

#include <stdlib.h>
#include <string.h>

void media_packet_init(media_packet_t* packet) {
    if (!packet) {
        return;
    }
    
    memset(packet, 0, sizeof(media_packet_t));
    
    // 初始化默认值
    packet->media_type = MEDIA_TYPE_UNKNOWN;
    packet->timestamp = 0;
    packet->duration = 0;
    packet->is_key_frame = 0;
    packet->has_aud = 0;
    packet->buffer = NULL;
    packet->width = 0;
    packet->height = 0;
    packet->fps = 0;
    packet->sample_rate = 0;
    packet->channels = 0;
    
    // 初始化 V4L2 后端特定数据
    packet->v4l2_fd = -1;
    packet->v4l2_buffer_index = -1;
    packet->v4l2_mmap_ptr = NULL;
    packet->v4l2_mmap_size = 0;
}

void media_packet_clear(media_packet_t* packet) {
    if (!packet) {
        return;
    }
    
    // 减少缓冲区的引用计数
    if (packet->buffer) {
        media_buffer_unref(packet->buffer);
        packet->buffer = NULL;
    }
    
    // 清空 V4L2 后端特定数据
    packet->v4l2_fd = -1;
    packet->v4l2_buffer_index = -1;
    packet->v4l2_mmap_ptr = NULL;
    packet->v4l2_mmap_size = 0;
}

int media_packet_alloc_buffer(media_packet_t* packet, size_t size) {
    if (!packet || size == 0) {
        return -1;
    }
    
    // 如果已存在缓冲区，先释放
    if (packet->buffer) {
        media_buffer_unref(packet->buffer);
        packet->buffer = NULL;
    }
    
    // 分配新缓冲区
    packet->buffer = media_buffer_create(size);
    if (!packet->buffer) {
        return -1;
    }
    
    return 0;
}

int media_packet_set_data(media_packet_t* packet,
                          uint8_t* data,
                          size_t size,
                          media_buffer_free_func free_func,
                          void* user_data) {
    if (!packet || !data || size == 0) {
        return -1;
    }
    
    // 如果已存在缓冲区，先释放
    if (packet->buffer) {
        media_buffer_unref(packet->buffer);
        packet->buffer = NULL;
    }
    
    // 创建使用外部数据的缓冲区
    packet->buffer = media_buffer_create_with_data(
        data, size, free_func, user_data
    );
    
    if (!packet->buffer) {
        return -1;
    }
    
    return 0;
}

int media_packet_copy(media_packet_t* src, media_packet_t* dst) {
    if (!src || !dst) {
        return -1;
    }
    
    // 复制元数据
    dst->media_type = src->media_type;
    dst->timestamp = src->timestamp;
    dst->duration = src->duration;
    dst->is_key_frame = src->is_key_frame;
    dst->has_aud = src->has_aud;
    dst->width = src->width;
    dst->height = src->height;
    dst->fps = src->fps;
    dst->sample_rate = src->sample_rate;
    dst->channels = src->channels;
    
    // 浅拷贝缓冲区（增加引用计数）
    if (src->buffer) {
        dst->buffer = media_buffer_ref(src->buffer);
    } else {
        dst->buffer = NULL;
    }
    
    // 复制 V4L2 后端特定数据
    dst->v4l2_fd = src->v4l2_fd;
    dst->v4l2_buffer_index = src->v4l2_buffer_index;
    dst->v4l2_mmap_ptr = src->v4l2_mmap_ptr;
    dst->v4l2_mmap_size = src->v4l2_mmap_size;
    
    return 0;
}

const char* media_type_name(media_type_t media_type) {
    switch (media_type) {
        case MEDIA_TYPE_UNKNOWN:
            return "UNKNOWN";
        case MEDIA_TYPE_VIDEO_H264:
            return "H264";
        case MEDIA_TYPE_VIDEO_H265:
            return "H265";
        case MEDIA_TYPE_VIDEO_MJPEG:
            return "MJPEG";
        case MEDIA_TYPE_AUDIO_AAC:
            return "AAC";
        case MEDIA_TYPE_AUDIO_PCMA:
            return "PCMA";
        case MEDIA_TYPE_AUDIO_PCMU:
            return "PCMU";
        case MEDIA_TYPE_AUDIO_OPUS:
            return "OPUS";
        default:
            return "INVALID";
    }
}

int media_type_is_video(media_type_t media_type) {
    switch (media_type) {
        case MEDIA_TYPE_VIDEO_H264:
        case MEDIA_TYPE_VIDEO_H265:
        case MEDIA_TYPE_VIDEO_MJPEG:
            return 1;
        default:
            return 0;
    }
}

int media_type_is_audio(media_type_t media_type) {
    switch (media_type) {
        case MEDIA_TYPE_AUDIO_AAC:
        case MEDIA_TYPE_AUDIO_PCMA:
        case MEDIA_TYPE_AUDIO_PCMU:
        case MEDIA_TYPE_AUDIO_OPUS:
            return 1;
        default:
            return 0;
    }
}
