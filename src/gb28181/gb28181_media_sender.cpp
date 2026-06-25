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

    // TODO: Create ASIO io_context, UDP socket, timer
    // Open video file, start timer

    std::cout << "[GB28181] MediaSender started" << std::endl;
    streaming_ = true;
    return true;
}

void Gb28181MediaSender::Stop() {
    // TODO: Stop timer, close socket
    streaming_ = false;
    std::cout << "[GB28181] MediaSender stopped" << std::endl;
}

void Gb28181MediaSender::OnTimer(const asio::error_code& ec) {
    if (ec || !streaming_) return;

    // TODO: Read frame → PS encode → RTP fragment → UDP send
}

void Gb28181MediaSender::SendRtpPacket(const std::vector<uint8_t>& ps_data) {
    // TODO: RTP fragment send
}

} // namespace gb28181
