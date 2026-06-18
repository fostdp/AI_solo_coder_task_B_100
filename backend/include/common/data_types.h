#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <chrono>
#include <optional>
#include <functional>

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
    std::string display_name;
    double density;
    double youngs_modulus_gpa;
    double poisson_ratio;
    double yield_strength_mpa;
    double ultimate_strength_mpa;
    double toughness_mj_m3;
    double specific_energy_absorption_kj_kg;
    double cost_per_unit;
    double durability_base;

    double youngs_modulus_pa() const { return youngs_modulus_gpa * 1e9; }
    double yield_strength_pa() const { return yield_strength_mpa * 1e6; }
    double ultimate_strength_pa() const { return ultimate_strength_mpa * 1e6; }
    double toughness_j_m3() const { return toughness_mj_m3 * 1e6; }
};

struct JohnsonCookParams {
    double A;
    double B;
    double n;
    double C;
    double m;
    double T_melt;
    double T_ref;
    double eps_dot_0;
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

enum class VehicleEra : uint8_t { ANCIENT = 0, MODERN = 1 };
enum class VehicleType : uint8_t {
    FENYUN = 0,
    CHONGCHE = 1,
    YUNTI = 2,
    MODERN_APC = 3,
    MODERN_TANK = 4,
    MODERN_IFV = 5
};

struct VehicleProfile {
    std::string id;
    std::string display_name;
    std::string description;
    VehicleEra era;
    VehicleType type;
    double length_m;
    double width_m;
    double height_m;
    double weight_ton;
    int crew_count;
    double max_speed_kmh;
    double roof_thickness_mm;
    double wall_thickness_mm;
    std::string primary_material;
    std::vector<std::string> available_materials;
    double protection_area_m2;
    int historical_year;
    std::string origin;
};

struct VehicleComparisonRequest {
    std::vector<std::string> vehicle_ids;
    double rock_mass_kg;
    double rock_velocity_ms;
    double impact_location_x;
    double impact_location_y;
    double temperature_K;
    bool use_johnson_cook = true;
};

struct VehicleComparisonItem {
    std::string vehicle_id;
    std::string display_name;
    VehicleEra era;
    SimulationResult simulation;
    double protection_efficiency_score;
    double weight_normalized_score;
    double cost_normalized_score;
    double overall_score;
    int rank;
};

struct VehicleComparisonResult {
    uint64_t comparison_id;
    int64_t timestamp_ms;
    VehicleComparisonRequest request;
    std::vector<VehicleComparisonItem> items;
    std::string best_vehicle_id;
    std::vector<std::string> insights;
};

struct FormationVehicle {
    uint32_t vehicle_id;
    std::string vehicle_type;
    double position_x;
    double position_y;
    double heading_deg;
    double spacing_m;
    bool is_lead;
};

struct FormationConfig {
    std::string formation_type;
    int vehicle_count;
    double spacing_m;
    double attack_width_m;
    double wall_distance_m;
    std::vector<std::string> vehicle_types;
};

struct FormationOptimizationRequest {
    int vehicle_count;
    double wall_height_m;
    double wall_length_m;
    double rock_fall_rate_per_sec;
    double avg_rock_mass_kg;
    FormationConfig baseline;
};

struct FormationOptimizationResult {
    uint64_t optimization_id;
    int64_t timestamp_ms;
    FormationConfig best_formation;
    double survival_probability;
    double avg_coverage_score;
    double total_progress_rate;
    std::vector<FormationConfig> candidate_formations;
    std::vector<std::string> recommendations;
};

struct UserSession {
    std::string session_id;
    int64_t created_ms;
    int64_t last_active_ms;
    std::string user_nickname;
    uint32_t current_vehicle_id;
    std::string vehicle_type;
};

struct UserVehicleState {
    std::string session_id;
    double position_x;
    double position_y;
    double heading_deg;
    double speed_ms;
    double health_percent;
    double armor_integrity_percent;
    int impacts_received;
    double distance_traveled_m;
    int64_t timestamp_ms;
};

struct UserActionRequest {
    std::string session_id;
    std::string action;
    double param1;
    double param2;
};

struct RockAttackEvent {
    uint64_t event_id;
    std::string session_id;
    double impact_x;
    double impact_y;
    double rock_mass_kg;
    double rock_velocity_ms;
    double damage_dealt;
    int64_t timestamp_ms;
};

using SensorCallback = std::function<void(const SensorData&)>;
using SimulationCallback = std::function<void(const SimulationResult&)>;
using AlertCallback = std::function<void(const AlertRecord&)>;
using EvaluationCallback = std::function<void(const std::vector<ProtectionEvaluation>&)>;

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
