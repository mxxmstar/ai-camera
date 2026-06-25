#pragma once

namespace http {
    class Router;
}

namespace serial {

/// @brief 注册串口模块 HTTP API 路由
///
/// 在 main.cpp 中调用此函数将串口管理接口暴露为 HTTP API。
///
/// 注册的路由：
///   - GET    /serial/ports          获取所有串口状态
///   - GET    /serial/ports/{name}   获取指定串口状态
///   - POST   /serial/ports          打开新串口（JSON body: {device, baud_rate, ...}）
///   - PUT    /serial/ports/{name}   更新串口配置
///   - DELETE /serial/ports/{name}   关闭指定串口
///   - POST   /serial/ports/{name}/send  向指定串口发送数据（JSON body: {data}）
///
/// @param router HTTP Router 实例（由 HttpServer 提供）
void RegisterSerialRoutes(http::Router& router);

} // namespace serial
