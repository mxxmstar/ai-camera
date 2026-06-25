#include "ai-camera/serial/serial_http.h"
#include "ai-camera/serial/serial_manager.h"
#include "ai-camera/log/logger.h"

#include <server/http/router.hpp>
#include <server/http/http_response.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace serial {

// ============================================================================
// HTTP API 路由注册
// ============================================================================

void RegisterSerialRoutes(http::Router& router) {
    // GET /serial/ports - 获取所有串口状态
    // GET /serial/ports?name=xxx - 获取指定串口状态
    router.get("/serial/ports", [](const http::Request& req) {
        auto& mgr = SerialManager::Instance();

        // 检查是否有 name 查询参数
        std::string query = req.query_string();
        std::string name = "";
        if (!query.empty()) {
            // 简单解析查询字符串: name=xxx
            auto pos = query.find("name=");
            if (pos != std::string::npos) {
                name = query.substr(pos + 5);
                // 去掉可能的后续参数
                auto amp = name.find('&');
                if (amp != std::string::npos) {
                    name = name.substr(0, amp);
                }
            }
        }

        if (!name.empty()) {
            // 获取指定串口状态
            if (!mgr.IsOpen(name)) {
                return http::Response::not_found("Port not found: " + name);
            }
            std::string json = mgr.GetPortStatusJson(name);
            return http::Response::ok(json, "application/json");
        }

        // 获取所有串口状态
        std::string json = mgr.GetAllStatusJson();
        return http::Response::ok(json, "application/json");
    });

    // POST /serial/ports - 打开新串口
    router.post("/serial/ports", [](const http::Request& req) {
        // 解析 JSON 请求体（简单字符串匹配）
        std::string body = req.body;
        if (body.empty()) {
            return http::Response::bad_request("Empty request body");
        }

        // 简单 JSON 解析（不依赖第三方库）
        auto ExtractString = [](const std::string& json,
                                const std::string& field) -> std::string {
            std::string key = "\"" + field + "\":\"";
            auto pos = json.find(key);
            if (pos == std::string::npos) return "";
            pos += key.size();
            auto end = json.find("\"", pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        };

        auto ExtractInt = [](const std::string& json,
                              const std::string& field) -> unsigned int {
            std::string key = "\"" + field + "\":";
            auto pos = json.find(key);
            if (pos == std::string::npos) return 0;
            pos += key.size();
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
            unsigned int val = 0;
            while (pos < json.size() && std::isdigit(json[pos])) {
                val = val * 10 + (json[pos] - '0');
                ++pos;
            }
            return val;
        };

        SerialConfig cfg;
        cfg.device    = ExtractString(body, "device");
        cfg.baud_rate = ExtractInt(body, "baud_rate");
        if (cfg.baud_rate == 0) cfg.baud_rate = 9600;

        cfg.data_bits = ExtractInt(body, "data_bits");
        if (cfg.data_bits == 0) cfg.data_bits = 8;

        std::string parity_str = ExtractString(body, "parity");
        cfg.parity = parity_str.empty() ? 'N' : parity_str[0];

        cfg.stop_bits = ExtractInt(body, "stop_bits");
        if (cfg.stop_bits == 0) cfg.stop_bits = 1;

        cfg.protocol = ExtractString(body, "protocol");
        if (cfg.protocol.empty()) cfg.protocol = "line";

        // 端口名称（从 JSON 或自动生成）
        std::string name = ExtractString(body, "name");
        if (name.empty()) {
            // 自动生成名称
            static int counter = 0;
            name = "port" + std::to_string(++counter);
        }

        auto& mgr = SerialManager::Instance();
        if (!mgr.IsRunning()) {
            mgr.Start();
        }

        bool ok = mgr.Open(name, cfg);
        if (!ok) {
            return http::Response::internal_error("Failed to open serial port");
        }

        std::string json = "{\"name\":\"" + name + "\",\"status\":\"opened\"}";
        return http::Response::ok(json, "application/json");
    });

    // DELETE /serial/ports?name=xxx - 关闭指定串口
    router.del("/serial/ports", [](const http::Request& req) {
        std::string query = req.query_string();
        std::string name = "";
        if (!query.empty()) {
            auto pos = query.find("name=");
            if (pos != std::string::npos) {
                name = query.substr(pos + 5);
                auto amp = name.find('&');
                if (amp != std::string::npos) {
                    name = name.substr(0, amp);
                }
            }
        }

        if (name.empty()) {
            return http::Response::bad_request("Missing 'name' query parameter");
        }

        auto& mgr = SerialManager::Instance();
        if (!mgr.IsOpen(name)) {
            return http::Response::not_found("Port not found: " + name);
        }

        mgr.Close(name);
        std::string json = "{\"name\":\"" + name + "\",\"status\":\"closed\"}";
        return http::Response::ok(json, "application/json");
    });

    // POST /serial/ports/send?name=xxx - 向指定串口发送数据
    router.post("/serial/ports/send", [](const http::Request& req) {
        // 从查询参数提取端口名称
        std::string query = req.query_string();
        std::string name = "";
        if (!query.empty()) {
            auto pos = query.find("name=");
            if (pos != std::string::npos) {
                name = query.substr(pos + 5);
                auto amp = name.find('&');
                if (amp != std::string::npos) {
                    name = name.substr(0, amp);
                }
            }
        }

        if (name.empty()) {
            return http::Response::bad_request("Missing 'name' query parameter");
        }

        // 解析请求体中的 data 字段
        std::string body = req.body;
        if (body.empty()) {
            return http::Response::bad_request("Empty request body");
        }

        auto ExtractString = [](const std::string& json,
                                const std::string& field) -> std::string {
            std::string key = "\"" + field + "\":\"";
            auto pos = json.find(key);
            if (pos == std::string::npos) return "";
            pos += key.size();
            auto end = json.find("\"", pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        };

        std::string data = ExtractString(body, "data");
        if (data.empty()) {
            return http::Response::bad_request("Missing 'data' field");
        }

        auto& mgr = SerialManager::Instance();
        bool ok = mgr.Send(name, data);
        if (!ok) {
            return http::Response::internal_error("Failed to send data");
        }

        std::string json = "{\"name\":\"" + name + "\",\"bytes\":" +
                          std::to_string(data.size()) + "}";
        return http::Response::ok(json, "application/json");
    });

    LOG_INFO("[SerialHttp] Registered serial API routes");
}

} // namespace serial
