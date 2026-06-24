#include "server/http/server.hpp"
#include "rtsp/rtspmgr.h"
#include "onvif/onvif_manager.h"

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

static std::unique_ptr<http::Server> g_http_server;
static std::string g_video_file;

static void signal_handler(int /*signum*/)
{
    std::cout << "\n[Main] Caught signal, shutting down..." << std::endl;
    if (g_http_server)
        g_http_server->stop();
    rtsp::RtspManager::Instance().Stop();
    onvif::OnvifManager::Instance().Stop();
}

static void print_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --video <file>       Raw H.264 video file to stream via RTSP\n"
              << "  --onvif-ip <ip>     Device IP for ONVIF services (default: 127.0.0.1)\n"
              << "  --onvif-http-port <p> HTTP port for ONVIF SOAP endpoints (default: 8080)\n"
              << "  --onvif-rtsp-port <p> RTSP port for ONVIF stream URI (default: 8554)\n"
              << "  --help               Show this help\n"
              << std::endl;
}

int main(int argc, char* argv[])
{
    onvif::ServiceConfig onvif_cfg;
    onvif_cfg.device_ip  = "127.0.0.1";
    onvif_cfg.http_port = 8080;
    onvif_cfg.rtsp_port = 8554;
    onvif_cfg.rtsp_path = "live";

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--video" && i + 1 < argc) {
            g_video_file = argv[++i];
        } else if (arg == "--onvif-ip" && i + 1 < argc) {
            onvif_cfg.device_ip = argv[++i];
        } else if (arg == "--onvif-http-port" && i + 1 < argc) {
            onvif_cfg.http_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--onvif-rtsp-port" && i + 1 < argc) {
            onvif_cfg.rtsp_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    if (g_video_file.empty()) {
        g_video_file = "test.h264";
    }

    // ------------------------------------------------------------
    // 0. Configure and register ONVIF module
    // ------------------------------------------------------------
    onvif::OnvifManager::Instance().SetConfig(onvif_cfg);
    std::cout << "[Main] ONVIF config: device_ip=" << onvif_cfg.device_ip
              << " http_port=" << onvif_cfg.http_port
              << " rtsp_port=" << onvif_cfg.rtsp_port << std::endl;

    // ------------------------------------------------------------
    // 1. Start RTSP server on port 8554
    // ------------------------------------------------------------
    std::cout << "[Main] Starting RTSP server on rtsp://localhost:8554" << std::endl;
    auto& rtsp_mgr = rtsp::RtspManager::Instance();

    if (!g_video_file.empty()) {
        rtsp_mgr.SetVideoFile(g_video_file);
        std::cout << "[Main] Video file: " << g_video_file << std::endl;
    }

    rtsp_mgr.Start(8554);

    // ------------------------------------------------------------
    // 2. Create HTTP server on port 8080
    // ------------------------------------------------------------
    g_http_server = std::make_unique<http::Server>("0.0.0.0", 8080);

    // ------------------------------------------------------------
    // 3. Register routes
    // ------------------------------------------------------------
    auto &router = g_http_server->router();

    // Register ONVIF SOAP endpoints
    onvif::OnvifManager::Instance().RegisterRoutes(router);

    router.get("/",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok(
                "<html>"
                "<head><title>ai-camera</title></head>"
                "<body>"
                "<h1>Welcome to ai-camera!</h1>"
                "<p>RTSP stream available at: <b>rtsp://localhost:8554/live</b></p>"
                "</body>"
                "</html>",
                "text/html");
        });

    router.get("/hello",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok("Hello, ASIO HTTP Server!\n");
        });

    router.get("/json",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok(
                "{ \"message\": \"Hello from ai-camera\", \"status\": \"ok\" }\n",
                "application/json");
        });

    router.post("/echo",
        [](const http::Request &req) -> http::Response
        {
            return http::Response::ok(req.body);
        });

    // ------------------------------------------------------------
    // 4. Register signal handlers for graceful shutdown
    // ------------------------------------------------------------
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ------------------------------------------------------------
    // 4.5 Start ONVIF services (WS-Discovery)
    // ------------------------------------------------------------
    if (!onvif::OnvifManager::Instance().Start()) {
        std::cerr << "[Main] Warning: ONVIF start failed, continuing..." << std::endl;
    }

    // ------------------------------------------------------------
    // 5. Start the HTTP server (blocks)
    // ------------------------------------------------------------
    std::cout << "[Main] Starting HTTP server on http://localhost:8080" << std::endl;
    g_http_server->run();

    std::cout << "[Main] Server stopped. Goodbye!" << std::endl;
    return 0;
}