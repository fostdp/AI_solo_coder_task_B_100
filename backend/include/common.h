#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <chrono>
#include <optional>

namespace fenyun {

constexpr double PI = 3.14159265358979323846;
constexpr double GRAVITY = 9.81;
constexpr double DEFAULT_ROOF_THICKNESS = 0.08;
constexpr double DEFAULT_DEFORMATION_THRESHOLD = 15.0;
constexpr double DEFAULT_PENETRATION_RATIO = 0.9;

struct SensorData {
    uint32_t vehicle_id;
    int64_t timestamp_ms;
    double roof_stress;
    double wheel_deformation;
    double rock_impact_force;
    double protection_thickness;
    std::string protection_material;
    double ambient_temp;
    double impact_location_x;
    double impact_location_y;
    double rock_mass;
    double rock_velocity;
};

struct MaterialProperties {
    std::string name;
    double density;
    double youngs_modulus_gpa;
    double poisson_ratio;
    double yield_strength_mpa;
    double ultimate_strength_mpa;
    double toughness_mj_m3;
    double specific_energy_absorption_kj_kg;
    double cost_per_unit;

    double youngs_modulus_pa() const { return youngs_modulus_gpa * 1e9; }
    double yield_strength_pa() const { return yield_strength_mpa * 1e6; }
    double ultimate_strength_pa() const { return ultimate_strength_mpa * 1e6; }
    double toughness_j_m3() const { return toughness_mj_m3 * 1e6; }
};

struct SimulationResult {
    uint64_t simulation_id;
    uint32_t vehicle_id;
    int64_t timestamp_ms;
    double roof_max_deformation_mm;
    double roof_plastic_strain;
    double roof_von_mises_stress_mpa;
    double impact_energy_j;
    double absorbed_energy_j;
    uint8_t damage_level;
    double penetration_depth_mm;
    bool is_penetrated;
    std::string failure_mode;
    std::vector<double> deformation_field;
    std::vector<double> stress_field;
    double strain_rate;
    double dynamic_yield_strength_mpa;
    double temperature_K;
    std::string protection_material;
};

struct AlertRecord {
    uint64_t alert_id;
    uint32_t vehicle_id;
    int64_t timestamp_ms;
    std::string alert_type;
    uint8_t alert_level;
    std::string alert_message;
    double measured_value;
    double threshold_value;
    std::string mqtt_topic;
    std::string mqtt_message_id;
};

struct ProtectionEvaluation {
    uint64_t eval_id;
    uint32_t vehicle_id;
    int64_t timestamp_ms;
    std::string material_type;
    double material_thickness_mm;
    double energy_absorption_score;
    double structural_strength_score;
    double weight_factor_score;
    double cost_factor_score;
    double durability_score;
    double ahp_weight_score;
    uint8_t rank_position;
    bool is_recommended;
};

inline int64_t current_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline std::string timestamp_ms_to_string(int64_t ts_ms) {
    auto seconds = static_cast<time_t>(ts_ms / 1000);
    auto millis = static_cast<int>(ts_ms % 1000);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&seconds));
    std::snprintf(buf + std::strlen(buf), sizeof(buf) - std::strlen(buf), ".%03d", millis);
    return std::string(buf);
}

}
