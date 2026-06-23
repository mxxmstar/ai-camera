#ifndef SERVER_WS_WS_HANDSHAKE_HPP
#define SERVER_WS_WS_HANDSHAKE_HPP

#include "ws/sha1.h"

#include <cctype>
#include <cstring>
#include <string_view>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

namespace ws {

/// @brief WebSocket 握手处理（RFC 6455 第 4 节）
///
/// 握手流程（服务端视角）：
/// 1. 客户端发起 HTTP GET 请求，携带 Upgrade: websocket 等头
/// 2. 服务端校验请求合法性
/// 3. 服务端计算 Sec-WebSocket-Accept 并返回 101 Switching Protocols
///
/// Sec-WebSocket-Accept 计算方式：
///   将客户端的 Sec-WebSocket-Key 与魔法字符串拼接，
///   做 SHA-1 摘要，再 Base64 编码。
///
///   Accept = Base64( SHA-1( Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" ) )
namespace handshake {

/// RFC 6455 规定的魔术GUID
constexpr const char* kMagicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/// @brief 解析 HTTP 请求头（简易实现，不依赖外部 HTTP 库）
///
/// 将形如以下文本解析为 method/path 和头部键值对：
/// ~~~
/// GET /chat HTTP/1.1
/// Host: example.com
/// Upgrade: websocket
/// Connection: Upgrade
/// Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
/// Sec-WebSocket-Version: 13
/// ~~~
struct HttpRequest {
    std::string method;                       ///< 请求方法（应为 GET）
    std::string path;                         ///< 请求路径
    std::string version;                      ///< HTTP 版本（如 "HTTP/1.1"）
    std::unordered_map<std::string, std::string> headers; ///< 头部（键已转小写）
};

/// @brief 从原始字节解析 HTTP 请求
/// @param data  数据首地址
/// @param size  数据字节数
/// @param[out] req 解析结果
/// @return >0：成功，返回已消费字节数（含结尾的 \r\n\r\n）；
///         ==0：数据不完整，需继续读取；
///         <0：解析失败。
inline long ParseHttpRequest(const uint8_t* data, std::size_t size, HttpRequest& req) {
    // 查找 \r\n\r\n 分隔符
    const char* end = nullptr;
    const char* start = reinterpret_cast<const char*>(data);
    for (std::size_t i = 0; i + 3 < size; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n' &&
            data[i + 2] == '\r' && data[i + 3] == '\n') {
            end = start + i;
            break;
        }
    }
    if (end == nullptr) return 0; // 未找到请求头结束标记

    std::string header_text(start, end);
    std::istringstream iss(header_text);
    std::string line;

    // 请求行
    if (!std::getline(iss, line)) return -1;
    // 去掉可能的 \r
    if (!line.empty() && line.back() == '\r') line.pop_back();
    {
        std::istringstream rl(line);
        rl >> req.method >> req.path >> req.version;
        if (req.method.empty() || req.path.empty() || req.version.empty()) return -1;
    }

    // 头部行
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        // 去除值首部空格
        std::size_t s = val.find_first_not_of(" \t");
        if (s != std::string::npos) val = val.substr(s);

        // key 转小写（便于不区分大小写查找）
        for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        req.headers[key] = val;
    }

    return static_cast<long>(end - start) + 4; // +4 为 \r\n\r\n
}

/// @brief 判断 HTTP 请求是否为有效的 WebSocket 升级请求
/// @return true 表示是有效的 WebSocket 握手请求
inline bool IsWebSocketUpgrade(const HttpRequest& req) {
    if (req.method != "GET") return false;

    auto find_ci = [&](const std::string& key) -> std::string {
        auto it = req.headers.find(key);
        return (it != req.headers.end()) ? it->second : std::string();
    };

    // Upgrade 头应包含 "websocket"（不区分大小写）
    std::string upgrade = find_ci("upgrade");
    for (auto& c : upgrade) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (upgrade.find("websocket") == std::string::npos) return false;

    // Connection 头应包含 "upgrade"
    std::string conn = find_ci("connection");
    for (auto& c : conn) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (conn.find("upgrade") == std::string::npos) return false;

    // 必须携带 Sec-WebSocket-Key
    if (find_ci("sec-websocket-key").empty()) return false;

    // 版本应为 13（也接受 8/7 以兼容旧版客户端）
    std::string ver = find_ci("sec-websocket-version");
    if (!ver.empty() && ver != "13" && ver != "8" && ver != "7") return false;

    return true;
}

/// @brief 去除字符串首尾空白
inline std::string Trim(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

/// @brief 按逗号分隔 HTTP token 列表
inline std::vector<std::string> SplitHeaderTokens(const std::string& value) {
    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        const std::size_t end = (comma == std::string::npos) ? value.size() : comma;
        std::string token = Trim(std::string_view(value).substr(start, end - start));
        if (!token.empty()) {
            tokens.push_back(std::move(token));
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return tokens;
}

/// @brief 获取客户端在握手时声明的所有子协议
inline std::vector<std::string> ParseSubProtocols(const HttpRequest& req) {
    auto it = req.headers.find("sec-websocket-protocol");
    if (it == req.headers.end()) {
        return {};
    }
    return SplitHeaderTokens(it->second);
}

/// @brief 按服务端支持列表协商子协议
inline std::string NegotiateSubProtocol(
    const HttpRequest& req,
    const std::vector<std::string>& supported_protocols)
{
    if (supported_protocols.empty()) {
        return {};
    }

    const auto requested_protocols = ParseSubProtocols(req);
    for (const auto& supported : supported_protocols) {
        for (const auto& requested : requested_protocols) {
            if (supported == requested) {
                return supported;
            }
        }
    }
    return {};
}

/// @brief 计算 Sec-WebSocket-Accept 值
/// @param key 客户端发送的 Sec-WebSocket-Key
/// @return 28 字符的 Accept 值
inline std::string ComputeAcceptKey(const std::string& key) {
    std::string combined = key + kMagicGuid;
    auto digest = crypto::Sha1::Hash(combined);
    return crypto::Base64Encode(digest);
}

/// @brief 构建 101 Switching Protocols 响应
/// @param key 客户端的 Sec-WebSocket-Key
/// @return 完整的 HTTP 响应文本
inline std::string BuildHandshakeResponse(
    const std::string& key,
    const std::string& subprotocol = {})
{
    std::string accept = ComputeAcceptKey(key);
    std::ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Accept: " << accept << "\r\n";
    if (!subprotocol.empty()) {
        oss << "Sec-WebSocket-Protocol: " << subprotocol << "\r\n";
    }
    oss << "\r\n";
    return oss.str();
}

} // namespace handshake
} // namespace ws

#endif // SERVER_WS_WS_HANDSHAKE_HPP
