/**
 * @file mdns_impl.cpp
 * @brief mDNS 内部实现类
 */

#include "mdns/mdns_impl.h"
#include "mdns/dns_message.h"

#include <iostream>
#include <thread>
#include <algorithm>

namespace mdns {

// ============================================================
// 构造函数与析构函数
// ============================================================

MdnsServiceImpl::MdnsServiceImpl(MdnsService* parent)
    : parent_(parent)
    , io_context_()
    , io_thread_(nullptr)
    , socket_(io_context_)
    , multicast_endpoint_(asio::ip::make_address(MDNS_MULTICAST_ADDRESS), MDNS_PORT)
    , recv_buffer_(RECV_BUFFER_SIZE)
    , next_browse_id_(1)
    , initialized_(false)
    , running_(false) {
}

MdnsServiceImpl::~MdnsServiceImpl() {
    Shutdown();
}

// ============================================================
// 初始化与销毁
// ============================================================

bool MdnsServiceImpl::Init(const std::string& host_name, Protocol protocol) {
    if (initialized_) {
        return false;
    }

    host_name_ = host_name + ".local.";
    protocol_ = protocol;

    std::cout << "[mDNS] Initializing... host_name=" << host_name_ << std::endl;

    // 初始化多播socket
    if (!InitMulticastSocket()) {
        std::cerr << "[mDNS] Failed to initialize multicast socket" << std::endl;
        return false;
    }

    initialized_ = true;
    running_ = true;

    // 启动IO线程
    io_thread_ = std::make_unique<std::thread>([this]() {
        try {
            // 开始接收数据
            StartReceive();
            
            // 运行IO上下文
            io_context_.run();
        } catch (const std::exception& e) {
            std::cerr << "[mDNS] IO thread exception: " << e.what() << std::endl;
        }
    });

    std::cout << "[mDNS] Initialized successfully" << std::endl;
    return true;
}

void MdnsServiceImpl::Shutdown() {
    if (!initialized_) {
        return;
    }

    running_ = false;

    // 停止IO上下文
    io_context_.stop();

    // 等待IO线程结束
    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }

    // 关闭socket
    if (socket_.is_open()) {
        try {
            socket_.close();
        } catch (const std::exception& e) {
            std::cerr << "[mDNS] Error closing socket: " << e.what() << std::endl;
        }
    }

    // 清空服务列表和回调
    registered_services_.clear();
    discovered_services_.clear();
    browse_handlers_.clear();

    initialized_ = false;

    std::cout << "[mDNS] Shutdown complete" << std::endl;
}

// ============================================================
// 服务注册
// ============================================================

bool MdnsServiceImpl::RegisterService(const ServiceInfo& service) {
    if (!initialized_) {
        return false;
    }

    std::string key = service.name + "." + service.GetFullTypeString();
    registered_services_[key] = service;

    std::cout << "[mDNS] Registering service: " << key << " on port " << service.port << std::endl;
    
    // 构建并发送注册报文
    auto packet = BuildRegistrationPacket(service);
    if (!packet.empty()) {
        SendPacket(packet);
        std::cout << "[mDNS] Service registered successfully: " << key << std::endl;
        return true;
    } else {
        std::cerr << "[mDNS] Failed to build registration packet for: " << key << std::endl;
        return false;
    }
}

bool MdnsServiceImpl::UnregisterService(const std::string& service_name, ServiceType type) {
    if (!initialized_) {
        return false;
    }

    std::string type_str = ServiceTypeToString(type);
    std::string key = service_name + "." + type_str;

    auto it = registered_services_.find(key);
    if (it == registered_services_.end()) {
        return false;
    }

    registered_services_.erase(it);
    std::cout << "[mDNS] Unregistered service: " << key << std::endl;
    return true;
}

bool MdnsServiceImpl::UpdateServiceTxt(const std::string& service_name,
                                        ServiceType type,
                                        const std::map<std::string, std::string>& txt_records) {
    if (!initialized_) {
        return false;
    }

    std::string type_str = ServiceTypeToString(type);
    std::string key = service_name + "." + type_str;

    auto it = registered_services_.find(key);
    if (it == registered_services_.end()) {
        return false;
    }

    it->second.txt_records = txt_records;
    return true;
}

