#include "fenyun_application.h"
#include "fenyun_http_server.h"

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <string>

static std::atomic<bool> g_running {true};
static fenyun::FenyunHttpServer* g_http_server = nullptr;
static fenyun::FenyunApplication* g_app = nullptr;

void signal_handler(int sig) {
    std::cout << "\n[Signal] Received signal " << sig << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    uint16_t http_port = 8080;
    std::string config_dir = "./config";
    std::string ch_host = "127.0.0.1";
    std::string mqtt_broker = "tcp://127.0.0.1:1883";
    double def_threshold = 15.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--http-port" && i + 1 < argc) {
            http_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--ch-host" && i + 1 < argc) {
            ch_host = argv[++i];
        } else if (arg == "--mqtt-broker" && i + 1 < argc) {
            mqtt_broker = argv[++i];
        } else if (arg == "--def-threshold" && i + 1 < argc) {
            def_threshold = std::stod(argv[++i]);
        } else if (arg == "--config-dir" && i + 1 < argc) {
            config_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --http-port PORT        HTTP server port (default: 8080)\n"
                      << "  --ch-host HOST          ClickHouse host (default: 127.0.0.1)\n"
                      << "  --mqtt-broker URL       MQTT broker URL (default: tcp://127.0.0.1:1883)\n"
                      << "  --def-threshold MM      Deformation alert threshold (default: 15.0)\n"
                      << "  --config-dir DIR        Config JSON directory (default: ./config)\n"
                      << "  -h, --help              Show this help\n";
            return 0;
        }
    }

    std::cout << "================================================" << std::endl;
    std::cout << "  轒辒车结构防护仿真与滚石冲击分析系统 后端服务" << std::endl;
    std::cout << "  模块化架构: DTU -> ImpactSim -> Optimizer / AlarmMQTT" << std::endl;
    std::cout << "  队列通信: LockFreeQueue (Boost.Lockfree compatible)" << std::endl;
    std::cout << "================================================" << std::endl;
    std::cout << "HTTP Port:            " << http_port << std::endl;
    std::cout << "Config Dir:           " << config_dir << std::endl;
    std::cout << "ClickHouse Host:      " << ch_host << std::endl;
    std::cout << "MQTT Broker:          " << mqtt_broker << std::endl;
    std::cout << "Deformation Threshold:" << def_threshold << " mm" << std::endl;
    std::cout << "================================================" << std::endl;

    auto app = std::make_unique<fenyun::FenyunApplication>();
    g_app = app.get();

    if (!app->load_config(config_dir)) {
        std::cerr << "[Main] Config load failed, exiting..." << std::endl;
        return 1;
    }

    if (!app->initialize()) {
        std::cerr << "[Main] Application initialization failed" << std::endl;
        return 1;
    }

    app->start();

    auto http_server = std::make_unique<fenyun::FenyunHttpServer>(std::move(app));
    g_http_server = http_server.get();

    if (!http_server->start(http_port)) {
        std::cerr << "[Main] HTTP server start failed" << std::endl;
        g_app->stop();
        return 1;
    }

    std::cout << "System started. Press Ctrl+C to stop." << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    http_server->stop();
    if (g_app) g_app->stop();

    std::cout << "System stopped successfully." << std::endl;
    return 0;
}
