#include "onvif/wsdiscovery.h"
#include "onvif/onvif_types.h"
#include "onvif/soap_helper.h"
#include <asio.hpp>
#include <cstring>
#include <random>
#include <ctime>
#include <iostream>
#include <memory>

using asio::ip::udp;
using asio::ip::address;
using asio::ip::address_v4;

namespace onvif {

static constexpr const char* WS_DISCOVERY_ADDR = "239.255.255.250";
static constexpr unsigned short  WS_DISCOVERY_PORT = 3702;

// =========================================================================
// 构造
// =========================================================================
WSDiscovery::WSDiscovery(const ServiceConfig& config)
    : config_(config), types_("tdn:NetworkVideoTransmitter")
{
    scopes_ = {
        DiscoveryScopes::NetworkVideoTransmitter,
        DiscoveryScopes::Device,
        std::string("onvif://www.onvif.org/name/") + config_.device_ip,
        std::string("onvif://www.onvif.org/type/video_encoder"),
    };
    device_id_ = SoapHelper::GenerateUuid();
    recv_buf_.resize(8192);
}

WSDiscovery::~WSDiscovery() { Stop(); }

// =========================================================================
// 启动
// =========================================================================
bool WSDiscovery::Start() {
    if (running_.load()) return true;
    try {
        io_ctx_ = std::make_unique<asio::io_context>();

        auto sock = std::make_unique<udp::socket>(*io_ctx_);
        sock->open(udp::v4());
        sock->set_option(asio::socket_base::reuse_address(true));
        sock->bind(udp::endpoint(udp::v4(), WS_DISCOVERY_PORT));

        // 加入多播组
        sock->set_option(asio::ip::multicast::join_group(
            asio::ip::make_address(WS_DISCOVERY_ADDR).to_v4()));
#ifdef _WIN32
        sock->set_option(asio::ip::multicast::enable_loopback(true));
#endif
        socket_raw_ = sock.get();

        // 启动异步接收循环
        DoReceive();

        running_.store(true);
        worker_thread_ = std::thread([this]() { io_ctx_->run(); });

        std::cout << "[WS-Discovery] Started on " << WS_DISCOVERY_ADDR
                  << ":" << WS_DISCOVERY_PORT << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WS-Discovery] Start failed: " << e.what() << std::endl;
        return false;
    }
}

// =========================================================================
// 停止
// =========================================================================
void WSDiscovery::Stop() {
    if (!running_.load()) return;
    running_.store(false);

    if (io_ctx_) io_ctx_->stop();

    if (socket_raw_) {
        auto* s = static_cast<udp::socket*>(socket_raw_);
        asio::error_code ec;
        s->close(ec);
        socket_raw_ = nullptr;
    }
    io_ctx_.reset();

    if (worker_thread_.joinable()) worker_thread_.join();
    std::cout << "[WS-Discovery] Stopped." << std::endl;
}

// =========================================================================
// 异步接收循环
// =========================================================================
void WSDiscovery::DoReceive() {
    if (!running_.load() || !socket_raw_) return;
    auto* s = static_cast<udp::socket*>(socket_raw_);

    auto sender = std::make_shared<udp::endpoint>();

    s->async_receive_from(
        asio::buffer(recv_buf_),
        *sender,
        [this, s, sender](const asio::error_code& ec, size_t bytes) {
            if (!running_.load()) return;
            if (!ec && bytes > 0) {
                HandleProbe(recv_buf_.data(), bytes);
            }
            // 继续接收
            if (running_.load()) {
                DoReceive();
            }
        });
}

// =========================================================================
// 设置 Scopes / Types
// =========================================================================
void WSDiscovery::SetScopes(const std::vector<std::string>& scopes) { scopes_ = scopes; }

void WSDiscovery::SetScopes(const std::string& scopes) {
    scopes_.clear();
    size_t pos = 0;
    while (pos < scopes.size()) {
        size_t end = scopes.find(';', pos);
        if (end == std::string::npos) end = scopes.size();
        std::string s = scopes.substr(pos, end - pos);
        if (!s.empty()) scopes_.push_back(s);
        pos = (end == scopes.size()) ? end : end + 1;
    }
}

void WSDiscovery::SetTypes(const std::string& types) { types_ = types; }

