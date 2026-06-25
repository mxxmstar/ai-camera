/**
 * @file media_frame.c
 * @brief C API 视频帧操作实现
 * 
 * 实现 media_frame_t 的初始化、清理、缓冲区分配等操作
 */

#include "ai-camera/media/media_frame.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void media_frame_init(media_frame_t* frame) {
    if (!frame) {
        return;
    }
    
    memset(frame, 0, sizeof(media_frame_t));
    
    // 初始化 V4L2 后端特定数据
    frame->v4l2_fd = -1;
    frame->v4l2_buffer_index = -1;
    frame->v4l2_mmap_ptr = NULL;
    frame->v4l2_mmap_size = 0;
}

void media_frame_clear(media_frame_t* frame) {
    if (!frame) {
        return;
    }
    
    // 减少所有缓冲区的引用计数
    for (int i = 0; i < 4; i++) {
        if (frame->buffers[i]) {
            media_buffer_unref(frame->buffers[i]);
            frame->buffers[i] = NULL;
        }
    }
    
    // 清空 V4L2 后端特定数据
    frame->v4l2_fd = -1;
    frame->v4l2_buffer_index = -1;
    frame->v4l2_mmap_ptr = NULL;
    frame->v4l2_mmap_size = 0;
}

int media_frame_calc_plane_sizes(media_pixel_format_t pixel_format,
                                 int32_t width,
                                 int32_t height,
                                 size_t plane_sizes[4],
                                 int32_t plane_strides[4]) {
    if (!plane_sizes || !plane_strides || width <= 0 || height <= 0) {
        return -1;
    }
    
    // 初始化输出数组
    for (int i = 0; i < 4; i++) {
        plane_sizes[i] = 0;
        plane_strides[i] = 0;
    }
    
    int num_planes = 0;
    
    switch (pixel_format) {
        case MEDIA_PIXEL_FORMAT_I420:
        case MEDIA_PIXEL_FORMAT_YV12:
            // YUV 4:2:0 平面格式
            // Y 平面：width * height
            // U/V 平面：width/2 * height/2
            plane_strides[0] = width;
            plane_sizes[0] = width * height;
            
            plane_strides[1] = width / 2;
            plane_sizes[1] = (width / 2) * (height / 2);
            
            plane_strides[2] = width / 2;
            plane_sizes[2] = (width / 2) * (height / 2);
            
            num_planes = 3;
            break;
            
        case MEDIA_PIXEL_FORMAT_NV12:
        case MEDIA_PIXEL_FORMAT_NV21:
            // YUV 4:2:0 半平面格式
            // Y 平面：width * height
            // UV 平面：width * height/2
            plane_strides[0] = width;
            plane_sizes[0] = width * height;
            
            plane_strides[1] = width;
            plane_sizes[1] = width * (height / 2);
            
            num_planes = 2;
            break;
            
        case MEDIA_PIXEL_FORMAT_YUYV:
        case MEDIA_PIXEL_FORMAT_UYVY:
            // YUV 4:2:2 打包格式
            // 每个像素 2 字节
            plane_strides[0] = width * 2;
            plane_sizes[0] = width * height * 2;
            
            num_planes = 1;
            break;
            
        case MEDIA_PIXEL_FORMAT_RGB24:
        case MEDIA_PIXEL_FORMAT_BGR24:
            // RGB/BGR 24-bit 打包格式
            // 每个像素 3 字节
            plane_strides[0] = width * 3;
            plane_sizes[0] = width * height * 3;
            
            num_planes = 1;
            break;
            
        case MEDIA_PIXEL_FORMAT_RGBA:
        case MEDIA_PIXEL_FORMAT_BGRA:
            // RGBA/BGRA 32-bit 打包格式
            // 每个像素 4 字节
            plane_strides[0] = width * 4;
            plane_sizes[0] = width * height * 4;
            
            num_planes = 1;
            break;
            
        case MEDIA_PIXEL_FORMAT_MJPEG:
        case MEDIA_PIXEL_FORMAT_H264:
        case MEDIA_PIXEL_FORMAT_H265:
            // 压缩格式，单个缓冲区
            // 步长和偏移量不适用
            num_planes = 1;
            plane_strides[0] = 0;
            plane_sizes[0] = 0; // 需要外部设置
            break;
            
        default:
            return -1;
    }
    
    return num_planes;
}

