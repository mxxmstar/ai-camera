#include "gb28181/gb28181_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

// 使用 tinyxml2 构造 XML 报文
#include <tinyxml2.h>

namespace gb28181 {

// ============================================================================
// 单例实现
// ============================================================================

Gb28181Manager& Gb28181Manager::Instance() {
    static Gb28181Manager instance;
    return instance;
}

Gb28181Manager::Gb28181Manager() {
    std::cout << "[GB28181] Gb28181Manager created" << std::endl;
}

Gb28181Manager::~Gb28181Manager() {
    Stop();
}

// ============================================================================
// 模块生命周期
// ============================================================================

bool Gb28181Manager::Init(const Gb28181Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        std::cout << "[GB28181] Already initialized, ignoring duplicate call" << std::endl;
        return true;
    }

    config_ = config;

    // 创建 SipAgent
    sip_agent_ = std::make_unique<SipAgent>();

    // 注册 SIP 事件回调
    SipEventCallbacks callbacks;
    callbacks.on_invite = [this](const std::string& from, const std::string& to,
                                  const std::string& call_id, const std::string& sdp) {
        OnInvite(from, to, call_id, sdp);
    };
    callbacks.on_bye = [this](const std::string& call_id) {
        OnBye(call_id);
    };
    callbacks.on_message = [this](const std::string& from, const std::string& xml_body) {
        OnMessage(from, xml_body);
    };
    callbacks.on_register_success = [this]() {
        OnRegisterSuccess();
    };
    callbacks.on_register_failed = [this](int code, const std::string& reason) {
        OnRegisterFailed(code, reason);
    };
    callbacks.on_subscribe = [this](const std::string& from,
                                     const std::string& call_id,
                                     const std::string& event_type) {
        OnSubscribe(from, call_id, event_type);
    };

    sip_agent_->SetCallbacks(callbacks);

    if (!sip_agent_->Init(config_)) {
        std::cerr << "[GB28181] SipAgent initialization failed" << std::endl;
        sip_agent_.reset();
        return false;
    }

    initialized_ = true;
    std::cout << "[GB28181] Gb28181Manager initialized successfully, device ID: "
              << config_.device_id << std::endl;
    return true;
}

bool Gb28181Manager::Start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        std::cerr << "[GB28181] Not initialized, please call Init() first" << std::endl;
        return false;
    }

    if (running_) {
        std::cout << "[GB28181] Already running" << std::endl;
        return true;
    }

    if (!sip_agent_->Start()) {
        std::cerr << "[GB28181] SipAgent start failed" << std::endl;
        return false;
    }

    running_ = true;
    std::cout << "[GB28181] Gb28181Manager started successfully" << std::endl;
    return true;
}

void Gb28181Manager::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        return;
    }

    if (sip_agent_) {
        sip_agent_->Stop();
    }

    running_    = false;
    registered_ = false;
    std::cout << "[GB28181] Gb28181Manager stopped" << std::endl;
}

bool Gb28181Manager::IsRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

// ============================================================================
// 平台交互接口
// ============================================================================

bool Gb28181Manager::ReportCatalog(const std::vector<Gb28181Channel>& channels) {
    if (!sip_agent_ || !sip_agent_->IsRegistered()) {
        std::cerr << "[GB28181] Not registered to platform, cannot report Catalog" << std::endl;
        return false;
    }

    // 构造 Catalog 响应 XML
    std::string sn = "1";  // 序号，实际应从请求中获取
    std::string xml_body = BuildCatalogResponse(sn, channels);

    // 通过 SIP MESSAGE 发送
    std::string platform_uri = "sip:" + config_.server_ip + ":" +
                                std::to_string(config_.server_port);
    return sip_agent_->SendMessage(platform_uri, xml_body);
}

bool Gb28181Manager::ReportDeviceInfo(const Gb28181DeviceInfo& info) {
    if (!sip_agent_ || !sip_agent_->IsRegistered()) {
        std::cerr << "[GB28181] 未注册到平台，无法上报 DeviceInfo" << std::endl;
        return false;
    }

    std::string sn = "1";
    std::string xml_body = BuildDeviceInfoResponse(sn, info);

    std::string platform_uri = "sip:" + config_.server_ip + ":" +
                                std::to_string(config_.server_port);
    return sip_agent_->SendMessage(platform_uri, xml_body);
}

bool Gb28181Manager::ReportAlarm(const Gb28181Alarm& alarm) {
    if (!sip_agent_ || !sip_agent_->IsRegistered()) {
        std::cerr << "[GB28181] 未注册到平台，无法上报告警" << std::endl;
        return false;
    }

    // 构造 Alarm 响应 XML（略，后续实现）
    std::cout << "[GB28181] ReportAlarm: 暂未实现" << std::endl;
    return true;
}

