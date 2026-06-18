#include "clickhouse_client.h"
#include <sstream>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

namespace fenyun {

struct ClickHouseClient::Impl {
#ifdef _WIN32
    SOCKET sock {INVALID_SOCKET};
    WSADATA wsa_data;
    bool wsa_init {false};
#else
    int sock {-1};
#endif

    bool send_http_request(const std::string& request, std::string& response) {
#ifdef _WIN32
        if (!wsa_init) {
            if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) return false;
            wsa_init = true;
        }
#endif
        std::string host_part = request;
        size_t pos = host_part.find("Host: ");
        std::string host;
        if (pos != std::string::npos) {
            size_t end = host_part.find("\r\n", pos);
            host = host_part.substr(pos + 6, end - pos - 6);
        }

#ifdef _WIN32
        if (sock == INVALID_SOCKET) {
#else
        if (sock < 0) {
#endif
            struct addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

#ifdef _WIN32
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock == INVALID_SOCKET) return false;
#else
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return false;
#endif

            std::string hostname = host;
            std::string port_str = "8123";
            size_t colon = hostname.find(':');
            if (colon != std::string::npos) {
                port_str = hostname.substr(colon + 1);
                hostname = hostname.substr(0, colon);
            }

            if (getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &res) != 0) return false;
            if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
                freeaddrinfo(res);
                return false;
            }
            freeaddrinfo(res);
        }

        int total = static_cast<int>(request.size());
        int sent = 0;
        while (sent < total) {
#ifdef _WIN32
            int n = send(sock, request.c_str() + sent, total - sent, 0);
#else
            int n = send(sock, request.c_str() + sent, total - sent, MSG_NOSIGNAL);
#endif
            if (n <= 0) return false;
            sent += n;
        }

        char buf[4096];
        response.clear();
        while (true) {
#ifdef _WIN32
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
#else
            int n = recv(sock, buf, sizeof(buf) - 1, MSG_NOSIGNAL);
#endif
            if (n <= 0) break;
            buf[n] = '\0';
            response += buf;
            if (response.find("\r\n0\r\n") != std::string::npos) break;
            if (response.size() > 1024 * 1024) break;
        }
        return true;
    }

    ~Impl() {
#ifdef _WIN32
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        if (wsa_init) WSACleanup();
#else
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
#endif
    }
};

ClickHouseClient::ClickHouseClient(const std::string& host, uint16_t port,
                                     const std::string& user, const std::string& password,
                                     const std::string& database)
    : impl_(std::make_unique<Impl>()),
      host_(host), port_(port), user_(user), password_(password), database_(database) {}

ClickHouseClient::~ClickHouseClient() = default;

bool ClickHouseClient::connect() {
    std::stringstream url_ss;
    url_ss << "http://" << host_ << ":" << std::to_string(8123) << "/?query=SELECT%201";

    std::string request =
        "GET /?query=SELECT%201 HTTP/1.1\r\n"
        "Host: " + host_ + ":8123\r\n"
        "Connection: keep-alive\r\n\r\n";

    std::string response;
    connected_ = impl_->send_http_request(request, response);
    return connected_;
}

void ClickHouseClient::disconnect() {
    connected_ = false;
}

bool ClickHouseClient::is_connected() const {
    return connected_;
}

std::string escape_for_sql(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '\'') result += "''";
        else result += c;
    }
    return result;
}

std::string escape_tsv(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '\t') result += "\\t";
        else if (c == '\n') result += "\\n";
        else result += c;
    }
    return result;
}

