#include "gb28181/gb28181_media_sender.h"
#include <iostream>

namespace gb28181 {

Gb28181MediaSender::Gb28181MediaSender() = default;

Gb28181MediaSender::~Gb28181MediaSender() {
    Stop();
}

bool Gb28181MediaSender::Start(const InviteMediaInfo& media_info,
                               const std::string& video_file,
                               uint32_t frame_rate) {
    media_info_ = media_info;
    video_file_ = video_file;
    frame_rate_ = frame_rate;

    // TODO: 创建 ASIO io_context、UDP socket、定时器
    // 打开视频文件，启动定时器

    std::cout << "[GB28181] MediaSender started" << std::endl;
    streaming_ = true;
    return true;
}

void Gb28181MediaSender::Stop() {
    // TODO: 停止定时器，关闭 socket
    streaming_ = false;
    std::cout << "[GB28181] MediaSender stopped" << std::endl;
}

void Gb28181MediaSender::OnTimer(const asio::error_code& ec) {
    if (ec || !streaming_) return;

    // TODO: 读取帧 → PS 编码 → RTP 分片 → UDP 发送
}

void Gb28181MediaSender::SendRtpPacket(const std::vector<uint8_t>& ps_data) {
    // TODO: RTP 分片发送
}

} // namespace gb28181