// =========================================================================
// 处理 Probe
// =========================================================================
void WSDiscovery::HandleProbe(const char* data, size_t len) {
    SoapHelper sh;
    SoapRequest req;
    if (!sh.ParseSoapRequest(std::string(data, len), req)) return;

    auto* s = static_cast<udp::socket*>(socket_raw_);
    if (!s) return;

    std::string probe_match_xml = BuildProbeMatch(
        req.message_id, device_id_, config_.DeviceServiceURL());

    udp::endpoint target(asio::ip::make_address(WS_DISCOVERY_ADDR), WS_DISCOVERY_PORT);
    asio::error_code ec;
    s->send_to(asio::buffer(probe_match_xml), target, 0, ec);
    if (ec) {
        std::cerr << "[WS-Discovery] Send ProbeMatch failed: "
                  << ec.message() << std::endl;
    }
}

// =========================================================================
// 构造 ProbeMatch SOAP 报文
// =========================================================================
std::string WSDiscovery::BuildProbeMatch(const std::string& relates_to,
                                           const std::string& endpoint_ref,
                                           const std::string& xaddrs) const {
    tinyxml2::XMLDocument doc;
    doc.InsertEndChild(doc.NewDeclaration());

    tinyxml2::XMLElement* env = doc.NewElement("SOAP-ENV:Envelope");
    env->SetAttribute("xmlns:SOAP-ENV", XmlNs::env);
    env->SetAttribute("xmlns:SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/");
    env->SetAttribute("xmlns:xsi",  XmlNs::xsi);
    env->SetAttribute("xmlns:xsd",  XmlNs::xsd);
    env->SetAttribute("xmlns:wsa",  XmlNs::wsa);
    env->SetAttribute("xmlns:wsdd", XmlNs::wsdd);
    env->SetAttribute("xmlns:tdn",  XmlNs::tdn);
    doc.InsertEndChild(env);

    tinyxml2::XMLElement* hdr = doc.NewElement("SOAP-ENV:Header");
    env->InsertEndChild(hdr);

    tinyxml2::XMLElement* act = doc.NewElement("wsa:Action");
    act->SetText("http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatch");
    hdr->InsertEndChild(act);

    tinyxml2::XMLElement* msg = doc.NewElement("wsa:MessageID");
    std::string my_uuid = SoapHelper::GenerateUuid();
    msg->SetText(my_uuid.c_str());
    hdr->InsertEndChild(msg);

    tinyxml2::XMLElement* rel = doc.NewElement("wsa:RelatesTo");
    rel->SetText(relates_to.c_str());
    hdr->InsertEndChild(rel);

    tinyxml2::XMLElement* to = doc.NewElement("wsa:To");
    to->SetText("http://schemas.xmlsoap.org/ws/2005/04/discovery");
    hdr->InsertEndChild(to);

    tinyxml2::XMLElement* body = doc.NewElement("SOAP-ENV:Body");
    env->InsertEndChild(body);

    tinyxml2::XMLElement* pm = doc.NewElement("wsdd:ProbeMatch");
    body->InsertEndChild(pm);

    tinyxml2::XMLElement* epr = doc.NewElement("wsa:EndpointReference");
    pm->InsertEndChild(epr);

    tinyxml2::XMLElement* addr = doc.NewElement("wsa:Address");
    addr->SetText(endpoint_ref.c_str());
    epr->InsertEndChild(addr);

    tinyxml2::XMLElement* types = doc.NewElement("wsdd:Types");
    types->SetText(types_.c_str());
    pm->InsertEndChild(types);

    if (!scopes_.empty()) {
        tinyxml2::XMLElement* sc = doc.NewElement("wsdd:Scopes");
        std::string scopes_str;
        for (size_t i = 0; i < scopes_.size(); ++i) {
            if (i > 0) scopes_str += " ";
            scopes_str += scopes_[i];
        }
        sc->SetText(scopes_str.c_str());
        pm->InsertEndChild(sc);
    }

    tinyxml2::XMLElement* xa = doc.NewElement("wsdd:XAddrs");
    xa->SetText(xaddrs.c_str());
    pm->InsertEndChild(xa);

    tinyxml2::XMLElement* mv = doc.NewElement("wsdd:MetadataVersion");
    mv->SetText("1");
    pm->InsertEndChild(mv);

    tinyxml2::XMLPrinter printer;
    doc.Accept(&printer);
    return std::string(printer.CStr());
}

} // namespace onvif