bool ClickHouseClient::insert_sensor_data(const SensorData& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream query;
    query << "INSERT INTO " << database_ << ".sensor_data FORMAT TSV\n";
    query << data.vehicle_id << "\t"
          << data.timestamp_ms << "\t"
          << data.roof_stress << "\t"
          << data.wheel_deformation << "\t"
          << data.rock_impact_force << "\t"
          << data.protection_thickness << "\t"
          << data.protection_material << "\t"
          << data.ambient_temp << "\t"
          << data.impact_location_x << "\t"
          << data.impact_location_y << "\t"
          << data.rock_mass << "\t"
          << data.rock_velocity << "\n";

    std::string payload = query.str();
    std::string request =
        "POST /?query=INSERT%20INTO%20" + database_ + ".sensor_data%20FORMAT%20TSV HTTP/1.1\r\n"
        "Host: " + host_ + ":8123\r\n"
        "Content-Type: text/tab-separated-values\r\n"
        "Content-Length: " + std::to_string(payload.size()) + "\r\n"
        "Connection: keep-alive\r\n\r\n" + payload;

    std::string response;
    return impl_->send_http_request(request, response);
}

bool ClickHouseClient::insert_sensor_batch(const std::vector<SensorData>& batch) {
    if (batch.empty()) return true;
    for (const auto& d : batch) {
        if (!insert_sensor_data(d)) return false;
    }
    return true;
}

bool ClickHouseClient::insert_simulation_result(const SimulationResult& r) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream def_field, stress_field;
    def_field << "[";
    for (size_t i = 0; i < r.deformation_field.size(); ++i) {
        if (i) def_field << ",";
        def_field << r.deformation_field[i];
    }
    def_field << "]";

    stress_field << "[";
    for (size_t i = 0; i < r.stress_field.size(); ++i) {
        if (i) stress_field << ",";
        stress_field << r.stress_field[i];
    }
    stress_field << "]";

    std::stringstream query;
    query << "INSERT INTO " << database_ << ".simulation_results VALUES ("
          << r.simulation_id << ","
          << r.vehicle_id << ","
          << r.timestamp_ms << ","
          << r.roof_max_deformation_mm << ","
          << r.roof_plastic_strain << ","
          << r.roof_von_mises_stress_mpa << ","
          << r.impact_energy_j << ","
          << r.absorbed_energy_j << ","
          << static_cast<int>(r.damage_level) << ","
          << r.penetration_depth_mm << ","
          << (r.is_penetrated ? 1 : 0) << ",'"
          << escape_for_sql(r.failure_mode) << "',"
          << def_field.str() << ","
          << stress_field.str() << ")";

    std::string sql = "INSERT INTO " + database_ + ".simulation_results FORMAT VALUES\n" + query.str() + "\n";
    std::string request =
        "POST /?query=INSERT%20INTO%20" + database_ + ".simulation_results%20FORMAT%20VALUES HTTP/1.1\r\n"
        "Host: " + host_ + ":8123\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(sql.size()) + "\r\n"
        "Connection: keep-alive\r\n\r\n" + sql;

    std::string response;
    return impl_->send_http_request(request, response);
}

bool ClickHouseClient::insert_alert_record(const AlertRecord& r) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream ss;
    ss << r.alert_id << "\t"
       << r.vehicle_id << "\t"
       << r.timestamp_ms << "\t"
       << escape_tsv(r.alert_type) << "\t"
       << static_cast<int>(r.alert_level) << "\t"
       << escape_tsv(r.alert_message) << "\t"
       << r.measured_value << "\t"
       << r.threshold_value << "\t"
       << 0 << "\t"
       << "\\N\t"
       << escape_tsv(r.mqtt_topic) << "\t"
       << escape_tsv(r.mqtt_message_id) << "\n";

    std::string payload = ss.str();
    std::string request =
        "POST /?query=INSERT%20INTO%20" + database_ + ".alert_records%20FORMAT%20TSV HTTP/1.1\r\n"
        "Host: " + host_ + ":8123\r\n"
        "Content-Type: text/tab-separated-values\r\n"
        "Content-Length: " + std::to_string(payload.size()) + "\r\n"
        "Connection: keep-alive\r\n\r\n" + payload;

    std::string response;
    return impl_->send_http_request(request, response);
}

