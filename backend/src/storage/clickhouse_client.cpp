#include "storage/clickhouse_client.h"

#include <iostream>
#include <sstream>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using ssize_t = SSIZE_T;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace fenyun {

ClickHouseClient::ClickHouseClient(std::shared_ptr<ConfigLoader> config)
    : config_(std::move(config)) {
    if (config_) {
        const auto& ch = config_->system_config().clickhouse;
        host_ = ch.host;
        port_ = ch.http_port;
        database_ = ch.database;
        user_ = ch.user;
        password_ = ch.password;
    }
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

ClickHouseClient::~ClickHouseClient() {
    disconnect();
#if defined(_WIN32)
    WSACleanup();
#endif
}

bool ClickHouseClient::connect() {
    if (connected_) return true;

#if defined(_WIN32)
    sock_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
#else
    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (sock_fd_ < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

    if (::connect(sock_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#if defined(_WIN32)
        closesocket(sock_fd_);
#else
        close(sock_fd_);
#endif
        sock_fd_ = -1;
        return false;
    }

    connected_ = true;
    std::cout << "[ClickHouse] Connected to " << host_ << ":" << port_ << std::endl;
    return true;
}

void ClickHouseClient::disconnect() {
    if (!connected_) return;
#if defined(_WIN32)
    closesocket(sock_fd_);
#else
    close(sock_fd_);
#endif
    sock_fd_ = -1;
    connected_ = false;
}

bool ClickHouseClient::send_http_request(const std::string& method,
                                          const std::string& path,
                                          const std::string& body,
                                          std::string& response) {
    if (!connected_ && !connect()) return false;

    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n";
    request << "Host: " << host_ << ":" << port_ << "\r\n";
    request << "Content-Type: text/plain; charset=UTF-8\r\n";
    request << "Content-Length: " << body.size() << "\r\n";
    request << "Connection: keep-alive\r\n";
    request << "X-ClickHouse-Database: " << database_ << "\r\n";
    if (!user_.empty()) {
        request << "X-ClickHouse-User: " << user_ << "\r\n";
        if (!password_.empty()) {
            request << "X-ClickHouse-Key: " << password_ << "\r\n";
        }
    }
    request << "\r\n";
    request << body;

    std::string req_str = request.str();
    ssize_t sent = 0;
    const char* p = req_str.c_str();
    size_t remaining = req_str.size();
    while (remaining > 0) {
#if defined(_WIN32)
        int n = send(sock_fd_, p, static_cast<int>(remaining), 0);
#else
        ssize_t n = send(sock_fd_, p, remaining, 0);
#endif
        if (n <= 0) {
            connected_ = false;
            return false;
        }
        sent += n;
        p += n;
        remaining -= n;
    }

    char buffer[8192];
    response.clear();
    bool header_end = false;
    size_t content_length = 0;
    size_t header_size = 0;

    int total_recv = 0;
    for (int i = 0; i < 100; ++i) {
#if defined(_WIN32)
        int n = recv(sock_fd_, buffer, sizeof(buffer) - 1, 0);
#else
        ssize_t n = recv(sock_fd_, buffer, sizeof(buffer) - 1, 0);
#endif
        if (n <= 0) break;
        buffer[n] = '\0';
        response.append(buffer, n);
        total_recv += n;

        if (!header_end) {
            size_t pos = response.find("\r\n\r\n");
            if (pos != std::string::npos) {
                header_end = true;
                header_size = pos + 4;
                std::string header = response.substr(0, pos);
                size_t cl_pos = header.find("Content-Length:");
                if (cl_pos != std::string::npos) {
                    content_length = std::stoul(header.substr(cl_pos + 16));
                }
                if (content_length == 0 && header.find("Transfer-Encoding: chunked") == std::string::npos) {
                    break;
                }
            }
        }
        if (header_end && content_length > 0) {
            if (response.size() - header_size >= content_length) break;
        }
    }

    return true;
}

std::string ClickHouseClient::build_insert_sql(const std::string& table,
                                                const std::string& format) {
    std::ostringstream sql;
    sql << "INSERT INTO " << database_ << "." << table << " FORMAT " << format;
    return sql.str();
}

std::string ClickHouseClient::escape_for_sql(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '\'': result += "\\'"; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

bool ClickHouseClient::insert_sensor_data(const SensorData& data) {
    std::ostringstream body;
    body << build_insert_sql("sensor_data", "Values") << "\n";
    body << "("
         << data.vehicle_id << ", "
         << data.timestamp_ms << ", "
         << data.roof_stress << ", "
         << data.wheel_deformation << ", "
         << data.rock_impact_force << ", "
         << data.protection_thickness << ", "
         << "'" << escape_for_sql(data.protection_material) << "', "
         << data.ambient_temp << ", "
         << data.impact_location_x << ", "
         << data.impact_location_y << ", "
         << data.rock_mass << ", "
         << data.rock_velocity
         << ")";

    std::string response;
    return send_http_request("POST", "/", body.str(), response);
}

bool ClickHouseClient::insert_sensor_batch(const std::vector<SensorData>& batch) {
    if (batch.empty()) return true;

    std::ostringstream body;
    body << build_insert_sql("sensor_data", "TSV") << "\n";
    for (const auto& d : batch) {
        body << d.vehicle_id << "\t"
             << d.timestamp_ms << "\t"
             << d.roof_stress << "\t"
             << d.wheel_deformation << "\t"
             << d.rock_impact_force << "\t"
             << d.protection_thickness << "\t"
             << d.protection_material << "\t"
             << d.ambient_temp << "\t"
             << d.impact_location_x << "\t"
             << d.impact_location_y << "\t"
             << d.rock_mass << "\t"
             << d.rock_velocity << "\n";
    }

    std::string response;
    return send_http_request("POST", "/", body.str(), response);
}

bool ClickHouseClient::insert_simulation_result(const SimulationResult& result) {
    std::ostringstream body;
    body << build_insert_sql("simulation_results", "Values") << "\n";
    body << "("
         << result.simulation_id << ", "
         << result.vehicle_id << ", "
         << result.timestamp_ms << ", "
         << result.roof_max_deformation_mm << ", "
         << result.roof_plastic_strain << ", "
         << result.roof_von_mises_stress_mpa << ", "
         << result.impact_energy_j << ", "
         << result.absorbed_energy_j << ", "
         << static_cast<int>(result.damage_level) << ", "
         << result.penetration_depth_mm << ", "
         << (result.is_penetrated ? 1 : 0) << ", "
         << "'" << escape_for_sql(result.failure_mode) << "', "
         << "'" << escape_for_sql(result.protection_material) << "', "
         << result.strain_rate << ", "
         << result.dynamic_yield_strength_mpa << ", "
         << result.temperature_K
         << ")";

    std::string response;
    return send_http_request("POST", "/", body.str(), response);
}

bool ClickHouseClient::insert_alert_record(const AlertRecord& alert) {
    std::ostringstream body;
    body << build_insert_sql("alert_records", "Values") << "\n";
    body << "("
         << alert.alert_id << ", "
         << alert.vehicle_id << ", "
         << alert.timestamp_ms << ", "
         << "'" << escape_for_sql(alert.alert_type) << "', "
         << static_cast<int>(alert.alert_level) << ", "
         << "'" << escape_for_sql(alert.alert_message) << "', "
         << alert.measured_value << ", "
         << alert.threshold_value << ", "
         << "'" << escape_for_sql(alert.mqtt_topic) << "', "
         << "'" << escape_for_sql(alert.mqtt_message_id) << "'"
         << ")";

    std::string response;
    return send_http_request("POST", "/", body.str(), response);
}

bool ClickHouseClient::insert_evaluation(const ProtectionEvaluation& eval) {
    std::ostringstream body;
    body << build_insert_sql("protection_evaluation", "Values") << "\n";
    body << "("
         << eval.eval_id << ", "
         << eval.vehicle_id << ", "
         << eval.timestamp_ms << ", "
         << "'" << escape_for_sql(eval.material_type) << "', "
         << eval.material_thickness_mm << ", "
         << eval.energy_absorption_score << ", "
         << eval.structural_strength_score << ", "
         << eval.weight_factor_score << ", "
         << eval.cost_factor_score << ", "
         << eval.durability_score << ", "
         << eval.ahp_weight_score << ", "
         << static_cast<int>(eval.rank_position) << ", "
         << (eval.is_recommended ? 1 : 0)
         << ")";

    std::string response;
    return send_http_request("POST", "/", body.str(), response);
}

std::vector<SensorData> ClickHouseClient::query_sensor_history(uint32_t vehicle_id,
                                                                 int64_t start_ms,
                                                                 int64_t end_ms,
                                                                 int limit) {
    std::ostringstream query;
    query << "SELECT vehicle_id, timestamp_ms, roof_stress, wheel_deformation, "
          << "rock_impact_force, protection_thickness, protection_material, "
          << "ambient_temp, impact_location_x, impact_location_y, rock_mass, rock_velocity "
          << "FROM " << database_ << ".sensor_data "
          << "WHERE vehicle_id = " << vehicle_id << " "
          << "AND timestamp_ms >= " << start_ms << " "
          << "AND timestamp_ms <= " << end_ms << " "
          << "ORDER BY timestamp_ms DESC LIMIT " << limit << " "
          << "FORMAT TSV";

    std::string response;
    std::vector<SensorData> result;

    if (!send_http_request("POST", "/", query.str(), response)) return result;

    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) return result;

    std::string body = response.substr(header_end + 4);
    std::istringstream iss(body);
    std::string line;

    while (std::getline(iss, line) && !line.empty()) {
        SensorData d{};
        size_t pos = 0;
        int col = 0;
        for (int i = 0; i < 12; ++i) {
            size_t next = line.find('\t', pos);
            std::string val = line.substr(pos, next - pos);
            switch (col++) {
                case 0: d.vehicle_id = static_cast<uint32_t>(std::stoul(val)); break;
                case 1: d.timestamp_ms = std::stoll(val); break;
                case 2: d.roof_stress = std::stod(val); break;
                case 3: d.wheel_deformation = std::stod(val); break;
                case 4: d.rock_impact_force = std::stod(val); break;
                case 5: d.protection_thickness = std::stod(val); break;
                case 6: d.protection_material = val; break;
                case 7: d.ambient_temp = std::stod(val); break;
                case 8: d.impact_location_x = std::stod(val); break;
                case 9: d.impact_location_y = std::stod(val); break;
                case 10: d.rock_mass = std::stod(val); break;
                case 11: d.rock_velocity = std::stod(val); break;
            }
            if (next == std::string::npos) break;
            pos = next + 1;
        }
        result.push_back(d);
    }

    return result;
}

std::vector<SimulationResult> ClickHouseClient::query_simulation_results(uint32_t /*vehicle_id*/, int /*limit*/) {
    return {};
}

std::vector<AlertRecord> ClickHouseClient::query_alerts(uint32_t /*vehicle_id*/, int /*limit*/) {
    return {};
}

std::vector<ProtectionEvaluation> ClickHouseClient::query_evaluations(uint32_t /*vehicle_id*/, int /*limit*/) {
    return {};
}

}
