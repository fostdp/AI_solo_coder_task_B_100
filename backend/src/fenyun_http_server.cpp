#include "fenyun_http_server.h"
#include "common/json.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <map>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace fenyun {

FenyunHttpServer::FenyunHttpServer(std::shared_ptr<FenyunApplication> app)
    : app_(std::move(app)) {
}

FenyunHttpServer::~FenyunHttpServer() {
    stop();
}

bool FenyunHttpServer::start(uint16_t port, const std::string& host) {
    port_ = port;
    host_ = host;

#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    server_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (server_fd_ < 0) {
        std::cerr << "[HttpServer] socket() failed" << std::endl;
        return false;
    }

    int opt = 1;
#if defined(_WIN32)
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "[HttpServer] bind() failed on port " << port << std::endl;
        return false;
    }

    if (listen(server_fd_, 32) != 0) {
        std::cerr << "[HttpServer] listen() failed" << std::endl;
        return false;
    }

    running_.store(true);
    accept_thread_ = std::thread(&FenyunHttpServer::accept_loop, this);

    std::cout << "[HttpServer] Listening on " << host << ":" << port << std::endl;
    return true;
}

void FenyunHttpServer::stop() {
    if (!running_.exchange(false)) return;

#if defined(_WIN32)
    closesocket(server_fd_);
#else
    close(server_fd_);
#endif
    server_fd_ = -1;

    if (accept_thread_.joinable()) accept_thread_.join();

#if defined(_WIN32)
    WSACleanup();
#endif
    std::cout << "[HttpServer] Stopped" << std::endl;
}

void FenyunHttpServer::accept_loop() {
    while (running_.load()) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = static_cast<int>(accept(
            server_fd_,
            reinterpret_cast<sockaddr*>(&client_addr),
            &addr_len
        ));
        if (client_fd < 0) {
            if (!running_.load()) break;
            continue;
        }
        handle_client(client_fd);
    }
}

void FenyunHttpServer::handle_client(int client_fd) {
    char buffer[16384];
    std::string request;

    int total = 0;
    for (int i = 0; i < 20; ++i) {
#if defined(_WIN32)
        int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
#else
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
#endif
        if (n <= 0) break;
        buffer[n] = '\0';
        request.append(buffer, n);
        total += n;

        if (request.find("\r\n\r\n") != std::string::npos) {
            size_t header_end = request.find("\r\n\r\n");
            std::string header = request.substr(0, header_end);
            size_t cl_pos = header.find("Content-Length:");
            if (cl_pos != std::string::npos) {
                size_t len = std::stoul(header.substr(cl_pos + 16));
                if (request.size() - header_end - 4 >= len) break;
            } else {
                break;
            }
        }
    }

    size_t header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        std::string resp = make_response(400, "text/plain", "Bad Request");
        send(client_fd, resp.c_str(), static_cast<int>(resp.size()), 0);
#if defined(_WIN32)
        closesocket(client_fd);
#else
        close(client_fd);
#endif
        return;
    }

    std::string header = request.substr(0, header_end);
    std::string body = request.substr(header_end + 4);

    std::string method, path, query;
    size_t sp1 = header.find(' ');
    if (sp1 != std::string::npos) {
        method = header.substr(0, sp1);
        size_t sp2 = header.find(' ', sp1 + 1);
        if (sp2 != std::string::npos) {
            std::string full_path = header.substr(sp1 + 1, sp2 - sp1 - 1);
            size_t qp = full_path.find('?');
            if (qp != std::string::npos) {
                path = full_path.substr(0, qp);
                query = full_path.substr(qp + 1);
            } else {
                path = full_path;
            }
        }
    }

    std::string response = process_request(method, path, query, body);

    send(client_fd, response.c_str(), static_cast<int>(response.size()), 0);
#if defined(_WIN32)
    closesocket(client_fd);
#else
    close(client_fd);
#endif
}

std::string FenyunHttpServer::process_request(const std::string& method,
                                               const std::string& path,
                                               const std::string& query,
                                               const std::string& body) {
    if (method == "GET" || method == "HEAD") {
        return handle_get(path, query);
    }
    if (method == "POST") {
        return handle_post(path, body);
    }
    if (method == "OPTIONS") {
        std::string resp = make_response(204, "text/plain", "");
        return resp;
    }
    return make_response(405, "text/plain", "Method Not Allowed");
}

