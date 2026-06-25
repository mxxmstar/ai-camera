/**
 * @file media_buffer.h
 * @brief C API 不透明缓冲区，用于跨 C/C++ 边界传递媒体数据
 * 
 * 使用不透明指针模式，实现细节隐藏在 .c 文件中
 * 支持引用计数和自定义释放回调，避免内存拷贝
 */

#ifndef AI_CAMERA_MEDIA_BUFFER_H
#define AI_CAMERA_MEDIA_BUFFER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 不透明缓冲区指针
 * 
 * 使用不透明指针模式，调用者无法直接访问内部结构
 * 只能通过提供的函数接口操作
 */
typedef struct media_buffer_t media_buffer_t;

/**
 * @brief 缓冲区释放回调函数类型
 * 
 * @param data 缓冲区数据指针
 * @param size 缓冲区大小
 * @param user_data 用户自定义数据
 */
typedef void (*media_buffer_free_func)(uint8_t* data, size_t size, void* user_data);

/**
 * @brief 创建缓冲区
 * 
 * @param size 缓冲区大小（字节）
 * @return media_buffer_t* 缓冲区指针，失败返回 NULL
 */
media_buffer_t* media_buffer_create(size_t size);

/**
 * @brief 创建缓冲区（使用外部数据，不拷贝）
 * 
 * @param data 外部数据指针
 * @param size 数据大小（字节）
 * @param free_func 释放回调函数（可选，为 NULL 时不释放）
 * @param user_data 传递给释放回调的用户数据
 * @return media_buffer_t* 缓冲区指针，失败返回 NULL
 * 
 * @note 使用此函数创建缓冲区时，不会拷贝数据，而是直接使用外部数据指针
 *       当缓冲区引用计数为 0 时，会调用 free_func 释放数据
 */
media_buffer_t* media_buffer_create_with_data(uint8_t* data, 
                                              size_t size,
                                              media_buffer_free_func free_func,
                                              void* user_data);

/**
 * @brief 增加引用计数
 * 
 * @param buffer 缓冲区指针
 * @return media_buffer_t* 缓冲区指针（与输入相同）
 */
media_buffer_t* media_buffer_ref(media_buffer_t* buffer);

/**
 * @brief 减少引用计数，计数为 0 时释放缓冲区
 * 
 * @param buffer 缓冲区指针
 */
void media_buffer_unref(media_buffer_t* buffer);

/**
 * @brief 获取缓冲区数据指针
 * 
 * @param buffer 缓冲区指针
 * @return uint8_t* 数据指针
 */
uint8_t* media_buffer_data(media_buffer_t* buffer);

/**
 * @brief 获取缓冲区大小
 * 
 * @param buffer 缓冲区指针
 * @return size_t 缓冲区大小（字节）
 */
size_t media_buffer_size(media_buffer_t* buffer);

/**
 * @brief 获取缓冲区引用计数
 * 
 * @param buffer 缓冲区指针
 * @return int 引用计数
 */
int media_buffer_ref_count(media_buffer_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // AI_CAMERA_MEDIA_BUFFER_H