// ============================================================
// 服务发现
// ============================================================

int MdnsServiceImpl::StartBrowse(ServiceType type,
                                  ServiceFoundHandler on_found,
                                  ServiceLostHandler on_lost) {
    if (!initialized_) {
        return -1;
    }

    int browse_id = next_browse_id_++;
    browse_handlers_[browse_id] = {on_found, on_lost};

    std::string service_type = ServiceTypeToString(type);
    std::cout << "[mDNS] Started browsing for: " << service_type
              << " (browse_id=" << browse_id << ")" << std::endl;

    // 发送查询报文
    auto query_packet = BuildQueryPacket(service_type);
    if (!query_packet.empty()) {
        SendPacket(query_packet);
        std::cout << "[mDNS] Sent query for: " << service_type << std::endl;
    }

    return browse_id;
}

void MdnsServiceImpl::StopBrowse(int browse_id) {
    auto it = browse_handlers_.find(browse_id);
    if (it != browse_handlers_.end()) {
        browse_handlers_.erase(it);
        std::cout << "[mDNS] Stopped browsing (browse_id=" << browse_id << ")" << std::endl;
    }
}

// ============================================================
// 服务解析
// ============================================================

void MdnsServiceImpl::ResolveService(const std::string& service_name,
                                     ServiceType type,
                                     ServiceResolvedHandler handler) {
    if (!initialized_) {
        return;
    }

    std::cout << "[mDNS] Resolving service: " << service_name << std::endl;

    // 简化版：直接调用回调，传入空信息
    if (handler) {
        ServiceInfo info;
        info.name = service_name;
        info.type = type;
        handler(info);
    }
}

// ============================================================
// 查询接口
// ============================================================

std::vector<ServiceInfo> MdnsServiceImpl::GetRegisteredServices() const {
    std::vector<ServiceInfo> result;
    for (const auto& [key, service] : registered_services_) {
        result.push_back(service);
    }
    return result;
}

std::vector<ServiceInfo> MdnsServiceImpl::GetDiscoveredServices(ServiceType type) const {
    std::vector<ServiceInfo> result;

    if (type == ServiceType::Custom) {
        for (const auto& [svc_type, services] : discovered_services_) {
            result.insert(result.end(), services.begin(), services.end());
        }
    } else {
        auto it = discovered_services_.find(type);
        if (it != discovered_services_.end()) {
            result = it->second;
        }
    }

    return result;
}

// ============================================================
// 私有方法（简化版，暂不实现实际网络功能）
// ============================================================

bool MdnsServiceImpl::InitMulticastSocket() {
    try {
        // 创建 UDP socket
        asio::ip::udp::socket socket(io_context_);
        socket_ = std::move(socket);
        
        // 允许端口复用（多播需要）
        socket_.set_option(asio::ip::udp::socket::reuse_address(true));
        
        // 绑定到多播端口
        asio::ip::udp::endpoint local_endpoint(asio::ip::address::from_string("0.0.0.0"), MDNS_PORT);
        socket_.bind(local_endpoint);
        
        // 加入多播组
        asio::ip::address multicast_address = asio::ip::address::from_string(MDNS_MULTICAST_ADDRESS);
        socket_.set_option(asio::ip::multicast::join_group(multicast_address.to_v4()));
        
        // 设置多播输出接口
        socket_.set_option(asio::ip::multicast::outbound_interface(asio::ip::address_v4::any()));
        
        std::cout << "[mDNS] Multicast socket initialized successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[mDNS] Failed to initialize multicast socket: " << e.what() << std::endl;
        return false;
    }
}

void MdnsServiceImpl::SendPacket(const std::vector<uint8_t>& packet) {
    if (!socket_.is_open()) {
        std::cerr << "[mDNS] Socket not open, cannot send packet" << std::endl;
        return;
    }

    try {
        asio::ip::udp::endpoint multicast_ep(
            asio::ip::address::from_string(MDNS_MULTICAST_ADDRESS), 
            MDNS_PORT
        );
        
        socket_.send_to(asio::buffer(packet), multicast_ep);
        std::cout << "[mDNS] Sent packet: " << packet.size() << " bytes" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[mDNS] Failed to send packet: " << e.what() << std::endl;
    }
}

