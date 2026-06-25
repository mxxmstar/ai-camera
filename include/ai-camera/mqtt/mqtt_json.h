#ifndef AI_CAMERA_MQTT_MQTT_JSON_H
#define AI_CAMERA_MQTT_MQTT_JSON_H

#include <sstream>
#include <string>

namespace mqtt {

/// @brief 轻量 JSON 构建器（仅构建，不解析）
///
/// 设计目标：
///   - 零依赖，header-only
///   - 仅支持构建 JSON 字符串（项目入站消息用简单字符串匹配解析）
///   - 链式调用，接口简洁
///
/// 用法示例：
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
///   std::string json = JsonBuilder()
///       .Add("device_id", "AICAM-001")
///       .Add("timestamp", 1719234567)
///       .Add("online", true)
///       .Build();
///   // 输出：{"device_id":"AICAM-001","timestamp":1719234567,"online":true}
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class JsonBuilder {
public:
    JsonBuilder() { ss_ << "{"; }

    /// 添加字符串字段
    JsonBuilder& Add(const std::string& key, const std::string& value) {
        AddKey(key);
        ss_ << "\"" << EscapeString(value) << "\"";
        return *this;
    }

    /// 添加整数/浮点数字段
    template<typename T>
    JsonBuilder& Add(const std::string& key, T value) {
        AddKey(key);
        ss_ << value;
        return *this;
    }

    /// 添加布尔字段
    JsonBuilder& Add(const std::string& key, bool value) {
        AddKey(key);
        ss_ << (value ? "true" : "false");
        return *this;
    }

    /// 添加嵌套对象（需要手动调用 Build() 并拼接）
    /// 用法：.AddRaw("data", JsonBuilder().Add(...).Build())
    JsonBuilder& AddRaw(const std::string& key, const std::string& raw_json) {
        AddKey(key);
        ss_ << raw_json;
        return *this;
    }

    /// 构建最终 JSON 字符串
    std::string Build() {
        if (built_) return result_;
        ss_ << "}";
        result_  = ss_.str();
        built_   = true;
        return result_;
    }

private:
    void AddKey(const std::string& key) {
        if (first_) first_ = false;
        else        ss_ << ",";
        ss_ << "\"" << key << "\":";
    }

    static std::string EscapeString(const std::string& s) {
        std::ostringstream oss;
        for (char c : s) {
            switch (c) {
                case '\"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b";  break;
                case '\f': oss << "\\f";  break;
                case '\n': oss << "\\n";  break;
                case '\r': oss << "\\r";  break;
                case '\t': oss << "\\t";  break;
                default:   oss << c;       break;
            }
        }
        return oss.str();
    }

    std::ostringstream ss_;
    bool                first_ = true;
    bool                built_ = false;
    std::string         result_;
};

} // namespace mqtt

#endif // AI_CAMERA_MQTT_MQTT_JSON_H