int media_frame_alloc_buffers(media_frame_t* frame) {
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        return -1;
    }
    
    // 计算平面大小
    size_t plane_sizes[4];
    int32_t plane_strides[4];
    
    int num_planes = media_frame_calc_plane_sizes(
        frame->pixel_format,
        frame->width,
        frame->height,
        plane_sizes,
        plane_strides
    );
    
    if (num_planes <= 0) {
        return -1;
    }
    
    // 分配每个平面的缓冲区
    for (int i = 0; i < num_planes; i++) {
        frame->buffers[i] = media_buffer_create(plane_sizes[i]);
        if (!frame->buffers[i]) {
            // 分配失败，清理已分配的缓冲区
            for (int j = 0; j < i; j++) {
                media_buffer_unref(frame->buffers[j]);
                frame->buffers[j] = NULL;
            }
            return -1;
        }
        
        frame->strides[i] = plane_strides[i];
        frame->offsets[i] = 0; // 偏移量从 0 开始
    }
    
    frame->num_planes = num_planes;
    
    return 0;
}

int media_frame_copy(media_frame_t* src, media_frame_t* dst) {
    if (!src || !dst) {
        return -1;
    }
    
    // 复制元数据
    dst->width = src->width;
    dst->height = src->height;
    dst->pixel_format = src->pixel_format;
    dst->timestamp = src->timestamp;
    dst->duration = src->duration;
    dst->num_planes = src->num_planes;
    
    // 复制步长和偏移量
    for (int i = 0; i < 4; i++) {
        dst->strides[i] = src->strides[i];
        dst->offsets[i] = src->offsets[i];
    }
    
    // 浅拷贝缓冲区（增加引用计数）
    for (int i = 0; i < src->num_planes; i++) {
        dst->buffers[i] = media_buffer_ref(src->buffers[i]);
    }
    
    // 复制 V4L2 后端特定数据
    dst->v4l2_fd = src->v4l2_fd;
    dst->v4l2_buffer_index = src->v4l2_buffer_index;
    dst->v4l2_mmap_ptr = src->v4l2_mmap_ptr;
    dst->v4l2_mmap_size = src->v4l2_mmap_size;
    
    return 0;
}

const char* media_pixel_format_name(media_pixel_format_t pixel_format) {
    switch (pixel_format) {
        case MEDIA_PIXEL_FORMAT_UNKNOWN:
            return "UNKNOWN";
        case MEDIA_PIXEL_FORMAT_I420:
            return "I420";
        case MEDIA_PIXEL_FORMAT_YV12:
            return "YV12";
        case MEDIA_PIXEL_FORMAT_I422:
            return "I422";
        case MEDIA_PIXEL_FORMAT_I444:
            return "I444";
        case MEDIA_PIXEL_FORMAT_NV12:
            return "NV12";
        case MEDIA_PIXEL_FORMAT_NV21:
            return "NV21";
        case MEDIA_PIXEL_FORMAT_YUYV:
            return "YUYV";
        case MEDIA_PIXEL_FORMAT_UYVY:
            return "UYVY";
        case MEDIA_PIXEL_FORMAT_RGB24:
            return "RGB24";
        case MEDIA_PIXEL_FORMAT_BGR24:
            return "BGR24";
        case MEDIA_PIXEL_FORMAT_RGBA:
            return "RGBA";
        case MEDIA_PIXEL_FORMAT_BGRA:
            return "BGRA";
        case MEDIA_PIXEL_FORMAT_MJPEG:
            return "MJPEG";
        case MEDIA_PIXEL_FORMAT_H264:
            return "H264";
        case MEDIA_PIXEL_FORMAT_H265:
            return "H265";
        default:
            return "INVALID";
    }
}
