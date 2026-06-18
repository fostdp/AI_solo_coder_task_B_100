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
    auto vc = app->vehicle_comparator();
    auto fo = app->formation_optimizer();
    auto usm = app->user_session_manager();

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

    if (path == "/api/vehicles") {
        if (!vc) return make_response(500, "text/plain", "VehicleComparator not ready");
        auto vehicles = vc->list_all_vehicles();
        json::Value arr(json::Array{});
        for (const auto& v : vehicles) {
            json::Value item(json::Object{});
            item["id"] = v.id;
            item["display_name"] = v.display_name;
            item["description"] = v.description;
            item["era"] = v.era == VehicleEra::ANCIENT ? "ancient" : "modern";
            item["type"] = static_cast<int64_t>(v.type);
            item["length_m"] = v.length_m;
            item["width_m"] = v.width_m;
            item["height_m"] = v.height_m;
            item["weight_ton"] = v.weight_ton;
            item["crew_count"] = static_cast<int64_t>(v.crew_count);
            item["max_speed_kmh"] = v.max_speed_kmh;
            item["roof_thickness_mm"] = v.roof_thickness_mm;
            item["wall_thickness_mm"] = v.wall_thickness_mm;
            item["primary_material"] = v.primary_material;
            json::Value mats(json::Array{});
            for (const auto& m : v.available_materials) mats.append(m);
            item["available_materials"] = mats;
            item["protection_area_m2"] = v.protection_area_m2;
            item["historical_year"] = static_cast<int64_t>(v.historical_year);
            item["origin"] = v.origin;
            arr.append(item);
        }
        json::Value result(json::Object{});
        result["data"] = arr;
        return make_json_response(200, result.dump());
    }

    if (path == "/api/vehicles/ancient") {
        if (!vc) return make_response(500, "text/plain", "VehicleComparator not ready");
        auto vehicles = vc->list_ancient_vehicles();
        json::Value arr(json::Array{});
        for (const auto& v : vehicles) {
            json::Value item(json::Object{});
            item["id"] = v.id;
            item["display_name"] = v.display_name;
            item["description"] = v.description;
            item["era"] = v.era == VehicleEra::ANCIENT ? "ancient" : "modern";
            item["type"] = static_cast<int64_t>(v.type);
            item["length_m"] = v.length_m;
            item["width_m"] = v.width_m;
            item["height_m"] = v.height_m;
            item["weight_ton"] = v.weight_ton;
            item["crew_count"] = static_cast<int64_t>(v.crew_count);
            item["max_speed_kmh"] = v.max_speed_kmh;
            item["roof_thickness_mm"] = v.roof_thickness_mm;
            item["wall_thickness_mm"] = v.wall_thickness_mm;
            item["primary_material"] = v.primary_material;
            json::Value mats(json::Array{});
            for (const auto& m : v.available_materials) mats.append(m);
            item["available_materials"] = mats;
            item["protection_area_m2"] = v.protection_area_m2;
            item["historical_year"] = static_cast<int64_t>(v.historical_year);
            item["origin"] = v.origin;
            arr.append(item);
        }
        json::Value result(json::Object{});
        result["data"] = arr;
        return make_json_response(200, result.dump());
    }

    if (path == "/api/vehicles/modern") {
        if (!vc) return make_response(500, "text/plain", "VehicleComparator not ready");
        auto vehicles = vc->list_modern_vehicles();
        json::Value arr(json::Array{});
        for (const auto& v : vehicles) {
            json::Value item(json::Object{});
            item["id"] = v.id;
            item["display_name"] = v.display_name;
            item["description"] = v.description;
            item["era"] = v.era == VehicleEra::ANCIENT ? "ancient" : "modern";
            item["type"] = static_cast<int64_t>(v.type);
            item["length_m"] = v.length_m;
            item["width_m"] = v.width_m;
            item["height_m"] = v.height_m;
            item["weight_ton"] = v.weight_ton;
            item["crew_count"] = static_cast<int64_t>(v.crew_count);
            item["max_speed_kmh"] = v.max_speed_kmh;
            item["roof_thickness_mm"] = v.roof_thickness_mm;
            item["wall_thickness_mm"] = v.wall_thickness_mm;
            item["primary_material"] = v.primary_material;
            json::Value mats(json::Array{});
            for (const auto& m : v.available_materials) mats.append(m);
            item["available_materials"] = mats;
            item["protection_area_m2"] = v.protection_area_m2;
            item["historical_year"] = static_cast<int64_t>(v.historical_year);
            item["origin"] = v.origin;
            arr.append(item);
        }
        json::Value result(json::Object{});
        result["data"] = arr;
        return make_json_response(200, result.dump());
    }

    if (path.size() > 14 && path.substr(0, 14) == "/api/vehicles/") {
        if (!config) return make_response(500, "text/plain", "ConfigLoader not ready");
        std::string vid = path.substr(14);
        if (!config->has_vehicle(vid)) {
            return make_response(404, "text/plain", "Vehicle not found");
        }
        auto v = config->get_vehicle(vid);
        json::Value item(json::Object{});
        item["id"] = v.id;
        item["display_name"] = v.display_name;
        item["description"] = v.description;
        item["era"] = v.era == VehicleEra::ANCIENT ? "ancient" : "modern";
        item["type"] = static_cast<int64_t>(v.type);
        item["length_m"] = v.length_m;
        item["width_m"] = v.width_m;
        item["height_m"] = v.height_m;
        item["weight_ton"] = v.weight_ton;
        item["crew_count"] = static_cast<int64_t>(v.crew_count);
        item["max_speed_kmh"] = v.max_speed_kmh;
        item["roof_thickness_mm"] = v.roof_thickness_mm;
        item["wall_thickness_mm"] = v.wall_thickness_mm;
        item["primary_material"] = v.primary_material;
        json::Value mats(json::Array{});
        for (const auto& m : v.available_materials) mats.append(m);
        item["available_materials"] = mats;
        item["protection_area_m2"] = v.protection_area_m2;
        item["historical_year"] = static_cast<int64_t>(v.historical_year);
        item["origin"] = v.origin;
        json::Value result(json::Object{});
        result["data"] = item;
        return make_json_response(200, result.dump());
    }

    if (path == "/api/vehicle/comparison/latest") {
        json::Value result(json::Object{});
        result["data"] = json::Value();
        return make_json_response(200, result.dump());
    }

    if (path == "/api/formation/types") {
        json::Value arr(json::Array{});
        json::Value t1(json::Object{});
        t1["type"] = "LINE";
        t1["name"] = "横排";
        t1["desc"] = "并列推进,覆盖最广";
        arr.append(t1);
        json::Value t2(json::Object{});
        t2["type"] = "WEDGE";
        t2["name"] = "楔形";
        t2["desc"] = "尖刀阵型,推进最快";
        arr.append(t2);
        json::Value t3(json::Object{});
        t3["type"] = "ECHELON";
        t3["name"] = "梯形";
        t3["desc"] = "平衡型,兼顾覆盖和推进";
        arr.append(t3);
        json::Value t4(json::Object{});
        t4["type"] = "V_SHAPE";
        t4["name"] = "V形";
        t4["desc"] = "保护中路,两翼前出";
        arr.append(t4);
        json::Value t5(json::Object{});
        t5["type"] = "COLUMN";
        t5["name"] = "纵队";
        t5["desc"] = "纵深推进,集中突破";
        arr.append(t5);
        json::Value t6(json::Object{});
        t6["type"] = "DIAMOND";
        t6["name"] = "菱形";
        t6["desc"] = "全方位防护,适合混战";
        arr.append(t6);
        return make_json_response(200, arr.dump());
    }

    if (path == "/api/user/sessions") {
        if (!usm) return make_response(500, "text/plain", "UserSessionManager not ready");
        auto sessions = usm->active_sessions();
        json::Value arr(json::Array{});
        for (const auto& s : sessions) {
            json::Value item(json::Object{});
            item["session_id"] = s.session_id;
            item["user_nickname"] = s.user_nickname;
            item["vehicle_type"] = s.vehicle_type;
            item["created_ms"] = s.created_ms;
            item["last_active_ms"] = s.last_active_ms;
            arr.append(item);
        }
        json::Value result(json::Object{});
        result["data"] = arr;
        return make_json_response(200, result.dump());
    }

    if (path.size() > 22 && path.substr(0, 22) == "/api/user/session/" && path.find("/state") != std::string::npos) {
        if (!usm) return make_response(500, "text/plain", "UserSessionManager not ready");
        size_t state_pos = path.find("/state");
        std::string sid = path.substr(22, state_pos - 22);
        if (!usm->has_session(sid)) {
            return make_response(404, "text/plain", "Session not found");
        }
        auto st = usm->get_vehicle_state(sid);
        json::Value item(json::Object{});
        item["session_id"] = st.session_id;
        item["position_x"] = st.position_x;
        item["position_y"] = st.position_y;
        item["heading_deg"] = st.heading_deg;
        item["speed_ms"] = st.speed_ms;
        item["health_percent"] = st.health_percent;
        item["armor_integrity_percent"] = st.armor_integrity_percent;
        item["impacts_received"] = static_cast<int64_t>(st.impacts_received);
        item["distance_traveled_m"] = st.distance_traveled_m;
        item["timestamp_ms"] = st.timestamp_ms;
        json::Value result(json::Object{});
        result["data"] = item;
        return make_json_response(200, result.dump());
    }

    if (path.size() > 22 && path.substr(0, 22) == "/api/user/session/" && path.find("/attacks") != std::string::npos) {
        if (!usm) return make_response(500, "text/plain", "UserSessionManager not ready");
        size_t attacks_pos = path.find("/attacks");
        std::string sid = path.substr(22, attacks_pos - 22);
        if (!usm->has_session(sid)) {
            return make_response(404, "text/plain", "Session not found");
        }
        auto attacks = usm->process_time_step(sid, 0.0);
        json::Value arr(json::Array{});
        for (const auto& a : attacks) {
            json::Value item(json::Object{});
            item["event_id"] = static_cast<int64_t>(a.event_id);
            item["impact_x"] = a.impact_x;
            item["impact_y"] = a.impact_y;
            item["rock_mass_kg"] = a.rock_mass_kg;
            item["rock_velocity_ms"] = a.rock_velocity_ms;
            item["damage_dealt"] = a.damage_dealt;
            item["timestamp_ms"] = a.timestamp_ms;
            arr.append(item);
        }
        json::Value result(json::Object{});
        result["data"] = arr;
        return make_json_response(200, result.dump());
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
    auto vc = app->vehicle_comparator();
    auto fo = app->formation_optimizer();
    auto usm = app->user_session_manager();

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

    if (path == "/api/vehicle/comparison") {
        if (!vc) return make_response(500, "text/plain", "VehicleComparator not ready");
        json::Value root = json::parse(body);
        VehicleComparisonRequest req{};
        req.rock_mass_kg = 50.0;
        req.rock_velocity_ms = 15.0;
        req.impact_location_x = 3.0;
        req.impact_location_y = 1.4;
        req.temperature_K = 293.15;
        req.use_johnson_cook = true;
        if (root.isObject()) {
            if (root.has("vehicle_ids") && root["vehicle_ids"].isArray()) {
                for (size_t i = 0; i < root["vehicle_ids"].size(); ++i) {
                    req.vehicle_ids.push_back(root["vehicle_ids"][i].asString());
                }
            }
            if (root.has("rock_mass_kg")) req.rock_mass_kg = root["rock_mass_kg"].asDouble();
            if (root.has("rock_velocity_ms")) req.rock_velocity_ms = root["rock_velocity_ms"].asDouble();
            if (root.has("impact_location_x")) req.impact_location_x = root["impact_location_x"].asDouble();
            if (root.has("impact_location_y")) req.impact_location_y = root["impact_location_y"].asDouble();
            if (root.has("temperature_K")) req.temperature_K = root["temperature_K"].asDouble();
            if (root.has("use_johnson_cook")) req.use_johnson_cook = root["use_johnson_cook"].asBool();
        }
        auto result = vc->compare_vehicles(req);
        json::Value r(json::Object{});
        r["comparison_id"] = static_cast<int64_t>(result.comparison_id);
        r["timestamp_ms"] = result.timestamp_ms;
        r["best_vehicle_id"] = result.best_vehicle_id;
        json::Value insights_arr(json::Array{});
        for (const auto& ins : result.insights) insights_arr.append(ins);
        r["insights"] = insights_arr;
        json::Value items_arr(json::Array{});
        for (const auto& item : result.items) {
            json::Value it(json::Object{});
            it["vehicle_id"] = item.vehicle_id;
            it["display_name"] = item.display_name;
            it["era"] = item.era == VehicleEra::ANCIENT ? "ancient" : "modern";
            it["protection_efficiency_score"] = item.protection_efficiency_score;
            it["weight_normalized_score"] = item.weight_normalized_score;
            it["cost_normalized_score"] = item.cost_normalized_score;
            it["overall_score"] = item.overall_score;
            it["rank"] = static_cast<int64_t>(item.rank);
            json::Value sim(json::Object{});
            sim["simulation_id"] = static_cast<int64_t>(item.simulation.simulation_id);
            sim["vehicle_id"] = static_cast<int64_t>(item.simulation.vehicle_id);
            sim["timestamp_ms"] = item.simulation.timestamp_ms;
            sim["roof_max_deformation_mm"] = item.simulation.roof_max_deformation_mm;
            sim["roof_plastic_strain"] = item.simulation.roof_plastic_strain;
            sim["roof_von_mises_stress_mpa"] = item.simulation.roof_von_mises_stress_mpa;
            sim["impact_energy_j"] = item.simulation.impact_energy_j;
            sim["absorbed_energy_j"] = item.simulation.absorbed_energy_j;
            sim["damage_level"] = static_cast<int64_t>(item.simulation.damage_level);
            sim["penetration_depth_mm"] = item.simulation.penetration_depth_mm;
            sim["is_penetrated"] = item.simulation.is_penetrated;
            sim["failure_mode"] = item.simulation.failure_mode;
            sim["strain_rate"] = item.simulation.strain_rate;
            sim["dynamic_yield_strength_mpa"] = item.simulation.dynamic_yield_strength_mpa;
            sim["temperature_K"] = item.simulation.temperature_K;
            sim["protection_material"] = item.simulation.protection_material;
            json::Value def_field(json::Array{});
            for (double v : item.simulation.deformation_field) def_field.append(v);
            sim["deformation_field"] = def_field;
            json::Value stress_field(json::Array{});
            for (double v : item.simulation.stress_field) stress_field.append(v);
            sim["stress_field"] = stress_field;
            it["simulation"] = sim;
            items_arr.append(it);
        }
        r["items"] = items_arr;
        json::Value wrapper(json::Object{});
        wrapper["data"] = r;
        return make_json_response(200, wrapper.dump());
    }

    if (path == "/api/vehicle/comparison/ancient") {
        if (!vc) return make_response(500, "text/plain", "VehicleComparator not ready");
        json::Value root = json::parse(body);
        double rock_mass_kg = 50.0;
        double rock_velocity_ms = 15.0;
        if (root.isObject()) {
            if (root.has("rock_mass_kg")) rock_mass_kg = root["rock_mass_kg"].asDouble();
            if (root.has("rock_velocity_ms")) rock_velocity_ms = root["rock_velocity_ms"].asDouble();
        }
        auto result = vc->compare_ancient_vehicles(rock_mass_kg, rock_velocity_ms);
        json::Value r(json::Object{});
        r["comparison_id"] = static_cast<int64_t>(result.comparison_id);
        r["timestamp_ms"] = result.timestamp_ms;
        r["best_vehicle_id"] = result.best_vehicle_id;
        json::Value insights_arr(json::Array{});
        for (const auto& ins : result.insights) insights_arr.append(ins);
        r["insights"] = insights_arr;
        json::Value items_arr(json::Array{});
        for (const auto& item : result.items) {
            json::Value it(json::Object{});
            it["vehicle_id"] = item.vehicle_id;
            it["display_name"] = item.display_name;
            it["era"] = item.era == VehicleEra::ANCIENT ? "ancient" : "modern";
            it["protection_efficiency_score"] = item.protection_efficiency_score;
            it["weight_normalized_score"] = item.weight_normalized_score;
            it["cost_normalized_score"] = item.cost_normalized_score;
            it["overall_score"] = item.overall_score;
            it["rank"] = static_cast<int64_t>(item.rank);
            json::Value sim(json::Object{});
            sim["simulation_id"] = static_cast<int64_t>(item.simulation.simulation_id);
            sim["vehicle_id"] = static_cast<int64_t>(item.simulation.vehicle_id);
            sim["timestamp_ms"] = item.simulation.timestamp_ms;
            sim["roof_max_deformation_mm"] = item.simulation.roof_max_deformation_mm;
            sim["roof_plastic_strain"] = item.simulation.roof_plastic_strain;
            sim["roof_von_mises_stress_mpa"] = item.simulation.roof_von_mises_stress_mpa;
            sim["impact_energy_j"] = item.simulation.impact_energy_j;
            sim["absorbed_energy_j"] = item.simulation.absorbed_energy_j;
            sim["damage_level"] = static_cast<int64_t>(item.simulation.damage_level);
            sim["penetration_depth_mm"] = item.simulation.penetration_depth_mm;
            sim["is_penetrated"] = item.simulation.is_penetrated;
            sim["failure_mode"] = item.simulation.failure_mode;
            sim["strain_rate"] = item.simulation.strain_rate;
            sim["dynamic_yield_strength_mpa"] = item.simulation.dynamic_yield_strength_mpa;
            sim["temperature_K"] = item.simulation.temperature_K;
            sim["protection_material"] = item.simulation.protection_material;
            json::Value def_field(json::Array{});
            for (double v : item.simulation.deformation_field) def_field.append(v);
            sim["deformation_field"] = def_field;
            json::Value stress_field(json::Array{});
            for (double v : item.simulation.stress_field) stress_field.append(v);
            sim["stress_field"] = stress_field;
            it["simulation"] = sim;
            items_arr.append(it);
        }
        r["items"] = items_arr;
        json::Value wrapper(json::Object{});
        wrapper["data"] = r;
        return make_json_response(200, wrapper.dump());
    }

    if (path == "/api/vehicle/comparison/cross-era") {
        if (!vc) return make_response(500, "text/plain", "VehicleComparator not ready");
        json::Value root = json::parse(body);
        double rock_mass_kg = 50.0;
        double rock_velocity_ms = 15.0;
        if (root.isObject()) {
            if (root.has("rock_mass_kg")) rock_mass_kg = root["rock_mass_kg"].asDouble();
            if (root.has("rock_velocity_ms")) rock_velocity_ms = root["rock_velocity_ms"].asDouble();
        }
        auto result = vc->compare_cross_era(rock_mass_kg, rock_velocity_ms);
        json::Value r(json::Object{});
        r["comparison_id"] = static_cast<int64_t>(result.comparison_id);
        r["timestamp_ms"] = result.timestamp_ms;
        r["best_vehicle_id"] = result.best_vehicle_id;
        json::Value insights_arr(json::Array{});
        for (const auto& ins : result.insights) insights_arr.append(ins);
        r["insights"] = insights_arr;
        json::Value items_arr(json::Array{});
        for (const auto& item : result.items) {
            json::Value it(json::Object{});
            it["vehicle_id"] = item.vehicle_id;
            it["display_name"] = item.display_name;
            it["era"] = item.era == VehicleEra::ANCIENT ? "ancient" : "modern";
            it["protection_efficiency_score"] = item.protection_efficiency_score;
            it["weight_normalized_score"] = item.weight_normalized_score;
            it["cost_normalized_score"] = item.cost_normalized_score;
            it["overall_score"] = item.overall_score;
            it["rank"] = static_cast<int64_t>(item.rank);
            json::Value sim(json::Object{});
            sim["simulation_id"] = static_cast<int64_t>(item.simulation.simulation_id);
            sim["vehicle_id"] = static_cast<int64_t>(item.simulation.vehicle_id);
            sim["timestamp_ms"] = item.simulation.timestamp_ms;
            sim["roof_max_deformation_mm"] = item.simulation.roof_max_deformation_mm;
            sim["roof_plastic_strain"] = item.simulation.roof_plastic_strain;
            sim["roof_von_mises_stress_mpa"] = item.simulation.roof_von_mises_stress_mpa;
            sim["impact_energy_j"] = item.simulation.impact_energy_j;
            sim["absorbed_energy_j"] = item.simulation.absorbed_energy_j;
            sim["damage_level"] = static_cast<int64_t>(item.simulation.damage_level);
            sim["penetration_depth_mm"] = item.simulation.penetration_depth_mm;
            sim["is_penetrated"] = item.simulation.is_penetrated;
            sim["failure_mode"] = item.simulation.failure_mode;
            sim["strain_rate"] = item.simulation.strain_rate;
            sim["dynamic_yield_strength_mpa"] = item.simulation.dynamic_yield_strength_mpa;
            sim["temperature_K"] = item.simulation.temperature_K;
            sim["protection_material"] = item.simulation.protection_material;
            json::Value def_field(json::Array{});
            for (double v : item.simulation.deformation_field) def_field.append(v);
            sim["deformation_field"] = def_field;
            json::Value stress_field(json::Array{});
            for (double v : item.simulation.stress_field) stress_field.append(v);
            sim["stress_field"] = stress_field;
            it["simulation"] = sim;
            items_arr.append(it);
        }
        r["items"] = items_arr;
        json::Value wrapper(json::Object{});
        wrapper["data"] = r;
        return make_json_response(200, wrapper.dump());
    }

    if (path == "/api/formation/optimize") {
        if (!fo) return make_response(500, "text/plain", "FormationOptimizer not ready");
        json::Value root = json::parse(body);
        FormationOptimizationRequest req{};
        req.vehicle_count = 5;
        req.wall_height_m = 10.0;
        req.wall_length_m = 100.0;
        req.rock_fall_rate_per_sec = 2.0;
        req.avg_rock_mass_kg = 50.0;
        req.baseline.formation_type = "LINE";
        req.baseline.vehicle_count = 5;
        req.baseline.spacing_m = 3.0;
        req.baseline.attack_width_m = 0.0;
        req.baseline.wall_distance_m = 0.0;
        if (root.isObject()) {
            if (root.has("vehicle_count")) req.vehicle_count = root["vehicle_count"].asInt();
            if (root.has("wall_height_m")) req.wall_height_m = root["wall_height_m"].asDouble();
            if (root.has("wall_length_m")) req.wall_length_m = root["wall_length_m"].asDouble();
            if (root.has("rock_fall_rate_per_sec")) req.rock_fall_rate_per_sec = root["rock_fall_rate_per_sec"].asDouble();
            if (root.has("avg_rock_mass_kg")) req.avg_rock_mass_kg = root["avg_rock_mass_kg"].asDouble();
            if (root.has("baseline") && root["baseline"].isObject()) {
                auto& bl = root["baseline"];
                if (bl.has("formation_type")) req.baseline.formation_type = bl["formation_type"].asString();
                if (bl.has("spacing_m")) req.baseline.spacing_m = bl["spacing_m"].asDouble();
                if (bl.has("vehicle_count")) req.baseline.vehicle_count = bl["vehicle_count"].asInt();
            }
        }
        auto result = fo->optimize(req);
        json::Value r(json::Object{});
        r["optimization_id"] = static_cast<int64_t>(result.optimization_id);
        r["timestamp_ms"] = result.timestamp_ms;
        r["survival_probability"] = result.survival_probability;
        r["avg_coverage_score"] = result.avg_coverage_score;
        r["total_progress_rate"] = result.total_progress_rate;
        json::Value bf(json::Object{});
        bf["formation_type"] = result.best_formation.formation_type;
        bf["vehicle_count"] = static_cast<int64_t>(result.best_formation.vehicle_count);
        bf["spacing_m"] = result.best_formation.spacing_m;
        bf["attack_width_m"] = result.best_formation.attack_width_m;
        bf["wall_distance_m"] = result.best_formation.wall_distance_m;
        json::Value vtypes(json::Array{});
        for (const auto& vt : result.best_formation.vehicle_types) vtypes.append(vt);
        bf["vehicle_types"] = vtypes;
        r["best_formation"] = bf;
        json::Value cands(json::Array{});
        for (const auto& c : result.candidate_formations) {
            json::Value ci(json::Object{});
            ci["formation_type"] = c.formation_type;
            ci["vehicle_count"] = static_cast<int64_t>(c.vehicle_count);
            ci["spacing_m"] = c.spacing_m;
            ci["attack_width_m"] = c.attack_width_m;
            ci["wall_distance_m"] = c.wall_distance_m;
            json::Value cvts(json::Array{});
            for (const auto& vt : c.vehicle_types) cvts.append(vt);
            ci["vehicle_types"] = cvts;
            cands.append(ci);
        }
        r["candidate_formations"] = cands;
        json::Value recs(json::Array{});
        for (const auto& rec : result.recommendations) recs.append(rec);
        r["recommendations"] = recs;
        json::Value wrapper(json::Object{});
        wrapper["data"] = r;
        return make_json_response(200, wrapper.dump());
    }

    if (path == "/api/formation/layout") {
        if (!fo) return make_response(500, "text/plain", "FormationOptimizer not ready");
        json::Value root = json::parse(body);
        FormationConfig cfg{};
        cfg.formation_type = "LINE";
        cfg.vehicle_count = 5;
        cfg.spacing_m = 3.0;
        cfg.attack_width_m = 0.0;
        cfg.wall_distance_m = 0.0;
        if (root.isObject()) {
            if (root.has("formation_type")) cfg.formation_type = root["formation_type"].asString();
            if (root.has("vehicle_count")) cfg.vehicle_count = root["vehicle_count"].asInt();
            if (root.has("spacing_m")) cfg.spacing_m = root["spacing_m"].asDouble();
            if (root.has("attack_width_m")) cfg.attack_width_m = root["attack_width_m"].asDouble();
            if (root.has("wall_distance_m")) cfg.wall_distance_m = root["wall_distance_m"].asDouble();
            if (root.has("vehicle_types") && root["vehicle_types"].isArray()) {
                for (size_t i = 0; i < root["vehicle_types"].size(); ++i) {
                    cfg.vehicle_types.push_back(root["vehicle_types"][i].asString());
                }
            }
        }
        auto vehicles = fo->layout_formation(cfg);
        json::Value arr(json::Array{});
        for (const auto& v : vehicles) {
            json::Value item(json::Object{});
            item["vehicle_id"] = static_cast<int64_t>(v.vehicle_id);
            item["vehicle_type"] = v.vehicle_type;
            item["position_x"] = v.position_x;
            item["position_y"] = v.position_y;
            item["heading_deg"] = v.heading_deg;
            item["spacing_m"] = v.spacing_m;
            item["is_lead"] = v.is_lead;
            arr.append(item);
        }
        json::Value result(json::Object{});
        result["vehicles"] = arr;
        return make_json_response(200, result.dump());
    }

    if (path == "/api/user/session/create") {
        if (!usm) return make_response(500, "text/plain", "UserSessionManager not ready");
        json::Value root = json::parse(body);
        std::string nickname = "";
        std::string vehicle_type = "fenyun_basic";
        if (root.isObject()) {
            if (root.has("nickname")) nickname = root["nickname"].asString();
            if (root.has("vehicle_type")) vehicle_type = root["vehicle_type"].asString();
        }
        auto session = usm->create_session(nickname, vehicle_type);
        json::Value r(json::Object{});
        r["session_id"] = session.session_id;
        r["vehicle_type"] = session.vehicle_type;
        r["nickname"] = session.user_nickname;
        return make_json_response(200, r.dump());
    }

    if (path == "/api/user/session/action") {
        if (!usm) return make_response(500, "text/plain", "UserSessionManager not ready");
        json::Value root = json::parse(body);
        UserActionRequest req{};
        req.param1 = 0.0;
        req.param2 = 0.0;
        if (root.isObject()) {
            if (root.has("session_id")) req.session_id = root["session_id"].asString();
            if (root.has("action")) req.action = root["action"].asString();
            if (root.has("param1")) req.param1 = root["param1"].asDouble();
            if (root.has("param2")) req.param2 = root["param2"].asDouble();
        }
        if (req.session_id.empty() || !usm->has_session(req.session_id)) {
            return make_response(404, "text/plain", "Session not found");
        }
        auto st = usm->apply_action(req);
        json::Value item(json::Object{});
        item["session_id"] = st.session_id;
        item["position_x"] = st.position_x;
        item["position_y"] = st.position_y;
        item["heading_deg"] = st.heading_deg;
        item["speed_ms"] = st.speed_ms;
        item["health_percent"] = st.health_percent;
        item["armor_integrity_percent"] = st.armor_integrity_percent;
        item["impacts_received"] = static_cast<int64_t>(st.impacts_received);
        item["distance_traveled_m"] = st.distance_traveled_m;
        item["timestamp_ms"] = st.timestamp_ms;
        json::Value result(json::Object{});
        result["data"] = item;
        return make_json_response(200, result.dump());
    }

    if (path == "/api/user/session/attack") {
        if (!usm) return make_response(500, "text/plain", "UserSessionManager not ready");
        json::Value root = json::parse(body);
        std::string session_id = "";
        double force_multiplier = 1.0;
        if (root.isObject()) {
            if (root.has("session_id")) session_id = root["session_id"].asString();
            if (root.has("force_multiplier")) force_multiplier = root["force_multiplier"].asDouble();
        }
        if (session_id.empty() || !usm->has_session(session_id)) {
            return make_response(404, "text/plain", "Session not found");
        }
        auto attacks = usm->trigger_rock_attack(session_id, force_multiplier);
        auto st = usm->get_vehicle_state(session_id);
        json::Value attacks_arr(json::Array{});
        for (const auto& a : attacks) {
            json::Value item(json::Object{});
            item["event_id"] = static_cast<int64_t>(a.event_id);
            item["impact_x"] = a.impact_x;
            item["impact_y"] = a.impact_y;
            item["rock_mass_kg"] = a.rock_mass_kg;
            item["rock_velocity_ms"] = a.rock_velocity_ms;
            item["damage_dealt"] = a.damage_dealt;
            item["timestamp_ms"] = a.timestamp_ms;
            attacks_arr.append(item);
        }
        json::Value state_obj(json::Object{});
        state_obj["session_id"] = st.session_id;
        state_obj["position_x"] = st.position_x;
        state_obj["position_y"] = st.position_y;
        state_obj["heading_deg"] = st.heading_deg;
        state_obj["speed_ms"] = st.speed_ms;
        state_obj["health_percent"] = st.health_percent;
        state_obj["armor_integrity_percent"] = st.armor_integrity_percent;
        state_obj["impacts_received"] = static_cast<int64_t>(st.impacts_received);
        state_obj["distance_traveled_m"] = st.distance_traveled_m;
        state_obj["timestamp_ms"] = st.timestamp_ms;
        json::Value result(json::Object{});
        result["attacks"] = attacks_arr;
        result["state"] = state_obj;
        return make_json_response(200, result.dump());
    }

    if (path == "/api/user/session/tick") {
        if (!usm) return make_response(500, "text/plain", "UserSessionManager not ready");
        json::Value root = json::parse(body);
        std::string session_id = "";
        double dt_seconds = 1.0;
        if (root.isObject()) {
            if (root.has("session_id")) session_id = root["session_id"].asString();
            if (root.has("dt_seconds")) dt_seconds = root["dt_seconds"].asDouble();
        }
        if (session_id.empty() || !usm->has_session(session_id)) {
            return make_response(404, "text/plain", "Session not found");
        }
        auto attacks = usm->process_time_step(session_id, dt_seconds);
        auto st = usm->get_vehicle_state(session_id);
        json::Value attacks_arr(json::Array{});
        for (const auto& a : attacks) {
            json::Value item(json::Object{});
            item["event_id"] = static_cast<int64_t>(a.event_id);
            item["impact_x"] = a.impact_x;
            item["impact_y"] = a.impact_y;
            item["rock_mass_kg"] = a.rock_mass_kg;
            item["rock_velocity_ms"] = a.rock_velocity_ms;
            item["damage_dealt"] = a.damage_dealt;
            item["timestamp_ms"] = a.timestamp_ms;
            attacks_arr.append(item);
        }
        json::Value state_obj(json::Object{});
        state_obj["session_id"] = st.session_id;
        state_obj["position_x"] = st.position_x;
        state_obj["position_y"] = st.position_y;
        state_obj["heading_deg"] = st.heading_deg;
        state_obj["speed_ms"] = st.speed_ms;
        state_obj["health_percent"] = st.health_percent;
        state_obj["armor_integrity_percent"] = st.armor_integrity_percent;
        state_obj["impacts_received"] = static_cast<int64_t>(st.impacts_received);
        state_obj["distance_traveled_m"] = st.distance_traveled_m;
        state_obj["timestamp_ms"] = st.timestamp_ms;
        json::Value result(json::Object{});
        result["attacks"] = attacks_arr;
        result["state"] = state_obj;
        return make_json_response(200, result.dump());
    }

    if (path == "/api/user/session/reset") {
        if (!usm) return make_response(500, "text/plain", "UserSessionManager not ready");
        json::Value root = json::parse(body);
        std::string session_id = "";
        if (root.isObject()) {
            if (root.has("session_id")) session_id = root["session_id"].asString();
        }
        if (session_id.empty() || !usm->has_session(session_id)) {
            return make_response(404, "text/plain", "Session not found");
        }
        usm->reset_session(session_id);
        json::Value r(json::Object{});
        r["status"] = "ok";
        return make_json_response(200, r.dump());
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