void MdnsServiceImpl::StartReceive() {
    if (!socket_.is_open()) {
        return;
    }

    socket_.async_receive_from(
        asio::buffer(recv_buffer_),
        multicast_endpoint_,
        [this](const asio::error_code& error, std::size_t bytes_received) {
            if (!error && bytes_received > 0) {
                std::vector<uint8_t> packet(recv_buffer_.begin(), 
                                            recv_buffer_.begin() + bytes_received);
                HandleReceivedPacket(packet, multicast_endpoint_);
            }
            
            // 继续接收
            if (running_) {
                StartReceive();
            }
        }
    );
}

void MdnsServiceImpl::HandleReceivedPacket(const std::vector<uint8_t>& packet,
                                            const asio::ip::udp::endpoint& sender) {
    auto msg_opt = DnsMessageCodec::Decode(packet);
    if (!msg_opt) {
        std::cerr << "[mDNS] Failed to decode received packet" << std::endl;
        return;
    }

    const auto& msg = *msg_opt;
    
    std::cout << "[mDNS] Received packet from " << sender.address().to_string() 
              << ":" << sender.port() << std::endl;
    
    // 处理回答记录（服务发现响应）
    for (const auto& rr : msg.answers) {
        if (rr->type == static_cast<uint16_t>(DnsRecordType::PTR)) {
            auto ptr_rr = std::dynamic_pointer_cast<DnsPtrRecord>(rr);
            if (ptr_rr) {
                std::cout << "[mDNS] Found service: " << ptr_rr->ptr_name << std::endl;
                
                // 触发服务发现回调
                for (const auto& [browse_id, handlers] : browse_handlers_) {
                    if (handlers.first) {
                        ServiceInfo info;
                        info.name = ptr_rr->ptr_name;
                        info.type = ServiceType::Custom;  // 需要从ptr_name解析
                        handlers.first(info);
                    }
                }
            }
        }
    }
    
    // 处理附加记录（获取详细信息）
    for (const auto& rr : msg.additional) {
        if (rr->type == static_cast<uint16_t>(DnsRecordType::SRV)) {
            auto srv_rr = std::dynamic_pointer_cast<DnsSrvRecord>(rr);
            if (srv_rr) {
                std::cout << "[mDNS] Service " << rr->name << " at port " << srv_rr->port << std::endl;
            }
        } else if (rr->type == static_cast<uint16_t>(DnsRecordType::A)) {
            auto a_rr = std::dynamic_pointer_cast<DnsARecord>(rr);
            if (a_rr) {
                std::cout << "[mDNS] Service IP: " << a_rr->ipv4 << std::endl;
            }
        }
    }
}