bool ClickHouseClient::insert_protection_evaluation(const ProtectionEvaluation& e) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream ss;
    ss << e.eval_id << "\t"
       << e.vehicle_id << "\t"
       << e.timestamp_ms << "\t"
       << e.material_type << "\t"
       << e.material_thickness_mm << "\t"
       << e.energy_absorption_score << "\t"
       << e.structural_strength_score << "\t"
       << e.weight_factor_score << "\t"
       << e.cost_factor_score << "\t"
       << e.durability_score << "\t"
       << e.ahp_weight_score << "\t"
       << static_cast<int>(e.rank_position) << "\t"
       << (e.is_recommended ? 1 : 0) << "\t"
       << "{}\n";

    std::string payload = ss.str();
    std::string request =
        "POST /?query=INSERT%20INTO%20" + database_ + ".protection_evaluation%20FORMAT%20TSV HTTP/1.1\r\n"
        "Host: " + host_ + ":8123\r\n"
        "Content-Type: text/tab-separated-values\r\n"
        "Content-Length: " + std::to_string(payload.size()) + "\r\n"
        "Connection: keep-alive\r\n\r\n" + payload;

    std::string response;
    return impl_->send_http_request(request, response);
}

bool ClickHouseClient::insert_protection_evaluation_batch(const std::vector<ProtectionEvaluation>& batch) {
    for (const auto& e : batch) {
        if (!insert_protection_evaluation(e)) return false;
    }
    return true;
}

std::vector<SensorData> ClickHouseClient::query_sensor_data(uint32_t vehicle_id,
                                                             int64_t start_ts_ms,
                                                             int64_t end_ts_ms,
                                                             int limit) {
    std::vector<SensorData> results;
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream query;
    query << "SELECT vehicle_id, timestamp, roof_stress, wheel_deformation, rock_impact_force, "
          << "protection_thickness, protection_material, ambient_temp, impact_location_x, "
          << "impact_location_y, rock_mass, rock_velocity FROM " << database_ << ".sensor_data "
          << "WHERE vehicle_id=" << vehicle_id
          << " AND timestamp>=" << start_ts_ms
          << " AND timestamp<=" << end_ts_ms
          << " ORDER BY timestamp DESC LIMIT " << limit
          << " FORMAT TSV";

    std::string sql = query.str();
    std::string request =
        "POST / HTTP/1.1\r\n"
        "Host: " + host_ + ":8123\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(sql.size()) + "\r\n"
        "Connection: keep-alive\r\n\r\n" + sql;

    std::string response;
    if (!impl_->send_http_request(request, response)) return results;

    size_t body_start = response.find("\r\n\r\n");
    if (body_start == std::string::npos) return results;
    std::string body = response.substr(body_start + 4);

    std::istringstream iss(body);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        SensorData d{};
        std::istringstream ls(line);
        std::string token;
        int idx = 0;
        while (std::getline(ls, token, '\t')) {
            switch (idx) {
                case 0: d.vehicle_id = static_cast<uint32_t>(std::stoul(token)); break;
                case 1: d.timestamp_ms = std::stoll(token); break;
                case 2: d.roof_stress = std::stod(token); break;
                case 3: d.wheel_deformation = std::stod(token); break;
                case 4: d.rock_impact_force = std::stod(token); break;
                case 5: d.protection_thickness = std::stod(token); break;
                case 6: d.protection_material = token; break;
                case 7: d.ambient_temp = std::stod(token); break;
                case 8: d.impact_location_x = std::stod(token); break;
                case 9: d.impact_location_y = std::stod(token); break;
                case 10: d.rock_mass = std::stod(token); break;
                case 11: d.rock_velocity = std::stod(token); break;
            }
            idx++;
        }
        if (idx >= 12) results.push_back(d);
    }
    return results;
}

std::vector<SimulationResult> ClickHouseClient::query_simulation_results(uint32_t vehicle_id, int limit) {
    return {};
}

std::vector<AlertRecord> ClickHouseClient::query_alerts(uint32_t vehicle_id, int limit) {
    return {};
}

std::vector<ProtectionEvaluation> ClickHouseClient::query_evaluations(uint32_t vehicle_id, int limit) {
    return {};
}

}
