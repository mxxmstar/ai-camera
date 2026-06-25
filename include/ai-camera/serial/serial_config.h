#pragma once

#include <string>

namespace serial {

/// @brief 串口配置参数
struct SerialConfig {
    std::string device;  ///< 设备路径: "COM1" (Windows) 或 "/dev/ttyUSB0" (Linux)

    unsigned int baud_rate = 9600;   ///< 波特率
    unsigned int data_bits = 8;      ///< 数据位 (5/6/7/8)
    char         parity    = 'N';    ///< 校验位: 'N'=无, 'O'=奇, 'E'=偶
    unsigned int stop_bits = 1;      ///< 停止位 (1/2)

    std::string protocol = "line";   ///< 协议类型: "line" | "length" | "raw"
    std::string line_delimiter = "\n"; ///< LineProtocol 行分隔符 (仅 line 协议有效)

    /// LengthProtocol 配置 (仅 length 协议有效)
    unsigned int length_field_size = 2;  ///< 长度字段字节数 (1/2/4)
    bool         length_big_endian = false; ///< 长度字段字节序 (false=小端)
    unsigned int length_offset     = 0;      ///< 长度字段在帧中的偏移
    unsigned int max_frame_size    = 4096;   ///< 最大帧长度 (防止异常数据)
};

} // namespace serial
