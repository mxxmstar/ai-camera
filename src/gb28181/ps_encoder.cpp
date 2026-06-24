#include "gb28181/ps_encoder.h"

namespace gb28181 {

std::vector<uint8_t> PsEncoder::EncodeFrame(const rtp::NALSource::NALUnitList& nals,
                                            uint64_t timestamp,
                                            bool is_keyframe) {
    // TODO: 实现 PS 编码
    return {};
}

std::vector<uint8_t> PsEncoder::BuildPackHeader(uint64_t scr, uint32_t mux_rate) {
    // TODO: 构建 PS Pack Header
    return {};
}

std::vector<uint8_t> PsEncoder::BuildSystemHeader() {
    // TODO: 构建 PS System Header
    return {};
}

std::vector<uint8_t> PsEncoder::BuildPESPacket(uint8_t stream_id,
                                               const std::vector<uint8_t>& payload,
                                               uint64_t pts,
                                               uint64_t dts) {
    // TODO: 构建 PES Packet
    return {};
}

} // namespace gb28181