std::string FenyunHttpServer::handle_get(const std::string& path,
                                          const std::string& query) {
    std::map<std::string, std::string> params;
    parse_query_string(query, params);

    auto app = app_;
    if (!app) return make_response(500, "text/plain", "App not initialized");

    auto config = app->config_loader();
    auto sim = app->impact_simulator();
    auto opt = app->protection_optimizer();
    auto ch = app->clickhouse_client();

    if (path == "/api/health") {
        json::Value r(json::Object{});
        r["status"] = "ok";
        r["version"] = config ? config->system_config().version : "1.0";
        r["running"] = app->is_running();
        r["deformation_threshold"] =
            config ? config->system_config().alarm.deformation_threshold_mm : 15.0;
        if (app->dtu_receiver()) {
            r["sensors_received"] = static_cast<int64_t>(app->dtu_receiver()->total_received());
            r["sensors_valid"] = static_cast<int64_t>(app->dtu_receiver()->total_valid());
        }
        if (sim) {
            r["simulations_run"] = static_cast<int64_t>(sim->simulations_run());
        }
        if (app->alarm_mqtt()) {
            r["alerts_published"] = static_cast<int64_t>(app->alarm_mqtt()->alerts_published());
        }
        return make_json_response(200, r.dump());
    }

    if (path == "/api/config") {
        json::Value cfg(json::Object{});
        if (config) {
            cfg["version"] = config->system_config().version;
            cfg["deformation_threshold_mm"] = config->system_config().alarm.deformation_threshold_mm;
            cfg["penetration_threshold_ratio"] = config->system_config().alarm.penetration_threshold_ratio;
            cfg["stress_threshold_mpa"] = config->system_config().alarm.stress_threshold_mpa;
            cfg["strain_rate_default"] = 100.0;

            json::Value mats(json::Object{});
            for (const auto& [name, mp] : config->materials()) {
                json::Value m(json::Object{});
                m["name"] = mp.display_name.empty() ? name : mp.display_name;
                m["density"] = mp.density;
                m["youngs_modulus_gpa"] = mp.youngs_modulus_gpa;
                m["yield_strength_mpa"] = mp.yield_strength_mpa;
                m["ultimate_strength_mpa"] = mp.ultimate_strength_mpa;
                m["toughness_mj_m3"] = mp.toughness_mj_m3;
                m["cost_per_unit"] = mp.cost_per_unit;
                mats[name] = m;
            }
            cfg["materials"] = mats;

            json::Value jc_db(json::Object{});
            for (const auto& [name, jc] : config->jc_params()) {
                json::Value j(json::Object{});
                j["A_Pa"] = jc.A;
                j["B_Pa"] = jc.B;
                j["n"] = jc.n;
                j["C"] = jc.C;
                j["m"] = jc.m;
                j["T_melt_K"] = jc.T_melt;
                j["T_ref_K"] = jc.T_ref;
                j["eps_dot_0"] = jc.eps_dot_0;
                jc_db[name] = j;
            }
            cfg["johnson_cook_database"] = jc_db;

            json::Value ahp_cfg(json::Object{});
            ahp_cfg["expert_count"] = config->ahp_config().group_decision.default_expert_count;
            ahp_cfg["cr_threshold"] = config->ahp_config().consistency.cr_threshold;
            ahp_cfg["consensus_threshold"] = config->ahp_config().group_decision.consensus_threshold;
            ahp_cfg["enabled"] = config->ahp_config().group_decision.enabled;
            cfg["ahp"] = ahp_cfg;
        }
        json::Value result(json::Object{});
        result["config"] = cfg;
        return make_json_response(200, result.dump());
    }

    if (path == "/api/ahp/weights") {
        if (!opt) return make_response(500, "text/plain", "Optimizer not ready");
        auto weights = opt->get_criteria_weights();
        auto criteria = config ? config->ahp_config().criteria :
            std::vector<std::string>{"energy","strength","weight","cost","durability"};

        json::Value obj(json::Object{});
        for (size_t i = 0; i < weights.size() && i < criteria.size(); ++i) {
            obj[criteria[i]] = weights[i];
        }
        json::Value r(json::Object{});
        r["weights"] = obj;
        r["consistency_ratio"] = opt->get_consistency_ratio();
        return make_json_response(200, r.dump());
    }

    if (path == "/api/sensor/history") {
        uint32_t vid = 1;
        int64_t start = 0;
        int64_t end = current_timestamp_ms();
        int limit = 60;

        if (params.count("vehicle_id")) vid = static_cast<uint32_t>(std::stoul(params["vehicle_id"]));
        if (params.count("start")) start = std::stoll(params["start"]);
        if (params.count("end")) end = std::stoll(params["end"]);
        if (params.count("limit")) limit = std::stoi(params["limit"]);

        std::vector<SensorData> history;
        if (ch) history = ch->query_sensor_history(vid, start, end, limit);

        json::Value arr(json::Array{});
        for (const auto& d : history) {
            json::Value item(json::Object{});
            item["vehicle_id"] = static_cast<int64_t>(d.vehicle_id);
            item["timestamp_ms"] = d.timestamp_ms;
            item["roof_stress"] = d.roof_stress;
            item["wheel_deformation"] = d.wheel_deformation;
            item["rock_impact_force"] = d.rock_impact_force;
            item["protection_thickness"] = d.protection_thickness;
            item["protection_material"] = d.protection_material;
            item["ambient_temp"] = d.ambient_temp;
            arr.append(item);
        }
        json::Value r(json::Object{});
        r["data"] = arr;
        r["total"] = static_cast<int64_t>(history.size());
        return make_json_response(200, r.dump());
    }

    if (path == "/api/simulation/latest") {
        if (!sim) return make_response(500, "text/plain", "Simulator not ready");

        uint32_t vid = 1;
        if (params.count("vehicle_id")) {
            vid = static_cast<uint32_t>(std::stoul(params["vehicle_id"]));
        }

        SensorData data{};
        data.vehicle_id = vid;
        data.timestamp_ms = current_timestamp_ms();
        data.roof_stress = 80.0;
        data.wheel_deformation = 0.3;
        data.rock_impact_force = 50.0;
        data.protection_thickness = config
            ? config->system_config().dtu.default_protection_thickness_mm : 80.0;
        data.protection_material = config
            ? config->system_config().dtu.default_material : "wood";
        data.ambient_temp = 20.0;
        data.impact_location_x = 3.0;
        data.impact_location_y = 1.25;
        data.rock_mass = 20 + std::rand() % 180;
        data.rock_velocity = 10.0 + (std::rand() % 2000) / 100.0;

        auto result = sim->run_simulation(data);

        json::Value def_field(json::Array{});
        for (double v : result.deformation_field) def_field.append(v);
        json::Value stress_field(json::Array{});
        for (double v : result.stress_field) stress_field.append(v);

        auto jc = sim->get_jc_params(result.protection_material);
        json::Value jc_json(json::Object{});
        jc_json["A_Pa"] = jc.A;
        jc_json["B_Pa"] = jc.B;
        jc_json["n"] = jc.n;
        jc_json["C"] = jc.C;
        jc_json["m"] = jc.m;
        jc_json["T_melt_K"] = jc.T_melt;
        jc_json["T_ref_K"] = jc.T_ref;
        jc_json["eps_dot_0"] = jc.eps_dot_0;

        json::Value r(json::Object{});
        r["roof_max_deformation_mm"] = result.roof_max_deformation_mm;
        r["roof_plastic_strain"] = result.roof_plastic_strain;
        r["roof_von_mises_stress_mpa"] = result.roof_von_mises_stress_mpa;
        r["impact_energy_j"] = result.impact_energy_j;
        r["absorbed_energy_j"] = result.absorbed_energy_j;
        r["damage_level"] = static_cast<int64_t>(result.damage_level);
        r["penetration_depth_mm"] = result.penetration_depth_mm;
        r["is_penetrated"] = result.is_penetrated;
        r["failure_mode"] = result.failure_mode;
        r["strain_rate"] = result.strain_rate;
        r["dynamic_yield_strength_mpa"] = result.dynamic_yield_strength_mpa;
        r["temperature_K"] = result.temperature_K;
        r["protection_material"] = result.protection_material;
        r["johnson_cook_params"] = jc_json;
        r["deformation_field"] = def_field;
        r["stress_field"] = stress_field;

        json::Value wrapper(json::Object{});
        wrapper["data"] = r;
        return make_json_response(200, wrapper.dump());
    }

    return make_response(404, "text/plain", "Not Found");
}