std::vector<uint8_t> MdnsServiceImpl::BuildRegistrationPacket(const ServiceInfo& service) {
    DnsMessage msg;
    
    // 设置头部（响应报文）
    msg.header.id = 0;
    msg.header.SetQR(true);  // 设置为响应
    msg.header.flags = htons(0x8400);  // Authoritative answer
    
    // 构建服务类型
    std::string service_type = service.GetFullTypeString();
    std::string instance_name = service.name + "." + service_type;
    
    // 添加 PTR 记录（指向实例名称）
    {
        auto ptr_rr = std::make_shared<DnsPtrRecord>();
        ptr_rr->name = service_type + ".local.";
        ptr_rr->type = static_cast<uint16_t>(DnsRecordType::PTR);
        ptr_rr->class_ = static_cast<uint16_t>(DnsRecordClass::INet);
        ptr_rr->ttl = 4500;  // 75 minutes
        ptr_rr->ptr_name = instance_name + ".local.";
        
        // 编码 PTR 数据
        auto name_bytes = DnsMessageCodec::EncodeName(ptr_rr->ptr_name);
        ptr_rr->data = name_bytes;
        
        msg.answers.push_back(ptr_rr);
    }
    
    // 添加 SRV 记录（服务位置）
    {
        auto srv_rr = std::make_shared<DnsSrvRecord>();
        srv_rr->name = instance_name + ".local.";
        srv_rr->type = static_cast<uint16_t>(DnsRecordType::SRV);
        srv_rr->class_ = static_cast<uint16_t>(DnsRecordClass::INet);
        srv_rr->ttl = 120;
        srv_rr->priority = 0;
        srv_rr->weight = 0;
        srv_rr->port = service.port;
        srv_rr->target = host_name_;
        
        // 编码 SRV 数据
        std::vector<uint8_t> srv_data;
        uint16_t priority_net = htons(srv_rr->priority);
        uint16_t weight_net = htons(srv_rr->weight);
        uint16_t port_net = htons(srv_rr->port);
        srv_data.insert(srv_data.end(), reinterpret_cast<uint8_t*>(&priority_net), 
                        reinterpret_cast<uint8_t*>(&priority_net) + 2);
        srv_data.insert(srv_data.end(), reinterpret_cast<uint8_t*>(&weight_net), 
                        reinterpret_cast<uint8_t*>(&weight_net) + 2);
        srv_data.insert(srv_data.end(), reinterpret_cast<uint8_t*>(&port_net), 
                        reinterpret_cast<uint8_t*>(&port_net) + 2);
        auto target_bytes = DnsMessageCodec::EncodeName(srv_rr->target);
        srv_data.insert(srv_data.end(), target_bytes.begin(), target_bytes.end());
        srv_rr->data = srv_data;
        
        msg.additional.push_back(srv_rr);
    }
    
    // 添加 TXT 记录
    {
        auto txt_rr = std::make_shared<DnsTxtRecord>();
        txt_rr->name = instance_name + ".local.";
        txt_rr->type = static_cast<uint16_t>(DnsRecordType::TXT);
        txt_rr->class_ = static_cast<uint16_t>(DnsRecordClass::INet);
        txt_rr->ttl = 4500;
        txt_rr->txt_data = service.txt_records;
        
        // 编码 TXT 数据
        std::vector<uint8_t> txt_data;
        for (const auto& [key, value] : service.txt_records) {
            std::string txt_str = key + "=" + value;
            if (txt_str.size() <= 255) {
                txt_data.push_back(static_cast<uint8_t>(txt_str.size()));
                txt_data.insert(txt_data.end(), txt_str.begin(), txt_str.end());
            }
        }
        txt_rr->data = txt_data;
        
        msg.additional.push_back(txt_rr);
    }
    
    // 添加 A 记录（IPv4 地址）
    if (!service.ipv4.empty()) {
        auto a_rr = std::make_shared<DnsARecord>();
        a_rr->name = host_name_;
        a_rr->type = static_cast<uint16_t>(DnsRecordType::A);
        a_rr->class_ = static_cast<uint16_t>(DnsRecordClass::INet);
        a_rr->ttl = 120;
        a_rr->ipv4 = service.ipv4;
        
        // 编码 A 数据
        std::vector<uint8_t> a_data;
        asio::ip::address_v4::bytes_type bytes = asio::ip::address_v4::from_string(service.ipv4).to_bytes();
        a_data.insert(a_data.end(), bytes.begin(), bytes.end());
        a_rr->data = a_data;
        
        msg.additional.push_back(a_rr);
    }
    
    // 编码为二进制报文
    return DnsMessageCodec::Encode(msg);
}

std::vector<uint8_t> MdnsServiceImpl::BuildQueryPacket(const std::string& service_type) {
    DnsMessage msg;
    
    // 设置头部（查询报文）
    msg.header.id = 0;
    msg.header.SetQR(false);  // 设置为查询
    msg.header.flags = 0;
    
    // 添加问题记录
    DnsQuestion q;
    q.name = service_type + ".local.";
    q.type = static_cast<uint16_t>(DnsRecordType::PTR);
    q.class_ = static_cast<uint16_t>(DnsRecordClass::INet);
    msg.questions.push_back(q);
    
    // 编码为二进制报文
    return DnsMessageCodec::Encode(msg);
}

} // namespace mdns
