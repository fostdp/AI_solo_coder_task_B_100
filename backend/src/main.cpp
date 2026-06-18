#include "fenyun_server.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

static std::atomic<bool> g_running {true};

void signal_handler(int sig) {
    std::cout << "\n[Signal] Received signal " << sig << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fenyun::ServerConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--http-port" && i + 1 < argc) {
            config.http_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--ch-host" && i + 1 < argc) {
            config.clickhouse_host = argv[++i];
        } else if (arg == "--ch-port" && i + 1 < argc) {
            config.clickhouse_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--mqtt-broker" && i + 1 < argc) {
            config.mqtt_broker = argv[++i];
        } else if (arg == "--def-threshold" && i + 1 < argc) {
            config.deformation_threshold_mm = std::stod(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --http-port PORT        HTTP server port (default: 8080)\n"
                      << "  --ch-host HOST          ClickHouse host (default: 127.0.0.1)\n"
                      << "  --ch-port PORT          ClickHouse native port (default: 9000)\n"
                      << "  --mqtt-broker URL       MQTT broker URL (default: tcp://127.0.0.1:1883)\n"
                      << "  --def-threshold MM      Deformation alert threshold (default: 15.0)\n"
                      << "  -h, --help              Show this help\n";
            return 0;
        }
    }

    std::cout << "================================================" << std::endl;
    std::cout << "  轒辒车结构防护仿真与滚石冲击分析系统 后端服务" << std::endl;
    std::cout << "================================================" << std::endl;
    std::cout << "HTTP Port:            " << config.http_port << std::endl;
    std::cout << "ClickHouse Host:      " << config.clickhouse_host << ":" << config.clickhouse_port << std::endl;
    std::cout << "MQTT Broker:          " << config.mqtt_broker << std::endl;
    std::cout << "Deformation Threshold:" << config.deformation_threshold_mm << " mm" << std::endl;
    std::cout << "================================================" << std::endl;

    fenyun::FenYunServer server(config);
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Server started. Press Ctrl+C to stop." << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    server.stop();
    std::cout << "Server stopped successfully." << std::endl;
    return 0;
}
