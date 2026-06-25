/**
 * @file media_packet.hpp
 * @brief C++ 媒体包类封装
 * 
 * 提供类型安全的媒体包操作接口
 * 用于传输 H264/H265 码流、AAC 音频等编码数据
 * 支持与 C API 的 media_packet_t 互转
 */

#ifndef AI_CAMERA_MEDIA_PACKET_HPP
#define AI_CAMERA_MEDIA_PACKET_HPP

#include <memory>
#include <cstdint>
#include <cstring>
#include <vector>

#include "i_media_buffer.hpp"
#include "media_frame.hpp"  // 包含 BackendHandle 定义
#include "media_packet.h"

namespace ai_camera {
namespace media {

/**
 * @brief 媒体包类
 * 
 * 封装 media_packet_t，提供类型安全的 C++ 接口
 * 用于传输编码后的媒体数据
 */
class MediaPacket {
public:
    /**
     * @brief 默认构造函数
     */
    MediaPacket();

    /**
     * @brief 析构函数
     */
    ~MediaPacket();

    /**
     * @brief 构造函数（指定媒体类型）
     * 
     * @param media_type 媒体类型
     */
    explicit MediaPacket(media_type_t media_type);

    /**
     * @brief 禁止拷贝构造
     */
    MediaPacket(const MediaPacket&) = delete;

    /**
     * @brief 禁止拷贝赋值
     */
    MediaPacket& operator=(const MediaPacket&) = delete;

    /**
     * @brief 允许移动构造
     */
    MediaPacket(MediaPacket&& other);

    /**
     * @brief 允许移动赋值
     */
    MediaPacket& operator=(MediaPacket&& other);

    /**
     * @brief 获取媒体类型
     */
    media_type_t MediaType() const { return packet_.media_type; }

    /**
     * @brief 设置媒体类型
     */
    void SetMediaType(media_type_t media_type) { 
        packet_.media_type = media_type; 
    }

    /**
     * @brief 获取时间戳
     */
    int64_t Timestamp() const { return packet_.timestamp; }

    /**
     * @brief 设置时间戳
     */
    void SetTimestamp(int64_t timestamp) { packet_.timestamp = timestamp; }

    /**
     * @brief 获取包持续时间
     */
    int64_t Duration() const { return packet_.duration; }

    /**
     * @brief 设置包持续时间
     */
    void SetDuration(int64_t duration) { packet_.duration = duration; }

    /**
     * @brief 判断是否关键帧
     */
    bool IsKeyFrame() const { return packet_.is_key_frame != 0; }

    /**
     * @brief 设置是否关键帧
     */
    void SetKeyFrame(bool is_key_frame) { 
        packet_.is_key_frame = is_key_frame ? 1 : 0; 
    }

    /**
     * @brief 判断是否包含 AUD
     */
    bool HasAUD() const { return packet_.has_aud != 0; }

    /**
     * @brief 设置是否包含 AUD
     */
    void SetHasAUD(bool has_aud) { 
        packet_.has_aud = has_aud ? 1 : 0; 
    }

    /**
     * @brief 获取编码数据缓冲区
     * 
     * @return std::shared_ptr<IMediaBuffer> 缓冲区指针
     */
    std::shared_ptr<IMediaBuffer> GetBuffer() const;

    /**
     * @brief 设置编码数据缓冲区
     * 
     * @param buffer 缓冲区指针
     */
    void SetBuffer(std::shared_ptr<IMediaBuffer> buffer);

    /**
     * @brief 获取视频宽度（可选）
     */
    int32_t Width() const { return packet_.width; }

    /**
     * @brief 设置视频宽度
     */
    void SetWidth(int32_t width) { packet_.width = width; }

    /**
     * @brief 获取视频高度（可选）
     */
    int32_t Height() const { return packet_.height; }

    /**
     * @brief 设置视频高度
     */
    void SetHeight(int32_t height) { packet_.height = height; }

    /**
     * @brief 获取视频帧率（可选）
     */
    int32_t Fps() const { return packet_.fps; }

    /**
     * @brief 设置视频帧率
     */
    void SetFps(int32_t fps) { packet_.fps = fps; }

    /**
     * @brief 获取音频采样率（可选）
     */
    int32_t SampleRate() const { return packet_.sample_rate; }

    /**
     * @brief 设置音频采样率
     */
    void SetSampleRate(int32_t sample_rate) { 
        packet_.sample_rate = sample_rate; 
    }

    /**
     * @brief 获取音频通道数（可选）
     */
    int32_t Channels() const { return packet_.channels; }

    /**
     * @brief 设置音频通道数
     */
    void SetChannels(int32_t channels) { packet_.channels = channels; }

    /**
     * @brief 分配编码数据缓冲区
     * 
     * @param size 缓冲区大小（字节）
     * @return true 成功，false 失败
     */
    bool AllocBuffer(size_t size);

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
     * @param c_packet C API 结构体指针
     */
    void ToCStruct(media_packet_t* c_packet) const;

    /**
     * @brief 从 C API 结构体加载（浅拷贝）
     * 
     * @param c_packet C API 结构体指针
     */
    void FromCStruct(const media_packet_t* c_packet);

private:
    media_packet_t packet_;                      ///< C API 结构体
    std::shared_ptr<IMediaBuffer> buffer_;       ///< 编码数据缓冲区
    std::shared_ptr<BackendHandle> backend_handle_; ///< 后端句柄
};

} // namespace media
} // namespace ai_camera

#endif // AI_CAMERA_MEDIA_PACKET_HPP