bool Gb28181Manager::SendKeepalive() {
    if (!sip_agent_ || !sip_agent_->IsRegistered()) {
        return false;
    }
    return sip_agent_->SendRegister();  // 保活就是重新发送 REGISTER
}

// ============================================================================
// 查询接口
// ============================================================================

Gb28181Config Gb28181Manager::GetConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool Gb28181Manager::IsRegistered() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return registered_;
}

// ============================================================================
// SipAgent 回调处理（私有方法）
// ============================================================================

void Gb28181Manager::OnInvite(const std::string& from, const std::string& to,
                               const std::string& call_id, const std::string& sdp) {
    std::cout << "[GB28181] OnInvite: call_id=" << call_id
              << ", from=" << from << std::endl;
    std::cout << "[GB28181] 收到 SDP:" << std::endl << sdp << std::endl;

    // TODO: 解析 SDP，获取平台接收地址和端口，启动 RTP 推流
    // 目前先回复 200 OK（带占位 SDP）
    if (sip_agent_) {
        std::string local_sdp = "v=0\r\n"
            "o=" + config_.device_id + " 0 0 IN IP4 127.0.0.1\r\n"
            "s=Play\r\n"
            "c=IN IP4 127.0.0.1\r\n"
            "t=0 0\r\n"
            "m=video 50000 RTP/AVP 96\r\n"
            "a=rtpmap:96 PS/90000\r\n"
            "a=sendonly\r\n";
        sip_agent_->SendInviteResponse(call_id, from, local_sdp);
    }
}

void Gb28181Manager::OnBye(const std::string& call_id) {
    std::cout << "[GB28181] OnBye: call_id=" << call_id << std::endl;
    // TODO: 停止对应的 RTP 推流
}

void Gb28181Manager::OnMessage(const std::string& from, const std::string& xml_body) {
    std::cout << "[GB28181] OnMessage: from=" << from << std::endl;
    std::cout << "[GB28181] XML 内容:" << std::endl << xml_body << std::endl;

    // 解析 XML，判断命令类型
    // 使用 tinyxml2 解析
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml_body.c_str()) != tinyxml2::XML_SUCCESS) {
        std::cerr << "[GB28181] XML 解析失败" << std::endl;
        return;
    }

    tinyxml2::XMLElement* cmd_type_elem = doc.FirstChildElement("CmdType");
    if (!cmd_type_elem) {
        // 可能在 Response 节点内
        cmd_type_elem = doc.FirstChildElement("Response");
        if (cmd_type_elem) {
            cmd_type_elem = cmd_type_elem->FirstChildElement("CmdType");
        }
    }

    if (!cmd_type_elem) {
        std::cerr << "[GB28181] 无法识别 CmdType" << std::endl;
        return;
    }

    std::string cmd_type = cmd_type_elem->GetText();
    std::cout << "[GB28181] 命令类型: " << cmd_type << std::endl;

    if (cmd_type == "Catalog") {
        // 目录查询：上报设备通道信息
        // TODO: 从配置或实际设备获取通道列表
        std::vector<Gb28181Channel> channels;
        Gb28181Channel ch;
        ch.device_id = config_.device_id;
        ch.name      = "AI Camera Channel 1";
        ch.status    = "ON";
        channels.push_back(ch);
        ReportCatalog(channels);
    } else if (cmd_type == "DeviceInfo") {
        // 设备信息查询
        Gb28181DeviceInfo info;
        info.ip_addr = "127.0.0.1";  // TODO: 获取本机 IP
        info.port    = config_.local_sip_port;
        ReportDeviceInfo(info);
    } else if (cmd_type == "Keepalive") {
        // 心跳响应（平台发来的心跳确认，一般不需要回复）
        std::cout << "[GB28181] 收到心跳确认" << std::endl;
    } else {
        std::cout << "[GB28181] 未处理的命令类型: " << cmd_type << std::endl;
    }
}

void Gb28181Manager::OnRegisterSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);
    registered_ = true;
    std::cout << "[GB28181] 注册成功！" << std::endl;
}

void Gb28181Manager::OnRegisterFailed(int status_code, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    registered_ = false;
    std::cerr << "[GB28181] 注册失败: " << status_code << " " << reason << std::endl;
}

void Gb28181Manager::OnSubscribe(const std::string& from, const std::string& call_id,
                                  const std::string& event_type) {
    std::cout << "[GB28181] OnSubscribe: event_type=" << event_type
              << ", from=" << from << std::endl;
    // TODO: 处理订阅，定期上报事件
}

// ============================================================================
// XML 报文构造
// ============================================================================

