#pragma once

#include <cstdint>
#include <vector>
#include "rtp/nalsource.h"

namespace gb28181 {

// ============================================================================
// PsEncoder：将 H.264 NAL 封装为 MPEG-PS 流
// ============================================================================
class PsEncoder {
public:
    PsEncoder() = default;
    ~PsEncoder() = default;

    // 编码一帧：NALUnitList → MPEG-PS 流字节
    std::vector<uint8_t> EncodeFrame(const rtp::NALSource::NALUnitList& nals,
                                     uint64_t timestamp,
                                     bool is_keyframe);

private:
    // 构建 PS Pack Header (0x000001BA + SCR + mux_rate)
    std::vector<uint8_t> BuildPackHeader(uint64_t scr, uint32_t mux_rate);

    // 构建 PS System Header (0x000001BB，关键帧前插入)
    std::vector<uint8_t> BuildSystemHeader();

    // 构建 PES Packet (stream_id=0xE0 + H264 NAL payload)
    std::vector<uint8_t> BuildPESPacket(uint8_t stream_id,
                                        const std::vector<uint8_t>& payload,
                                        uint64_t pts,
                                        uint64_t dts);
};

} // namespace gb28181
