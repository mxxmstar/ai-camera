#pragma once

#include <memory>
#include <string>
#include "gb28181_types.h"

// ASIO UDP socket 和 timer
#include <asio.hpp>

namespace gb28181 {

// ============================================================================
// Gb28181MediaSender：RTP/PS over UDP 媒体发送器
// ============================================================================
class Gb28181MediaSender {
public:
    Gb28181MediaSender();
    ~Gb28181MediaSender();

    // 启动媒体推流
    bool Start(const InviteMediaInfo& media_info,
               const std::string& video_file,
               uint32_t frame_rate);

    // 停止媒体推流
    void Stop();

    // 是否在推流中
    bool IsStreaming() const { return streaming_; }

private:
    // 定时器回调：读取帧 → PS 编码 → RTP 分片 → UDP 发送
    void OnTimer(const asio::error_code& ec);

    // 发送 RTP 包
    void SendRtpPacket(const std::vector<uint8_t>& ps_data);

    // ASIO IO 上下文和线程
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<std::thread>      thread_;

    // UDP socket 和 endpoint
    std::unique_ptr<asio::ip::udp::socket> socket_;
    asio::ip::udp::endpoint             platform_endpoint_;

    // 定时器
    std::unique_ptr<asio::steady_timer> timer_;

    // 媒体参数
    InviteMediaInfo  media_info_;
    std::string      video_file_;
    uint32_t         frame_rate_ = 25;

    // 状态
    bool            streaming_ = false;
    uint32_t        ssrc_      = 0;
    uint16_t        seq_num_   = 0;
    uint32_t        rtp_timestamp_ = 0;
};

} // namespace gb28181
