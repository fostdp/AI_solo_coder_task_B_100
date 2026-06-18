#include "config/config_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

namespace fenyun {

ConfigLoader::ConfigLoader() = default;
ConfigLoader::~ConfigLoader() = default;

std::string read_file_content(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool ConfigLoader::load_from_file(const std::string& system_path,
                                   const std::string& materials_path,
                                   const std::string& ahp_path,
                                   const std::string& vehicles_path) {
    try {
        std::string sys_str = read_file_content(system_path);
        std::string mat_str = read_file_content(materials_path);
        std::string ahp_str = read_file_content(ahp_path);
        std::string veh_str;
        if (!vehicles_path.empty()) {
            veh_str = read_file_content(vehicles_path);
        }
        return load_from_string(sys_str, mat_str, ahp_str, veh_str);
    } catch (const std::exception& e) {
        std::cerr << "[Config] Load error: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigLoader::load_from_string(const std::string& system_json,
                                     const std::string& materials_json,
                                     const std::string& ahp_json,
                                     const std::string& vehicles_json) {
    try {
        json::Value sys_root = json::parse(system_json);
        json::Value mat_root = json::parse(materials_json);
        json::Value ahp_root = json::parse(ahp_json);

        if (sys_root.isNull() || mat_root.isNull() || ahp_root.isNull()) {
            std::cerr << "[Config] JSON parse failed" << std::endl;
            return false;
        }

        bool ok = true;
        ok &= parse_system_config(sys_root);
        ok &= parse_materials_config(mat_root);
        ok &= parse_ahp_config(ahp_root);

        if (!vehicles_json.empty()) {
            json::Value veh_root = json::parse(vehicles_json);
            if (veh_root.isNull()) {
                std::cerr << "[Config] Vehicles JSON parse failed" << std::endl;
                ok = false;
            } else {
                ok &= parse_vehicles_config(veh_root);
            }
        }

        loaded_ = ok;
        return ok;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Load exception: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigLoader::parse_system_config(const json::Value& root) {
    system_.version = root["version"].asString();

    const json::Value& http = root["http"];
    system_.http.host = http["host"].asString();
    system_.http.port = static_cast<uint16_t>(http["port"].asInt());
    system_.http.max_request_size_bytes =
        static_cast<size_t>(http["max_request_size_bytes"].asInt());

    const json::Value& ch = root["clickhouse"];
    system_.clickhouse.host = ch["host"].asString();
    system_.clickhouse.http_port = static_cast<uint16_t>(ch["http_port"].asInt());
    system_.clickhouse.tcp_port = static_cast<uint16_t>(ch["tcp_port"].asInt());
    system_.clickhouse.user = ch["user"].asString();
    system_.clickhouse.password = ch["password"].asString();
    system_.clickhouse.database = ch["database"].asString();
    system_.clickhouse.connection_timeout_ms = ch["connection_timeout_ms"].asInt();

    const json::Value& mq = root["mqtt"];
    system_.mqtt.broker_url = mq["broker_url"].asString();
    system_.mqtt.client_id = mq["client_id"].asString();
    system_.mqtt.username = mq["username"].asString();
    system_.mqtt.password = mq["password"].asString();
    system_.mqtt.qos = mq["qos"].asInt();
    system_.mqtt.topic_prefix = mq["topic_prefix"].asString();

    const json::Value& sim = root["simulation"];
    system_.simulation.threads = sim["threads"].asInt();
    system_.simulation.queue_capacity = static_cast<size_t>(sim["queue_capacity"].asInt());
    system_.simulation.default_roof_length_m = sim["default_roof_length_m"].asDouble();
    system_.simulation.default_roof_width_m = sim["default_roof_width_m"].asDouble();
    system_.simulation.default_grid_size = sim["default_grid_size"].asInt();
    system_.simulation.jc_bisection_iterations = sim["jc_plastic_strain_bisection_iterations"].asInt();

    const json::Value& al = root["alarm"];
    system_.alarm.threads = al["threads"].asInt();
    system_.alarm.queue_capacity = static_cast<size_t>(al["queue_capacity"].asInt());
    system_.alarm.deformation_threshold_mm = al["deformation_threshold_mm"].asDouble();
    system_.alarm.penetration_threshold_ratio = al["penetration_threshold_ratio"].asDouble();
    system_.alarm.stress_threshold_mpa = al["stress_threshold_mpa"].asDouble();
    system_.alarm.deformation_warn_ratio = al["deformation_warn_ratio"].asDouble();

    const json::Value& opt = root["protection_optimizer"];
    system_.optimizer.queue_capacity = static_cast<size_t>(opt["queue_capacity"].asInt());
    system_.optimizer.threads = opt["threads"].asInt();
    system_.optimizer.auto_reevaluate_seconds = opt["auto_reevaluate_seconds"].asInt();

    const json::Value& dtu = root["dtu_receiver"];
    system_.dtu.report_interval_seconds = dtu["report_interval_seconds"].asInt();
    system_.dtu.vehicle_count = dtu["vehicle_count"].asInt();
    system_.dtu.default_material = dtu["default_material"].asString();
    system_.dtu.default_protection_thickness_mm = dtu["default_protection_thickness_mm"].asDouble();

    return true;
}

bool ConfigLoader::parse_materials_config(const json::Value& root) {
    materials_.clear();
    jc_params_.clear();

    const json::Value& mats = root["materials"];
    if (!mats.isObject()) return false;

    for (const auto& [name, mat_val] : mats.asObject()) {
        MaterialProperties prop;
        prop.name = name;
        prop.display_name = mat_val["display_name"].asString();
        prop.density = mat_val["density"].asDouble();
        prop.youngs_modulus_gpa = mat_val["youngs_modulus_gpa"].asDouble();
        prop.poisson_ratio = mat_val["poisson_ratio"].asDouble();
        prop.yield_strength_mpa = mat_val["yield_strength_mpa"].asDouble();
        prop.ultimate_strength_mpa = mat_val["ultimate_strength_mpa"].asDouble();
        prop.toughness_mj_m3 = mat_val["toughness_mj_m3"].asDouble();
        prop.specific_energy_absorption_kj_kg =
            mat_val["specific_energy_absorption_kj_kg"].asDouble();
        prop.cost_per_unit = mat_val["cost_per_unit"].asDouble();
        prop.durability_base = mat_val["durability_base"].asDouble();
        materials_[name] = prop;

        const json::Value& jc_val = mat_val["johnson_cook"];
        JohnsonCookParams jc;
        jc.A = jc_val["A_pa"].asDouble();
        jc.B = jc_val["B_pa"].asDouble();
        jc.n = jc_val["n"].asDouble();
        jc.C = jc_val["C"].asDouble();
        jc.m = jc_val["m"].asDouble();
        jc.T_melt = jc_val["T_melt_K"].asDouble();
        jc.T_ref = jc_val["T_ref_K"].asDouble();
        jc.eps_dot_0 = jc_val["eps_dot_0"].asDouble();
        jc_params_[name] = jc;
    }

    return !materials_.empty();
}

bool ConfigLoader::parse_ahp_config(const json::Value& root) {
    ahp_.version = root["version"].asString();

    const json::Value& crit = root["criteria"];
    ahp_.criteria.clear();
    for (size_t i = 0; i < crit.size(); ++i) {
        ahp_.criteria.push_back(crit[i].asString());
    }

    const json::Value& disp = root["criteria_display"];
    ahp_.criteria_display.clear();
    for (const auto& [k, v] : disp.asObject()) {
        ahp_.criteria_display[k] = v.asString();
    }

    const json::Value& matrix = root["reference_pairwise_matrix"];
    ahp_.reference_pairwise_matrix.clear();
    for (size_t i = 0; i < matrix.size(); ++i) {
        std::vector<double> row;
        const json::Value& row_v = matrix[i];
        for (size_t j = 0; j < row_v.size(); ++j) {
            row.push_back(row_v[j].asDouble());
        }
        ahp_.reference_pairwise_matrix.push_back(std::move(row));
    }

    const json::Value& c = root["consistency"];
    ahp_.consistency.cr_threshold = c["cr_threshold"].asDouble();
    ahp_.consistency.auto_correction = c["auto_correction"].asBool();
    ahp_.consistency.auto_correction_cr_target = c["auto_correction_cr_target"].asDouble();
    ahp_.consistency.max_correction_iterations = c["max_correction_iterations"].asInt();
    ahp_.consistency.correction_step_ratio = c["correction_step_ratio"].asDouble();

    const json::Value& g = root["group_decision"];
    ahp_.group_decision.enabled = g["enabled"].asBool();
    ahp_.group_decision.default_expert_count = g["default_expert_count"].asInt();
    ahp_.group_decision.max_expert_count = g["max_expert_count"].asInt();
    ahp_.group_decision.expert_divergence = g["expert_divergence"].asDouble();
    ahp_.group_decision.consensus_threshold = g["consensus_threshold"].asDouble();
    ahp_.group_decision.aggregation_method = g["aggregation_method"].asString();

    const json::Value& pool = g["experts_pool"];
    ahp_.group_decision.experts_pool.clear();
    for (size_t i = 0; i < pool.size(); ++i) {
        AHPConfig::ExpertPoolEntry e;
        e.name = pool[i]["name"].asString();
        e.title = pool[i]["title"].asString();
        e.authority_weight = pool[i]["authority_weight"].asDouble();
        ahp_.group_decision.experts_pool.push_back(std::move(e));
    }

    const json::Value& schemes = root["evaluation_schemes"];
    ahp_.evaluation_schemes.clear();
    for (size_t i = 0; i < schemes.size(); ++i) {
        AHPConfig::EvaluationScheme s;
        s.material_type = schemes[i]["material_type"].asString();
        s.thickness_mm = schemes[i]["thickness_mm"].asDouble();
        ahp_.evaluation_schemes.push_back(std::move(s));
    }

    return !ahp_.criteria.empty() && !ahp_.reference_pairwise_matrix.empty();
}

MaterialProperties ConfigLoader::get_material(const std::string& name) const {
    auto it = materials_.find(name);
    if (it != materials_.end()) return it->second;
    if (!materials_.empty()) return materials_.begin()->second;
    return MaterialProperties{};
}

JohnsonCookParams ConfigLoader::get_jc_params(const std::string& material_name) const {
    auto it = jc_params_.find(material_name);
    if (it != jc_params_.end()) return it->second;
    if (!jc_params_.empty()) return jc_params_.begin()->second;
    return JohnsonCookParams{};
}

bool ConfigLoader::has_material(const std::string& name) const {
    return materials_.find(name) != materials_.end();
}

VehicleEra ConfigLoader::era_from_string(const std::string& s) const {
    if (s == "ancient") return VehicleEra::ANCIENT;
    if (s == "modern") return VehicleEra::MODERN;
    return VehicleEra::ANCIENT;
}

VehicleType ConfigLoader::type_from_string(const std::string& s) const {
    if (s == "FENYUN") return VehicleType::FENYUN;
    if (s == "CHONGCHE") return VehicleType::CHONGCHE;
    if (s == "YUNTI") return VehicleType::YUNTI;
    if (s == "MODERN_APC") return VehicleType::MODERN_APC;
    if (s == "MODERN_TANK") return VehicleType::MODERN_TANK;
    if (s == "MODERN_IFV") return VehicleType::MODERN_IFV;
    return VehicleType::FENYUN;
}

bool ConfigLoader::parse_vehicles_config(const json::Value& root) {
    vehicles_.clear();

    const json::Value& vehs = root["vehicles"];
    if (!vehs.isObject()) return false;

    for (const auto& [id, veh_val] : vehs.asObject()) {
        VehicleProfile vp;
        vp.id = id;
        vp.display_name = veh_val["display_name"].asString();
        vp.description = veh_val["description"].asString();
        vp.era = era_from_string(veh_val["era"].asString());
        vp.type = type_from_string(veh_val["type"].asString());
        vp.length_m = veh_val["length_m"].asDouble();
        vp.width_m = veh_val["width_m"].asDouble();
        vp.height_m = veh_val["height_m"].asDouble();
        vp.weight_ton = veh_val["weight_ton"].asDouble();
        vp.crew_count = veh_val["crew_count"].asInt();
        vp.max_speed_kmh = veh_val["max_speed_kmh"].asDouble();
        vp.roof_thickness_mm = veh_val["roof_thickness_mm"].asDouble();
        vp.wall_thickness_mm = veh_val["wall_thickness_mm"].asDouble();
        vp.primary_material = veh_val["primary_material"].asString();

        const json::Value& am = veh_val["available_materials"];
        vp.available_materials.clear();
        for (size_t i = 0; i < am.size(); ++i) {
            vp.available_materials.push_back(am[i].asString());
        }

        vp.protection_area_m2 = veh_val["protection_area_m2"].asDouble();
        vp.historical_year = veh_val["historical_year"].asInt();
        vp.origin = veh_val["origin"].asString();

        if (veh_val.has("literature_source") && !veh_val["literature_source"].isNull()) {
            vp.literature_source = veh_val["literature_source"].asString();
        }
        if (veh_val.has("unit_conversion_note") && !veh_val["unit_conversion_note"].isNull()) {
            vp.unit_conversion_note = veh_val["unit_conversion_note"].asString();
        }
        if (veh_val.has("stanag_equivalent_level") && !veh_val["stanag_equivalent_level"].isNull()) {
            vp.stanag_equivalent_level = veh_val["stanag_equivalent_level"].asInt();
        }
        if (veh_val.has("rha_equivalent_mm") && !veh_val["rha_equivalent_mm"].isNull()) {
            vp.rha_equivalent_mm = veh_val["rha_equivalent_mm"].asDouble();
        }
        if (veh_val.has("ground_pressure_kpa") && !veh_val["ground_pressure_kpa"].isNull()) {
            vp.ground_pressure_kpa = veh_val["ground_pressure_kpa"].asDouble();
        } else {
            double contact_area = vp.length_m * vp.width_m * 0.75;
            if (contact_area > 1e-6) {
                vp.ground_pressure_kpa = (vp.weight_ton * 1000.0 * 9.8 / contact_area) / 1000.0;
            } else {
                vp.ground_pressure_kpa = (vp.era == VehicleEra::ANCIENT) ? 100.0 : 350.0;
            }
        }
        if (veh_val.has("climb_grade_deg") && !veh_val["climb_grade_deg"].isNull()) {
            vp.climb_grade_deg = veh_val["climb_grade_deg"].asDouble();
        } else {
            vp.climb_grade_deg = (vp.era == VehicleEra::ANCIENT) ? 12.5 : 45.0;
        }
        if (veh_val.has("trench_crossing_m") && !veh_val["trench_crossing_m"].isNull()) {
            vp.trench_crossing_m = veh_val["trench_crossing_m"].asDouble();
        } else {
            vp.trench_crossing_m = (vp.era == VehicleEra::ANCIENT) ? (vp.width_m * 0.7) : (vp.length_m * 0.5);
        }

        vehicles_[id] = std::move(vp);
    }

    return !vehicles_.empty();
}

VehicleProfile ConfigLoader::get_vehicle(const std::string& id) const {
    auto it = vehicles_.find(id);
    if (it != vehicles_.end()) return it->second;
    if (!vehicles_.empty()) return vehicles_.begin()->second;
    return VehicleProfile{};
}

std::vector<VehicleProfile> ConfigLoader::get_vehicles_by_era(VehicleEra era) const {
    std::vector<VehicleProfile> result;
    for (const auto& [id, vp] : vehicles_) {
        if (vp.era == era) {
            result.push_back(vp);
        }
    }
    return result;
}

std::vector<VehicleProfile> ConfigLoader::get_vehicles_by_type(VehicleType type) const {
    std::vector<VehicleProfile> result;
    for (const auto& [id, vp] : vehicles_) {
        if (vp.type == type) {
            result.push_back(vp);
        }
    }
    return result;
}

bool ConfigLoader::has_vehicle(const std::string& id) const {
    return vehicles_.find(id) != vehicles_.end();
}

std::shared_ptr<ConfigLoader> create_default_config_loader() {
    return std::make_shared<ConfigLoader>();
}

}
