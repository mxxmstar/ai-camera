#pragma once

#include <string>
#include <vector>
#include <memory>

namespace serial {

/// @brief 串口协议解析策略接口
///
/// 采用策略模式，不同协议继承此类并实现 OnData 方法。
/// 内置实现：LineProtocol（按行分帧）、LengthProtocol（长度字段分帧）、RawProtocol（透传）。
class SerialProtocol {
public:
    virtual ~SerialProtocol() = default;

    /// @brief 输入原始字节流，输出解析出的完整帧列表
    /// @param data  原始数据指针
    /// @param len   数据长度
    /// @return     解析出的完整帧（每条帧为一个 std::string）
    virtual std::vector<std::string> OnData(const char* data, size_t len) = 0;

    /// @brief 帧封装（发送时调用，子类可重写添加帧头/帧尾/校验等）
    /// @param payload 原始 payload
    /// @return        封装后的完整帧数据
    virtual std::string WrapFrame(const std::string& payload) { return payload; }
};

// ============================================================================
// 内置协议实现
// ============================================================================

/// @brief 按行分帧协议
///
/// 以指定分隔符（默认 "\n"）作为帧边界，适用于文本协议（如 AT 指令、NMEA 等）。
class LineProtocol : public SerialProtocol {
public:
    /// @param delimiter 行分隔符，默认 "\n"
    explicit LineProtocol(const std::string& delimiter = "\n");

    std::vector<std::string> OnData(const char* data, size_t len) override;
    std::string WrapFrame(const std::string& payload) override;

private:
    std::string delimiter_;
    std::string buffer_;  ///< 未满一行的数据缓存
};

/// @brief 长度字段分帧协议
///
/// 帧头包含长度字段，适用于二进制协议。
/// 帧格式：[长度字段(1/2/4字节)][payload...]
class LengthProtocol : public SerialProtocol {
public:
    /// @param field_size   长度字段字节数 (1/2/4)
    /// @param big_endian   长度字段字节序 (true=大端, false=小端)
    /// @param offset       长度字段在帧中的偏移量
    /// @param max_frame    最大帧长度（防止异常数据导致内存耗尽）
    LengthProtocol(unsigned int field_size = 2,
                   bool         big_endian = false,
                   unsigned int offset     = 0,
                   unsigned int max_frame  = 4096);

    std::vector<std::string> OnData(const char* data, size_t len) override;

private:
    unsigned int field_size_;
    bool         big_endian_;
    unsigned int offset_;
    unsigned int max_frame_;
    std::string  buffer_;
};

/// @brief 透传协议（不分帧）
///
/// 所有收到的数据立即作为一帧回调，适用于自定义解析或原始数据流场景。
class RawProtocol : public SerialProtocol {
public:
    std::vector<std::string> OnData(const char* data, size_t len) override;
};

/// @brief 根据协议名称创建对应协议实例
/// @param protocol_name "line" | "length" | "raw"
/// @param config        SerialConfig（部分协议需要读取额外配置）
/// @return              协议实例，未知协议名返回 nullptr
std::unique_ptr<SerialProtocol> CreateProtocol(const std::string&  protocol_name,
                                               const SerialConfig& config = {});

} // namespace serial