std::string Gb28181Manager::BuildCatalogResponse(
    const std::string& sn,
    const std::vector<Gb28181Channel>& channels) {

    tinyxml2::XMLDocument doc;
    auto* decl = doc.NewDeclaration(R"(xml version="1.0" encoding="GB2312")");
    doc.InsertFirstChild(decl);

    auto* response = doc.NewElement("Response");
    doc.InsertEndChild(response);

    auto* cmd_type = doc.NewElement("CmdType");
    cmd_type->SetText("Catalog");
    response->InsertEndChild(cmd_type);

    auto* sn_elem = doc.NewElement("SN");
    sn_elem->SetText(sn.c_str());
    response->InsertEndChild(sn_elem);

    auto* device_id = doc.NewElement("DeviceID");
    device_id->SetText(config_.device_id.c_str());
    response->InsertEndChild(device_id);

    auto* sum_num = doc.NewElement("SumNum");
    sum_num->SetText(static_cast<int>(channels.size()));
    response->InsertEndChild(sum_num);

    auto* cmd = doc.NewElement("Cmd");
    response->InsertEndChild(cmd);
    auto* info = doc.NewElement("Info");
    cmd->InsertEndChild(info);

    for (const auto& ch : channels) {
        auto* item = doc.NewElement("CmdItem");

        auto* id = doc.NewElement("DeviceID");
        id->SetText(ch.device_id.c_str());
        item->InsertEndChild(id);

        auto* name = doc.NewElement("Name");
        name->SetText(ch.name.c_str());
        item->InsertEndChild(name);

        auto* manufacturer = doc.NewElement("Manufacturer");
        manufacturer->SetText(ch.manufacturer.c_str());
        item->InsertEndChild(manufacturer);

        auto* model = doc.NewElement("Model");
        model->SetText(ch.model.c_str());
        item->InsertEndChild(model);

        auto* status = doc.NewElement("Status");
        status->SetText(ch.status.c_str());
        item->InsertEndChild(status);

        info->InsertEndChild(item);
    }

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return std::string(printer.CStr());
}

std::string Gb28181Manager::BuildDeviceInfoResponse(
    const std::string& sn,
    const Gb28181DeviceInfo& info) {

    tinyxml2::XMLDocument doc;
    auto* decl = doc.NewDeclaration(R"(xml version="1.0" encoding="GB2312")");
    doc.InsertFirstChild(decl);

    auto* response = doc.NewElement("Response");
    doc.InsertEndChild(response);

    auto* cmd_type = doc.NewElement("CmdType");
    cmd_type->SetText("DeviceInfo");
    response->InsertEndChild(cmd_type);

    auto* sn_elem = doc.NewElement("SN");
    sn_elem->SetText(sn.c_str());
    response->InsertEndChild(sn_elem);

    auto* device_id = doc.NewElement("DeviceID");
    device_id->SetText(config_.device_id.c_str());
    response->InsertEndChild(device_id);

    auto* device_info = doc.NewElement("DeviceInfo");
    response->InsertEndChild(device_info);

    auto* name = doc.NewElement("DeviceName");
    name->SetText(info.device_name.c_str());
    device_info->InsertEndChild(name);

    auto* manufacturer = doc.NewElement("Manufacturer");
    manufacturer->SetText(info.manufacturer.c_str());
    device_info->InsertEndChild(manufacturer);

    auto* model = doc.NewElement("Model");
    model->SetText(info.model.c_str());
    device_info->InsertEndChild(model);

    auto* firmware = doc.NewElement("Firmware");
    firmware->SetText(info.firmware.c_str());
    device_info->InsertEndChild(firmware);

    auto* serial = doc.NewElement("SerialNumber");
    serial->SetText(info.serial.c_str());
    device_info->InsertEndChild(serial);

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return std::string(printer.CStr());
}

std::string Gb28181Manager::BuildKeepaliveResponse(const std::string& sn) {
    tinyxml2::XMLDocument doc;
    auto* decl = doc.NewDeclaration(R"(xml version="1.0" encoding="GB2312")");
    doc.InsertFirstChild(decl);

    auto* keepalive = doc.NewElement("Keepalive");
    doc.InsertEndChild(keepalive);

    auto* cmd_type = doc.NewElement("CmdType");
    cmd_type->SetText("Keepalive");
    keepalive->InsertEndChild(cmd_type);

    auto* sn_elem = doc.NewElement("SN");
    sn_elem->SetText(sn.c_str());
    keepalive->InsertEndChild(sn_elem);

    auto* device_id = doc.NewElement("DeviceID");
    device_id->SetText(config_.device_id.c_str());
    keepalive->InsertEndChild(device_id);

    auto* status = doc.NewElement("Status");
    status->SetText("OK");
    keepalive->InsertEndChild(status);

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return std::string(printer.CStr());
}

} // namespace gb28181