std::string FenyunHttpServer::handle_post(const std::string& path,
                                            const std::string& body) {
    auto app = app_;
    if (!app) return make_response(500, "text/plain", "App not initialized");

    auto dtu = app->dtu_receiver();
    auto opt = app->protection_optimizer();
    auto sim = app->impact_simulator();
    auto config = app->config_loader();

    if (path == "/api/sensor") {
        if (!dtu) return make_response(500, "text/plain", "DTU not ready");
        std::string error;
        bool ok = dtu->ingest_sensor_json(body, error);
        json::Value r(json::Object{});
        r["status"] = ok ? "received" : "error";
        if (!ok) r["error"] = error;
        return make_json_response(ok ? 200 : 400, r.dump());
    }

    if (path == "/api/sensor/batch") {
        if (!dtu) return make_response(500, "text/plain", "DTU not ready");
        std::string error;
        int ok = dtu->ingest_json_batch(body, error);
        json::Value r(json::Object{});
        r["received"] = static_cast<int64_t>(ok);
        r["error"] = error;
        return make_json_response(200, r.dump());
    }

    if (path == "/api/evaluate") {
        if (!opt) return make_response(500, "text/plain", "Optimizer not ready");

        json::Value root = json::parse(body);
        uint32_t vid = 1;
        int expert_count = 5;
        bool enable_group = true;

        if (root.isObject()) {
            if (root.has("vehicle_id")) {
                vid = static_cast<uint32_t>(root["vehicle_id"].asUInt());
            }
            if (root.has("expert_count")) {
                expert_count = root["expert_count"].asInt();
            }
            if (root.has("enable_group_decision")) {
                enable_group = root["enable_group_decision"].asBool();
            }
        }

        if (enable_group && expert_count > 1) {
            opt->enable_group_decision(true);
            opt->set_expert_count(expert_count);
        } else {
            opt->enable_group_decision(false);
        }

        auto evals = opt->evaluate_all(vid, enable_group, expert_count);

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
        result["consistency_ratio"] = opt->get_consistency_ratio();
        result["expert_count"] = static_cast<int64_t>(enable_group ? expert_count : 1);
        result["enable_group_decision"] = enable_group;

        if (enable_group && expert_count > 1) {
            auto gr = opt->get_group_decision_report();
            result["group_consensus_index"] = gr.consensus_index;
            result["group_cr"] = gr.group_cr;
            result["passed_experts"] = static_cast<int64_t>(gr.passed_experts);
            result["total_experts"] = static_cast<int64_t>(gr.total_experts);

            json::Value exp_arr(json::Array{});
            for (const auto& e : gr.experts) {
                json::Value ej(json::Object{});
                ej["name"] = e.name;
                ej["title"] = e.title;
                ej["authority_weight"] = e.authority_weight;
                ej["consistency_ratio"] = e.consistency_ratio;
                ej["passed"] = e.passed;
                exp_arr.append(ej);
            }
            result["experts"] = exp_arr;
        }

        auto weights = opt->get_criteria_weights();
        auto criteria = config ? config->ahp_config().criteria :
            std::vector<std::string>{"energy","strength","weight","cost","durability"};
        json::Value w_obj(json::Object{});
        for (size_t i = 0; i < weights.size() && i < criteria.size(); ++i) {
            w_obj[criteria[i]] = weights[i];
        }
        result["criteria_weights"] = w_obj;

        return make_json_response(200, result.dump());
    }

    return make_response(404, "text/plain", "Not Found");
}

