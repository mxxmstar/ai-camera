#pragma once

#include <string>
#include "onvif_types.h"
#include "soap_helper.h"

namespace onvif {

// ============================================================================
// Media Service：处理 ONVIF 媒体相关 SOAP 操作
// 对应 trt: 命名空间 (http://www.onvif.org/ver10/media/wsdl)
// ============================================================================
class MediaService {
public:
    explicit MediaService(const ServiceConfig& config)
        : config_(config) {}
    ~MediaService() = default;

    // ------------------------------------------------------------------
    // 主入口：根据 SOAP 操作名分发到对应处理函数
    // 返回 SOAP 响应体（完整的 XML 字符串，含 SOAP Envelope）
    // ------------------------------------------------------------------
    std::string HandleRequest(const std::string& soap_body);

private:
    // === ONVIF Media 标准操作 ===

    // GetProfiles：返回媒体配置文件列表（Profile S）
    std::string HandleGetProfiles(const SoapRequest& req);
    // GetStreamUri：返回 RTSP 流地址（对接现有 RtspManager）
    std::string HandleGetStreamUri(const SoapRequest& req);
    // GetVideoSources：返回视频源配置
    std::string HandleGetVideoSources(const SoapRequest& req);
    // GetVideoSourceConfigurations：返回视频源配置列表
    std::string HandleGetVideoSourceConfigurations(const SoapRequest& req);
    // GetVideoEncoderConfigurations：返回视频编码器配置
    std::string HandleGetVideoEncoderConfigurations(const SoapRequest& req);
    // GetAudioSources：返回音频源配置
    std::string HandleGetAudioSources(const SoapRequest& req);
    // GetAudioSourceConfigurations：返回音频源配置列表
    std::string HandleGetAudioSourceConfigurations(const SoapRequest& req);
    // GetAudioEncoderConfigurations：返回音频编码器配置
    std::string HandleGetAudioEncoderConfigurations(const SoapRequest& req);
    // GetProfile：返回指定 Token 的 Profile 详情
    std::string HandleGetProfile(const SoapRequest& req);
    // GetSnapshotUri：返回快照 URI（可选实现）
    std::string HandleGetSnapshotUri(const SoapRequest& req);

    // ------------------------------------------------------------------
    // 构造媒体服务响应 XML 片段（插入 SOAP Body）
    // ------------------------------------------------------------------
    std::string BuildProfileToken() const;
    std::string BuildVideoSourceConfiguration() const;
    std::string BuildVideoEncoderConfiguration() const;
    std::string BuildAudioSourceConfiguration() const;
    std::string BuildAudioEncoderConfiguration() const;

    const ServiceConfig& config_;
    SoapHelper          soap_helper_;
};

} // namespace onvif
