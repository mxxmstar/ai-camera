#pragma once

#include <string>
#include "gb28181_config.h"
#include "gb28181_types.h"

namespace gb28181 {

// ============================================================================
// Gb28181SdpHelper：SDP 解析与构造
// ============================================================================
class Gb28181SdpHelper {
public:
    // 解析 INVITE SDP，提取平台接收地址和 SSRC
    static InviteMediaInfo ParseInviteSdp(const std::string& sdp);

    // 构造 200 OK SDP 响应
    static std::string BuildAnswerSdp(const Gb28181Config& config,
                                      const InviteMediaInfo& media_info);
};

} // namespace gb28181
