/**
 * @file nalsource_adapter.hpp
 * @brief NALSource 适配器
 * 
 * 将 MediaPacket 转换为 NALUnitList
 * 与现有 GB28181/RTSP 模块无缝集成
 */

#ifndef AI_CAMERA_NALSOURCE_ADAPTER_HPP
#define AI_CAMERA_NALSOURCE_ADAPTER_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#include "media_packet.hpp"
#include "rtp/nalsource.h"

namespace ai_camera {
namespace media {

/**
 * @brief NALSource 适配器类
 * 
 * 继承 rtp::NALSource，将 MediaPacket 转换为 NALUnitList
 * 支持线程安全的 MediaPacket 队列操作
 */
class NALSourceAdapter : public rtp::NALSource {
public:
    /**
     * @brief 构造函数
     */
    NALSourceAdapter();

    /**
     * @brief 析构函数
     */
    virtual ~NALSourceAdapter();

    /**
     * @brief 禁止拷贝构造
     */
    NALSourceAdapter(const NALSourceAdapter&) = delete;

    /**
     * @brief 禁止拷贝赋值
     */
    NALSourceAdapter& operator=(const NALSourceAdapter&) = delete;

    /**
     * @brief 允许移动构造
     */
    NALSourceAdapter(NALSourceAdapter&& other);

    /**
     * @brief 允许移动赋值
     */
    NALSourceAdapter& operator=(NALSourceAdapter&& other);

    // ========== NALSource 接口实现 ==========

    /**
     * @brief 打开媒体源
     * 
     * @param source 媒体源标识符（此处为适配器名称）
     * @return 是否成功
     */
    bool Open(const std::string& source) override;

    /**
     * @brief 关闭媒体源
     */
    void Close() override;

    /**
     * @brief 读取下一帧的所有 NAL 单元
     * 
     * @return FrameNAL，如果到达文件末尾或出错则返回 std::nullopt
     */
    FrameNAL ReadNextFrame() override;

    /**
     * @brief 尝试读取下一帧（非阻塞版本）
     * 
     * @param frame_nals 输出参数，读取到的 NAL 单元列表
     * @return 是否成功读取
     */
    bool TryReadNextFrame(FrameNAL& frame_nals) override;

    /**
     * @brief 是否还有更多数据
     * 
     * @return true 有更多数据，false 没有更多数据
     */
    bool HasMoreData() const override;

    /**
     * @brief 获取媒体类型
     * 
     * @return MediaType 媒体类型
     */
    rtp::MediaType GetMediaType() const override;

    /**
     * @brief 获取编码格式名称
     * 
     * @return std::string 编码格式名称
     */
    std::string GetCodecName() const override;

    /**
     * @brief 获取帧率
     * 
     * @return uint32_t 帧率
     */
    uint32_t GetFrameRate() const override;

    /**
     * @brief 获取视频宽度
     * 
     * @return int 视频宽度
     */
    int GetWidth() const override;

    /**
     * @brief 获取视频高度
     * 
     * @return int 视频高度
     */
    int GetHeight() const override;

    /**
     * @brief 获取时长（毫秒）
     * 
     * @return int64_t 时长
     */
    int64_t GetDuration() const override;

    /**
     * @brief 跳转到指定时间（毫秒）
     * 
     * @param timestamp_ms 时间戳（毫秒）
     * @return 是否成功
     */
    bool Seek(int64_t timestamp_ms) override;

    /**
     * @brief 重置到文件开头
     */
    void Reset() override;

    // ========== 适配器特有接口 ==========

    /**
     * @brief 添加 MediaPacket 到队列
     * 
     * @param packet MediaPacket 指针
     * @return true 成功，false 失败
     */
    bool PushPacket(std::shared_ptr<MediaPacket> packet);

    /**
     * @brief 设置媒体类型
     * 
     * @param media_type 媒体类型
     */
    void SetMediaType(rtp::MediaType media_type);

    /**
     * @brief 设置编码格式名称
     * 
     * @param codec_name 编码格式名称
     */
    void SetCodecName(const std::string& codec_name);

    /**
     * @brief 设置帧率
     * 
     * @param fps 帧率
     */
    void SetFrameRate(uint32_t fps);

    /**
     * @brief 设置视频宽度
     * 
     * @param width 视频宽度
     */
    void SetWidth(int width);

    /**
     * @brief 设置视频高度
     * 
     * @param height 视频高度
     */
    void SetHeight(int height);

    /**
     * @brief 设置时长（毫秒）
     * 
     * @param duration 时长
     */
    void SetDuration(int64_t duration);

    /**
     * @brief 清空队列
     */
    void ClearQueue();

    /**
     * @brief 获取队列大小
     * 
     * @return size_t 队列大小
     */
    size_t GetQueueSize() const;

    /**
     * @brief 设置队列最大大小
     * 
     * @param max_size 最大大小
     */
    void SetMaxQueueSize(size_t max_size);

    /**
     * @brief 标记数据源结束
     * 
     * @param end_of_stream true 表示数据源结束
     */
    void SetEndOfStream(bool end_of_stream);

private:
    /**
     * @brief 从 MediaPacket 解析 NAL 单元
     * 
     * @param packet MediaPacket 指针
     * @return NALUnitList NAL 单元列表
     */
    NALUnitList ParseNALUnitsFromPacket(std::shared_ptr<MediaPacket> packet);

    /**
     * @brief 查找 NAL 起始码
     * 
     * @param data 数据指针
     * @param size 数据大小
     * @param start_pos 起始位置
     * @return size_t 起始码位置，未找到返回 std::string::npos
     */
    size_t FindStartCode(const uint8_t* data, size_t size, size_t start_pos);

    /**
     * @brief 将 MediaType 转换为 rtp::MediaType
     * 
     * @param media_type MediaType
     * @return rtp::MediaType rtp::MediaType
     */
    static rtp::MediaType ToRtpMediaType(media_type_t media_type);

    /**
     * @brief 将 rtp::MediaType 转换为 MediaType
     * 
     * @param media_type rtp::MediaType
     * @return media_type_t MediaType
     */
    static media_type_t FromRtpMediaType(rtp::MediaType media_type);

private:
    std::queue<std::shared_ptr<MediaPacket>> packet_queue_;  ///< MediaPacket 队列
    mutable std::mutex queue_mutex_;                         ///< 队列互斥锁
    std::condition_variable queue_cv_;                       ///< 队列条件变量
    
    rtp::MediaType media_type_;                              ///< 媒体类型
    std::string codec_name_;                                  ///< 编码格式名称
    uint32_t frame_rate_;                                    ///< 帧率
    int width_;                                              ///< 视频宽度
    int height_;                                             ///< 视频高度
    int64_t duration_;                                       ///< 时长（毫秒）
    
    std::atomic<size_t> max_queue_size_;                    ///< 队列最大大小（原子变量）
    std::atomic<bool> end_of_stream_;                       ///< 数据源是否结束
    std::atomic<bool> is_open_;                              ///< 是否已打开
};

} // namespace media
} // namespace ai_camera

#endif // AI_CAMERA_NALSOURCE_ADAPTER_HPP
