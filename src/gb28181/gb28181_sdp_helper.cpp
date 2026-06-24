#include "gb28181/gb28181_sdp_helper.h"

namespace gb28181 {

InviteMediaInfo Gb28181SdpHelper::ParseInviteSdp(const std::string& sdp) {
    // TODO: 解析 INVITE SDP
    InviteMediaInfo info;
    return info;
}

std::string Gb28181SdpHelper::BuildAnswerSdp(const Gb28181Config& config,
                                             const InviteMediaInfo& media_info) {
    // TODO: 构造 200 OK SDP 响应
    return "";
}

} // namespace gb28181
