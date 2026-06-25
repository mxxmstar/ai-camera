/**
 * @file media_buffer.c
 * @brief C API 缓冲区实现
 * 
 * 实现 media_buffer_t 的引用计数管理和内存操作
 */

#include "ai-camera/media/media_buffer.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief 缓冲区结构体（不透明指针实现）
 */
struct media_buffer_t {
    uint8_t* data;                   ///< 数据指针
    size_t size;                     ///< 数据大小（字节）
    int ref_count;                   ///< 引用计数
    
    media_buffer_free_func free_func; ///< 释放回调函数
    void* user_data;                 ///< 用户自定义数据
};

media_buffer_t* media_buffer_create(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    media_buffer_t* buffer = (media_buffer_t*)malloc(sizeof(media_buffer_t));
    if (!buffer) {
        return NULL;
    }
    
    buffer->data = (uint8_t*)malloc(size);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }
    
    memset(buffer->data, 0, size);
    buffer->size = size;
    buffer->ref_count = 1;
    buffer->free_func = NULL;
    buffer->user_data = NULL;
    
    return buffer;
}

media_buffer_t* media_buffer_create_with_data(uint8_t* data, 
                                              size_t size,
                                              media_buffer_free_func free_func,
                                              void* user_data) {
    if (!data || size == 0) {
        return NULL;
    }
    
    media_buffer_t* buffer = (media_buffer_t*)malloc(sizeof(media_buffer_t));
    if (!buffer) {
        return NULL;
    }
    
    buffer->data = data;
    buffer->size = size;
    buffer->ref_count = 1;
    buffer->free_func = free_func;
    buffer->user_data = user_data;
    
    return buffer;
}

media_buffer_t* media_buffer_ref(media_buffer_t* buffer) {
    if (!buffer) {
        return NULL;
    }
    
    buffer->ref_count++;
    return buffer;
}

void media_buffer_unref(media_buffer_t* buffer) {
    if (!buffer) {
        return;
    }
    
    buffer->ref_count--;
    
    if (buffer->ref_count <= 0) {
        // 调用释放回调函数（如果有）
        if (buffer->free_func) {
            buffer->free_func(buffer->data, buffer->size, buffer->user_data);
        } else {
            // 默认释放数据
            free(buffer->data);
        }
        
        // 释放缓冲区结构体
        free(buffer);
    }
}

uint8_t* media_buffer_data(media_buffer_t* buffer) {
    if (!buffer) {
        return NULL;
    }
    
    return buffer->data;
}

size_t media_buffer_size(media_buffer_t* buffer) {
    if (!buffer) {
        return 0;
    }
    
    return buffer->size;
}

int media_buffer_ref_count(media_buffer_t* buffer) {
    if (!buffer) {
        return 0;
    }
    
    return buffer->ref_count;
}