std::string FenyunHttpServer::make_json_response(int status_code,
                                                  const std::string& json_body) {
    return make_response(status_code, "application/json; charset=utf-8", json_body);
}

std::string FenyunHttpServer::make_response(int status_code,
                                             const std::string& content_type,
                                             const std::string& body) {
    std::ostringstream oss;
    const char* reason = "OK";
    switch (status_code) {
        case 200: reason = "OK"; break;
        case 204: reason = "No Content"; break;
        case 400: reason = "Bad Request"; break;
        case 404: reason = "Not Found"; break;
        case 405: reason = "Method Not Allowed"; break;
        case 500: reason = "Internal Server Error"; break;
        default:  reason = "OK";
    }
    oss << "HTTP/1.1 " << status_code << " " << reason << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

std::string FenyunHttpServer::url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            std::string hex = s.substr(i + 1, 2);
            int val = std::stoi(hex, nullptr, 16);
            out += static_cast<char>(val);
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

void FenyunHttpServer::parse_query_string(const std::string& query,
                                           std::map<std::string, std::string>& params) {
    size_t pos = 0;
    while (pos < query.size()) {
        size_t amp = query.find('&', pos);
        std::string pair = query.substr(pos,
            amp == std::string::npos ? query.size() - pos : amp - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = url_decode(pair.substr(0, eq));
            std::string val = url_decode(pair.substr(eq + 1));
            params[key] = val;
        } else if (!pair.empty()) {
            params[url_decode(pair)] = "";
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
}

}
