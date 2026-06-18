#include "fenyun_server.h"
#include "json.h"
#include <sstream>
#include <cstring>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#define INVALID_SOCK INVALID_SOCKET
#define SOCKET_ERROR_CODE SOCKET_ERROR
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCK (-1)
#define SOCKET_ERROR_CODE (-1)
#define CLOSE_SOCKET close
#endif

namespace fenyun {

struct FenYunServer::HttpImpl {
    socket_t server_fd {INVALID_SOCK};
    std::atomic<bool> running {false};
#ifdef _WIN32
    WSADATA wsa_data;
    bool wsa_init {false};
#endif
};

FenYunServer::FenYunServer(const ServerConfig& config)
    : config_(config),
      http_impl_(std::make_unique<HttpImpl>()) {

    db_client_ = std::make_unique<ClickHouseClient>(
        config.clickhouse_host, config.clickhouse_port,
        config.clickhouse_user, config.clickhouse_password,
        config.clickhouse_database
    );

    mqtt_manager_ = std::make_unique<MQTTAlertManager>(
        config.mqtt_broker, config.mqtt_client_id,
        config.mqtt_username, config.mqtt_password
    );

    mqtt_manager_->set_deformation_threshold(config.deformation_threshold_mm);
    mqtt_manager_->set_penetration_threshold(config.penetration_threshold_ratio);

    simulator_ = std::make_unique<StructuralSimulation>();
    simulator_->set_deformation_threshold(config.deformation_threshold_mm);
    simulator_->set_penetration_threshold(config.penetration_threshold_ratio);

    ahp_evaluator_ = std::make_unique<AHPEvaluator>();
}

FenYunServer::~FenYunServer() {
    stop();
}

static std::string http_response(int code, const std::string& content_type, const std::string& body) {
    std::string status_text = (code == 200) ? "OK" : (code == 201) ? "Created" : (code == 400) ? "Bad Request" :
                              (code == 404) ? "Not Found" : "Internal Server Error";
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << status_text << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        << "Access-Control-Allow-Headers: Content-Type\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    return oss.str();
}

static std::string urldecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out += ' ';
        } else if (s[i] == '%' && i + 2 < s.size()) {
            int val = 0;
            std::istringstream iss(s.substr(i + 1, 2));
            if (iss >> std::hex >> val) {
                out += static_cast<char>(val);
                i += 2;
            } else {
                out += s[i];
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static bool parse_json_body(const std::string& body, json::Value& root) {
    root = json::parse(body);
    return !root.isNull();
}

void FenYunServer::process_sensor_data(const SensorData& data) {
    {
        std::lock_guard<std::mutex> lock(sim_queue_mutex_);
        sim_queue_.push(data);
    }
    sim_queue_cv_.notify_one();
}

void FenYunServer::simulation_worker() {
    while (running_) {
        SensorData data;
        {
            std::unique_lock<std::mutex> lock(sim_queue_mutex_);
            sim_queue_cv_.wait(lock, [this] { return !sim_queue_.empty() || !running_; });
            if (!running_ && sim_queue_.empty()) return;
            data = sim_queue_.front();
            sim_queue_.pop();
        }

        SimulationResult sim = simulator_->run_simulation(data);
        sim.simulation_id = simulation_id_counter_.fetch_add(1);

        db_client_->insert_simulation_result(sim);

        {
            std::lock_guard<std::mutex> lock(alert_queue_mutex_);
            alert_queue_.push({data, sim, alert_id_counter_.fetch_add(1)});
        }
        alert_queue_cv_.notify_one();
    }
}

void FenYunServer::alert_worker() {
    while (running_) {
        AlertTask task;
        {
            std::unique_lock<std::mutex> lock(alert_queue_mutex_);
            alert_queue_cv_.wait(lock, [this] { return !alert_queue_.empty() || !running_; });
            if (!running_ && alert_queue_.empty()) return;
            task = alert_queue_.front();
            alert_queue_.pop();
        }

        auto alerts = mqtt_manager_->check_and_alert(task.sensor, task.sim, task.alert_id);
        for (const auto& alert : alerts) {
            db_client_->insert_alert_record(alert);
        }
    }
}

void FenYunServer::http_server_worker() {
#ifdef _WIN32
    if (!http_impl_->wsa_init) {
        if (WSAStartup(MAKEWORD(2, 2), &http_impl_->wsa_data) != 0) return;
        http_impl_->wsa_init = true;
    }
#endif

    http_impl_->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (http_impl_->server_fd == INVALID_SOCK) return;

    int opt = 1;
#ifdef _WIN32
    setsockopt(http_impl_->server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(http_impl_->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.http_port);
    inet_pton(AF_INET, config_.http_host.c_str(), &addr.sin_addr);

    if (bind(http_impl_->server_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_CODE) {
        CLOSE_SOCKET(http_impl_->server_fd);
        return;
    }

    if (listen(http_impl_->server_fd, 64) == SOCKET_ERROR_CODE) {
        CLOSE_SOCKET(http_impl_->server_fd);
        return;
    }

    http_impl_->running = true;
    std::cout << "[HTTP] Server listening on " << config_.http_host << ":" << config_.http_port << std::endl;

    while (running_ && http_impl_->running) {
        struct sockaddr_in client_addr;
#ifdef _WIN32
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif
        socket_t client_fd = accept(http_impl_->server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd == INVALID_SOCK) continue;

        char buffer[8192];
        std::string request_str;
        while (true) {
#ifdef _WIN32
            int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
#else
            int n = recv(client_fd, buffer, sizeof(buffer) - 1, MSG_NOSIGNAL);
#endif
            if (n <= 0) break;
            buffer[n] = '\0';
            request_str += buffer;
            if (request_str.find("\r\n\r\n") != std::string::npos) break;
            if (request_str.size() > 65536) break;
        }

        std::string response_body;
        int response_code = 200;
        std::string content_type = "application/json";

        size_t method_end = request_str.find(' ');
        size_t path_end = request_str.find(' ', method_end + 1);
        if (method_end == std::string::npos || path_end == std::string::npos) {
            response_body = "{\"error\":\"Invalid request\"}";
            response_code = 400;
        } else {
            std::string method = request_str.substr(0, method_end);
            std::string path = request_str.substr(method_end + 1, path_end - method_end - 1);

            size_t qpos = path.find('?');
            std::string query;
            if (qpos != std::string::npos) {
                query = path.substr(qpos + 1);
                path = path.substr(0, qpos);
            }

            size_t body_start = request_str.find("\r\n\r\n");
            std::string body = (body_start != std::string::npos) ? request_str.substr(body_start + 4) : "";

            if (method == "OPTIONS") {
                response_code = 200;
                response_body = "{}";
            } else if (path == "/api/health" && method == "GET") {
                json::Value result(json::Object{});
                result["status"] = "ok";
                result["timestamp"] = static_cast<int64_t>(current_timestamp_ms());
                result["deformation_threshold"] = config_.deformation_threshold_mm;
                response_body = result.dump();
            } else if (path == "/api/sensor" && method == "POST") {
                json::Value root;
                if (parse_json_body(body, root)) {
                    SensorData data{};
                    data.vehicle_id = static_cast<uint32_t>(root.has("vehicle_id") ? root["vehicle_id"].asUInt() : 1);
                    data.timestamp_ms = root.has("timestamp_ms") ? root["timestamp_ms"].asInt64() : current_timestamp_ms();
                    data.roof_stress = root.has("roof_stress") ? root["roof_stress"].asDouble() : 0.0;
                    data.wheel_deformation = root.has("wheel_deformation") ? root["wheel_deformation"].asDouble() : 0.0;
                    data.rock_impact_force = root.has("rock_impact_force") ? root["rock_impact_force"].asDouble() : 0.0;
                    data.protection_thickness = root.has("protection_thickness") ? root["protection_thickness"].asDouble() : 80.0;
                    data.protection_material = root.has("protection_material") ? root["protection_material"].asString() : "wood";
                    data.ambient_temp = root.has("ambient_temp") ? root["ambient_temp"].asDouble() : 20.0;
                    data.impact_location_x = root.has("impact_location_x") ? root["impact_location_x"].asDouble() : 3.0;
                    data.impact_location_y = root.has("impact_location_y") ? root["impact_location_y"].asDouble() : 1.25;
                    data.rock_mass = root.has("rock_mass") ? root["rock_mass"].asDouble() : 50.0;
                    data.rock_velocity = root.has("rock_velocity") ? root["rock_velocity"].asDouble() : 15.0;

                    db_client_->insert_sensor_data(data);
                    process_sensor_data(data);

                    json::Value result(json::Object{});
                    result["status"] = "received";
                    result["vehicle_id"] = static_cast<int64_t>(data.vehicle_id);
                    result["timestamp_ms"] = data.timestamp_ms;
                    response_body = result.dump();
                    response_code = 201;
                } else {
                    response_body = "{\"error\":\"Invalid JSON\"}";
                    response_code = 400;
                }
            } else if (path == "/api/sensor/batch" && method == "POST") {
                json::Value root;
                int count = 0;
                if (parse_json_body(body, root) && root.isArray()) {
                    for (size_t i = 0; i < root.size(); ++i) {
                        const auto& item = root[i];
                        SensorData data{};
                        data.vehicle_id = static_cast<uint32_t>(item.has("vehicle_id") ? item["vehicle_id"].asUInt() : 1);
                        data.timestamp_ms = item.has("timestamp_ms") ? item["timestamp_ms"].asInt64() : current_timestamp_ms();
                        data.roof_stress = item.has("roof_stress") ? item["roof_stress"].asDouble() : 0.0;
                        data.wheel_deformation = item.has("wheel_deformation") ? item["wheel_deformation"].asDouble() : 0.0;
                        data.rock_impact_force = item.has("rock_impact_force") ? item["rock_impact_force"].asDouble() : 0.0;
                        data.protection_thickness = item.has("protection_thickness") ? item["protection_thickness"].asDouble() : 80.0;
                        data.protection_material = item.has("protection_material") ? item["protection_material"].asString() : "wood";
                        data.ambient_temp = item.has("ambient_temp") ? item["ambient_temp"].asDouble() : 20.0;
                        data.impact_location_x = item.has("impact_location_x") ? item["impact_location_x"].asDouble() : 3.0;
                        data.impact_location_y = item.has("impact_location_y") ? item["impact_location_y"].asDouble() : 1.25;
                        data.rock_mass = item.has("rock_mass") ? item["rock_mass"].asDouble() : 50.0;
                        data.rock_velocity = item.has("rock_velocity") ? item["rock_velocity"].asDouble() : 15.0;

                        db_client_->insert_sensor_data(data);
                        process_sensor_data(data);
                        count++;
                    }
                }
                json::Value result(json::Object{});
                result["status"] = "received";
                result["count"] = count;
                response_body = result.dump();
                response_code = 201;
            } else if (path == "/api/sensor/history" && method == "GET") {
                uint32_t vid = 1;
                int64_t start = current_timestamp_ms() - 3600000;
                int64_t end = current_timestamp_ms();
                int limit = 1000;

                size_t pos = 0;
                while (pos < query.size()) {
                    size_t amp = query.find('&', pos);
                    std::string kv = (amp == std::string::npos) ? query.substr(pos) : query.substr(pos, amp - pos);
                    size_t eq = kv.find('=');
                    if (eq != std::string::npos) {
                        std::string k = kv.substr(0, eq);
                        std::string v = urldecode(kv.substr(eq + 1));
                        try {
                            if (k == "vehicle_id") vid = static_cast<uint32_t>(std::stoul(v));
                            else if (k == "start") start = std::stoll(v);
                            else if (k == "end") end = std::stoll(v);
                            else if (k == "limit") limit = std::stoi(v);
                        } catch (...) {}
                    }
                    if (amp == std::string::npos) break;
                    pos = amp + 1;
                }

                auto data = db_client_->query_sensor_data(vid, start, end, limit);
                json::Value arr(json::Array{});
                for (const auto& d : data) {
                    json::Value item(json::Object{});
                    item["vehicle_id"] = static_cast<int64_t>(d.vehicle_id);
                    item["timestamp_ms"] = d.timestamp_ms;
                    item["roof_stress"] = d.roof_stress;
                    item["wheel_deformation"] = d.wheel_deformation;
                    item["rock_impact_force"] = d.rock_impact_force;
                    item["protection_thickness"] = d.protection_thickness;
                    item["protection_material"] = d.protection_material;
                    item["ambient_temp"] = d.ambient_temp;
                    item["impact_location_x"] = d.impact_location_x;
                    item["impact_location_y"] = d.impact_location_y;
                    item["rock_mass"] = d.rock_mass;
                    item["rock_velocity"] = d.rock_velocity;
                    arr.append(item);
                }
                json::Value result(json::Object{});
                result["data"] = arr;
                result["count"] = static_cast<int64_t>(arr.size());
                response_body = result.dump();
            } else if (path == "/api/simulation/latest" && method == "GET") {
                uint32_t vid = 1;
                size_t pos = 0;
                while (pos < query.size()) {
                    size_t amp = query.find('&', pos);
                    std::string kv = (amp == std::string::npos) ? query.substr(pos) : query.substr(pos, amp - pos);
                    size_t eq = kv.find('=');
                    if (eq != std::string::npos) {
                        std::string k = kv.substr(0, eq);
                        std::string v = urldecode(kv.substr(eq + 1));
                        try {
                            if (k == "vehicle_id") vid = static_cast<uint32_t>(std::stoul(v));
                        } catch (...) {}
                    }
                    if (amp == std::string::npos) break;
                    pos = amp + 1;
                }

                SensorData sim_input{};
                sim_input.vehicle_id = vid;
                sim_input.timestamp_ms = current_timestamp_ms();
                sim_input.roof_stress = 80.0;
                sim_input.wheel_deformation = 2.0;
                sim_input.rock_impact_force = 35.0;
                sim_input.protection_thickness = 80.0;
                sim_input.protection_material = "wood";
                sim_input.ambient_temp = 20.0;
                sim_input.impact_location_x = 3.0;
                sim_input.impact_location_y = 1.25;
                sim_input.rock_mass = 50.0;
                sim_input.rock_velocity = 15.0;

                auto sim = simulator_->run_simulation(sim_input);

                json::Value def_field(json::Array{});
                for (double v : sim.deformation_field) def_field.append(v);
                json::Value stress_field(json::Array{});
                for (double v : sim.stress_field) stress_field.append(v);

                json::Value r(json::Object{});
                r["roof_max_deformation_mm"] = sim.roof_max_deformation_mm;
                r["roof_plastic_strain"] = sim.roof_plastic_strain;
                r["roof_von_mises_stress_mpa"] = sim.roof_von_mises_stress_mpa;
                r["impact_energy_j"] = sim.impact_energy_j;
                r["absorbed_energy_j"] = sim.absorbed_energy_j;
                r["damage_level"] = static_cast<int64_t>(sim.damage_level);
                r["penetration_depth_mm"] = sim.penetration_depth_mm;
                r["is_penetrated"] = sim.is_penetrated;
                r["failure_mode"] = sim.failure_mode;
                r["deformation_field"] = def_field;
                r["stress_field"] = stress_field;

                json::Value result(json::Object{});
                result["data"] = r;
                response_body = result.dump();
            } else if (path == "/api/evaluate" && method == "POST") {
                json::Value root;
                uint32_t vid = 1;
                if (parse_json_body(body, root)) {
                    vid = static_cast<uint32_t>(root.has("vehicle_id") ? root["vehicle_id"].asUInt() : 1);
                }
                auto evals = run_ahp_evaluation(vid);
                json::Value arr(json::Array{});
                for (const auto& e : evals) {
                    json::Value item(json::Object{});
                    item["material_type"] = e.material_type;
                    item["material_thickness_mm"] = e.material_thickness_mm;
                    item["energy_absorption_score"] = e.energy_absorption_score;
                    item["structural_strength_score"] = e.structural_strength_score;
                    item["weight_factor_score"] = e.weight_factor_score;
                    item["cost_factor_score"] = e.cost_factor_score;
                    item["durability_score"] = e.durability_score;
                    item["ahp_weight_score"] = e.ahp_weight_score;
                    item["rank_position"] = static_cast<int64_t>(e.rank_position);
                    item["is_recommended"] = e.is_recommended;
                    arr.append(item);
                }
                json::Value result(json::Object{});
                result["data"] = arr;
                result["consistency_ratio"] = ahp_evaluator_->get_consistency_ratio();
                response_body = result.dump();
            } else if (path == "/api/ahp/weights" && method == "GET") {
                auto weights = ahp_evaluator_->get_criteria_weights();
                json::Value w(json::Object{});
                for (const auto& [k, v] : weights) {
                    w[k] = v;
                }
                json::Value result(json::Object{});
                result["weights"] = w;
                result["consistency_ratio"] = ahp_evaluator_->get_consistency_ratio();
                response_body = result.dump();
            } else if (path == "/api/config" && method == "GET") {
                json::Value cfg(json::Object{});
                cfg["deformation_threshold_mm"] = config_.deformation_threshold_mm;
                cfg["penetration_threshold_ratio"] = config_.penetration_threshold_ratio;
                cfg["http_port"] = static_cast<int64_t>(config_.http_port);
                cfg["clickhouse_host"] = config_.clickhouse_host;
                cfg["mqtt_broker"] = config_.mqtt_broker;

                json::Value mats(json::Object{});
                const char* mat_names[] = {"cowhide", "wood", "iron", "composite"};
                double mat_data[][8] = {
                    {860.0,   0.15, 0.40,  25.0,  60.0, 15.0,  80.0, 3.0},
                    {650.0,  10.0,  0.35,  60.0, 120.0, 10.0,  50.0, 1.0},
                    {7850.0, 206.0, 0.29, 235.0, 400.0, 80.0, 200.0, 10.0},
                    {900.0,  15.0,  0.33, 120.0, 250.0, 45.0, 300.0, 5.0}
                };
                for (int i = 0; i < 4; ++i) {
                    json::Value m(json::Object{});
                    m["density"] = mat_data[i][0];
                    m["youngs_modulus_gpa"] = mat_data[i][1];
                    m["poisson_ratio"] = mat_data[i][2];
                    m["yield_strength_mpa"] = mat_data[i][3];
                    m["ultimate_strength_mpa"] = mat_data[i][4];
                    m["toughness_mj_m3"] = mat_data[i][5];
                    m["specific_energy_absorption_kj_kg"] = mat_data[i][6];
                    m["cost_per_unit"] = mat_data[i][7];
                    mats[mat_names[i]] = m;
                }
                cfg["materials"] = mats;

                json::Value result(json::Object{});
                result["config"] = cfg;
                response_body = result.dump();
            } else {
                response_body = "{\"error\":\"Not found\"}";
                response_code = 404;
            }
        }

        std::string response = http_response(response_code, content_type, response_body);
#ifdef _WIN32
        send(client_fd, response.c_str(), static_cast<int>(response.size()), 0);
#else
        send(client_fd, response.c_str(), response.size(), MSG_NOSIGNAL);
#endif
        CLOSE_SOCKET(client_fd);
    }
}

bool FenYunServer::start() {
    if (running_) return true;

    db_client_->connect();
    mqtt_manager_->connect();

    running_ = true;

    for (int i = 0; i < config_.simulation_threads; ++i) {
        sim_threads_.emplace_back(&FenYunServer::simulation_worker, this);
    }
    for (int i = 0; i < config_.alert_threads; ++i) {
        alert_threads_.emplace_back(&FenYunServer::alert_worker, this);
    }
    http_thread_ = std::thread(&FenYunServer::http_server_worker, this);

    std::cout << "[FenYunServer] Started successfully" << std::endl;
    return true;
}

void FenYunServer::stop() {
    if (!running_) return;
    running_ = false;

    sim_queue_cv_.notify_all();
    alert_queue_cv_.notify_all();

    for (auto& t : sim_threads_) {
        if (t.joinable()) t.join();
    }
    for (auto& t : alert_threads_) {
        if (t.joinable()) t.join();
    }
    if (http_impl_->server_fd != INVALID_SOCK) {
        CLOSE_SOCKET(http_impl_->server_fd);
        http_impl_->server_fd = INVALID_SOCK;
    }
    if (http_thread_.joinable()) http_thread_.join();

    mqtt_manager_->disconnect();
    db_client_->disconnect();

    sim_threads_.clear();
    alert_threads_.clear();

    std::cout << "[FenYunServer] Stopped" << std::endl;
}

bool FenYunServer::is_running() const {
    return running_;
}

bool FenYunServer::ingest_sensor_data(const SensorData& data) {
    db_client_->insert_sensor_data(data);
    process_sensor_data(data);
    return true;
}

std::vector<ProtectionEvaluation> FenYunServer::run_ahp_evaluation(uint32_t vehicle_id) {
    auto evals = ahp_evaluator_->evaluate_all(vehicle_id);
    uint64_t eval_id = 1;
    for (auto& e : evals) {
        e.eval_id = eval_id++;
        e.timestamp_ms = current_timestamp_ms();
    }
    db_client_->insert_protection_evaluation_batch(evals);
    return evals;
}

std::vector<SensorData> FenYunServer::get_sensor_history(uint32_t vehicle_id,
                                                          int64_t start_ts,
                                                          int64_t end_ts,
                                                          int limit) {
    return db_client_->query_sensor_data(vehicle_id, start_ts, end_ts, limit);
}

std::vector<SimulationResult> FenYunServer::get_simulation_history(uint32_t vehicle_id, int limit) {
    return db_client_->query_simulation_results(vehicle_id, limit);
}

std::vector<AlertRecord> FenYunServer::get_alert_history(uint32_t vehicle_id, int limit) {
    return db_client_->query_alerts(vehicle_id, limit);
}

ServerConfig FenYunServer::get_config() const {
    return config_;
}

}
