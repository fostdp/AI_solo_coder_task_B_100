#include "structural_simulation.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>

namespace fenyun {

StructuralSimulation::StructuralSimulation() {
    material_db_["cowhide"] = {
        "cowhide", 860.0, 0.15, 0.40, 25.0, 60.0, 15.0, 80.0, 3.0
    };
    material_db_["wood"] = {
        "wood", 650.0, 10.0, 0.35, 60.0, 120.0, 10.0, 50.0, 1.0
    };
    material_db_["iron"] = {
        "iron", 7850.0, 206.0, 0.29, 235.0, 400.0, 80.0, 200.0, 10.0
    };
    material_db_["composite"] = {
        "composite", 900.0, 15.0, 0.33, 120.0, 250.0, 45.0, 300.0, 5.0
    };

    jc_params_db_["cowhide"] = {
        25.0e6,   // A: 静态屈服
        80.0e6,   // B: 硬化系数
        0.25,     // n: 硬化指数
        0.04,     // C: 应变率敏感性
        0.8,      // m: 热软化
        600.0,    // Tmelt: K
        298.15,   // Tref:  K
        1.0       // eps0:  1/s
    };
    jc_params_db_["wood"] = {
        60.0e6,
        120.0e6,
        0.45,
        0.06,
        1.0,
        550.0,
        298.15,
        1.0
    };
    jc_params_db_["iron"] = {
        235.0e6,
        380.0e6,
        0.32,
        0.022,
        0.55,
        1811.0,
        298.15,
        1.0
    };
    jc_params_db_["composite"] = {
        120.0e6,
        220.0e6,
        0.35,
        0.018,
        0.70,
        900.0,
        298.15,
        1.0
    };
}

void StructuralSimulation::set_roof_dimensions(double length_m, double width_m, double thickness_m) {
    roof_length_m_ = length_m;
    roof_width_m_ = width_m;
    roof_thickness_m_ = thickness_m;
}

void StructuralSimulation::set_deformation_threshold(double mm) {
    deformation_threshold_mm_ = mm;
}

void StructuralSimulation::set_penetration_threshold(double ratio) {
    penetration_threshold_ratio_ = ratio;
}

void StructuralSimulation::set_strain_rate(double eps_dot) {
    strain_rate_ = std::max(eps_dot, 1e-6);
}

void StructuralSimulation::set_temperature(double ambient_K) {
    temperature_K_ = ambient_K;
}

const JohnsonCookParams& StructuralSimulation::get_jc_params(const std::string& material) const {
    static JohnsonCookParams fallback{200e6, 400e6, 0.3, 0.02, 0.6, 1500, 298.15, 1.0};
    auto it = jc_params_db_.find(material);
    return it != jc_params_db_.end() ? it->second : fallback;
}

bool StructuralSimulation::has_jc_params(const std::string& material) const {
    return jc_params_db_.find(material) != jc_params_db_.end();
}

MaterialProperties StructuralSimulation::get_material_properties(const std::string& material_name) const {
    auto it = material_db_.find(material_name);
    if (it != material_db_.end()) return it->second;
    return material_db_.at("wood");
}

double StructuralSimulation::calc_impact_energy(double mass_kg, double velocity_m_s) const {
    return 0.5 * mass_kg * velocity_m_s * velocity_m_s;
}

double StructuralSimulation::calc_contact_force(double impact_energy_j, double contact_area_m2,
                                                  double impactor_radius_m) const {
    double impulse = std::sqrt(2.0 * impact_energy_j * 100.0);
    double contact_time = 0.001 + 0.005 * (1.0 - std::exp(-impactor_radius_m / 0.1));
    return impulse / contact_time;
}

double StructuralSimulation::calc_strain_rate(double impact_velocity_m_s, double thickness_m) const {
    return impact_velocity_m_s / std::max(thickness_m, 1e-6);
}

double StructuralSimulation::calc_bending_deformation(double force_n, double length_m, double width_m,
                                                        double thickness_m, double E_pa, double nu) const {
    double D = E_pa * std::pow(thickness_m, 3) / (12.0 * (1.0 - nu * nu));
    double pressure = force_n / (length_m * width_m);
    double max_deflection = 0.004 * pressure * std::pow(length_m, 4) / D;
    return std::min(max_deflection * 1000.0, thickness_m * 1000.0 * 0.5);
}

// ============================================================
// Johnson-Cook 高应变率本构模型
// σ_y(εp, ε̇, T) = (A + B·εpⁿ) · [1 + C·ln(ε̇/ε̇₀)] · [1 - T*ᵐ]
// 其中 T* = (T - Tref) / (Tmelt - Tref)
// ============================================================
double StructuralSimulation::calc_johnson_cook_flow_stress(const MaterialProperties& mat,
                                                             const JohnsonCookParams& jc,
                                                             double plastic_strain,
                                                             double strain_rate,
                                                             double temperature_K) const {
    (void)mat;

    double eps_p = std::max(plastic_strain, 1e-6);

    double strain_hardening = jc.A + jc.B * std::pow(eps_p, jc.n);

    double strain_rate_ratio = std::max(strain_rate / std::max(jc.eps_dot_0, 1e-6), 1.0);
    double rate_sensitivity = 1.0 + jc.C * std::log(strain_rate_ratio);

    double T_eff = std::max(temperature_K, jc.T_ref);
    T_eff = std::min(T_eff, jc.T_melt - 1.0);
    double T_star = (T_eff - jc.T_ref) / std::max(jc.T_melt - jc.T_ref, 1.0);
    T_star = std::max(0.0, std::min(T_star, 0.99));
    double thermal_softening = 1.0 - std::pow(T_star, jc.m);

    return strain_hardening * rate_sensitivity * thermal_softening;
}

// ============================================================
// 基于 JC 流应力的塑性应变反解 (二分法迭代)
// σ_vm = JC(εp)  → 求解 εp
// ============================================================
double StructuralSimulation::calc_plastic_strain_jc(double von_mises_pa,
                                                      const MaterialProperties& mat,
                                                      const JohnsonCookParams& jc,
                                                      double strain_rate,
                                                      double temperature_K,
                                                      double E_pa) const {
    double sigma_y0 = jc.A;
    if (von_mises_pa <= sigma_y0) {
        return 0.0;
    }

    double sigma_eff_static = von_mises_pa;
    {
        double sr_ratio = std::max(strain_rate / std::max(jc.eps_dot_0, 1e-6), 1.0);
        double rate_factor = 1.0 + jc.C * std::log(sr_ratio);
        double T_eff = std::max(temperature_K, jc.T_ref);
        T_eff = std::min(T_eff, jc.T_melt - 1.0);
        double T_star = (T_eff - jc.T_ref) / std::max(jc.T_melt - jc.T_ref, 1.0);
        T_star = std::max(0.0, std::min(T_star, 0.99));
        double thermal_factor = 1.0 - std::pow(T_star, jc.m);
        double total_factor = std::max(rate_factor * thermal_factor, 1e-6);
        sigma_eff_static = von_mises_pa / total_factor;
    }

    if (sigma_eff_static <= jc.A) {
        double elastic_strain = von_mises_pa / std::max(E_pa, 1.0);
        return std::max(0.0, elastic_strain - jc.A / std::max(E_pa, 1.0));
    }

    double target = sigma_eff_static - jc.A;
    double lo = 1e-6, hi = 0.5;
    for (int iter = 0; iter < 60; ++iter) {
        double mid = (lo + hi) * 0.5;
        double f = jc.B * std::pow(mid, jc.n) - target;
        if (std::abs(f) < 1e-3) {
            return mid;
        }
        if (f < 0) lo = mid;
        else hi = mid;
    }
    return (lo + hi) * 0.5;
}

double StructuralSimulation::calc_von_mises_stress(double force_n, double contact_area_m2,
                                                      double bending_stress_pa) const {
    double contact_stress = force_n / std::max(contact_area_m2, 1e-6);
    double sigma_x = bending_stress_pa;
    double sigma_y = 0.3 * bending_stress_pa;
    double sigma_z = -contact_stress;
    double tau_xy = 0.1 * bending_stress_pa;

    double von_mises = std::sqrt(
        0.5 * (std::pow(sigma_x - sigma_y, 2) +
               std::pow(sigma_y - sigma_z, 2) +
               std::pow(sigma_z - sigma_x, 2) +
               6.0 * (tau_xy * tau_xy))
    );
    return von_mises;
}

// ============================================================
// 基于 JC 动态屈服面的侵彻深度模型
// 考虑应变率强化效应，高应变率下材料承载力提升
// ============================================================
double StructuralSimulation::calc_penetration_depth_jc(double impact_energy_j,
                                                         const MaterialProperties& mat,
                                                         const JohnsonCookParams& jc,
                                                         double thickness_m,
                                                         double contact_area_m2,
                                                         double strain_rate,
                                                         double temperature_K) const {
    double dynamic_flow_stress = calc_johnson_cook_flow_stress(mat, jc, 0.05, strain_rate, temperature_K);
    double dynamic_toughness = 0.7 * mat.toughness_j_m3() * (dynamic_flow_stress / std::max(mat.yield_strength_pa(), 1.0));
    double volume = contact_area_m2 * thickness_m;
    double max_absorbable = dynamic_toughness * volume;
    double ratio = impact_energy_j / std::max(max_absorbable, 1.0);
    ratio = std::min(ratio, 1.5);
    return ratio * thickness_m * 1000.0;
}

double StructuralSimulation::calc_absorbed_energy(double impact_energy_j,
                                                    const MaterialProperties& mat,
                                                    double thickness_m,
                                                    double area_m2) const {
    double volume = area_m2 * thickness_m;
    double max_absorbable = mat.toughness_j_m3() * volume;
    double elastic_energy = 0.5 * impact_energy_j * std::exp(-impact_energy_j / (2.0 * max_absorbable));
    double plastic_energy = std::min(impact_energy_j - elastic_energy, max_absorbable * 0.8);
    return elastic_energy + plastic_energy;
}

uint8_t StructuralSimulation::determine_damage_level(double plastic_strain,
                                                      double penetration_ratio,
                                                      double von_mises_ratio) const {
    double score = (plastic_strain * 50.0) + (penetration_ratio * 3.0) + (von_mises_ratio * 1.5);
    if (score < 0.3) return 0;
    if (score < 0.8) return 1;
    if (score < 1.5) return 2;
    if (score < 2.5) return 3;
    return 4;
}

std::string StructuralSimulation::determine_failure_mode(double bending_stress_pa,
                                                          double shear_stress_pa,
                                                          double contact_stress_pa,
                                                          const MaterialProperties& mat) const {
    double bending_ratio = bending_stress_pa / mat.ultimate_strength_pa();
    double shear_ratio = shear_stress_pa / (0.6 * mat.ultimate_strength_pa());
    double punching_ratio = contact_stress_pa / mat.ultimate_strength_pa();

    if (bending_ratio > shear_ratio && bending_ratio > punching_ratio) return "bending";
    else if (shear_ratio > bending_ratio && shear_ratio > punching_ratio) return "shear";
    else if (punching_ratio > bending_ratio && punching_ratio > shear_ratio) return "punching";
    return "combined";
}

void StructuralSimulation::generate_deformation_field(std::vector<double>& field,
                                                       double max_deformation_mm,
                                                       double impact_x, double impact_y) const {
    field.resize(GRID_SIZE * GRID_SIZE);
    double dx = roof_length_m_ / (GRID_SIZE - 1);
    double dy = roof_width_m_ / (GRID_SIZE - 1);
    double sigma = 0.3 * std::max(roof_length_m_, roof_width_m_);

    for (int i = 0; i < GRID_SIZE; ++i) {
        for (int j = 0; j < GRID_SIZE; ++j) {
            double x = i * dx;
            double y = j * dy;
            double dist2 = (x - impact_x) * (x - impact_x) + (y - impact_y) * (y - impact_y);
            double gaussian = std::exp(-dist2 / (2.0 * sigma * sigma));
            field[i * GRID_SIZE + j] = max_deformation_mm * gaussian;
        }
    }
}

void StructuralSimulation::generate_stress_field(std::vector<double>& field,
                                                  double max_stress_mpa,
                                                  double impact_x, double impact_y) const {
    field.resize(GRID_SIZE * GRID_SIZE);
    double dx = roof_length_m_ / (GRID_SIZE - 1);
    double dy = roof_width_m_ / (GRID_SIZE - 1);
    double sigma = 0.25 * std::max(roof_length_m_, roof_width_m_);

    for (int i = 0; i < GRID_SIZE; ++i) {
        for (int j = 0; j < GRID_SIZE; ++j) {
            double x = i * dx;
            double y = j * dy;
            double dist2 = (x - impact_x) * (x - impact_x) + (y - impact_y) * (y - impact_y);
            double gaussian = std::exp(-dist2 / (2.0 * sigma * sigma));
            double edge_factor = 1.0 + 0.3 * std::sin(PI * i / (GRID_SIZE - 1)) * std::sin(PI * j / (GRID_SIZE - 1));
            field[i * GRID_SIZE + j] = max_stress_mpa * gaussian * edge_factor;
        }
    }
}

SimulationResult StructuralSimulation::run_simulation(const SensorData& sensor_data) {
    SimulationResult result;
    result.timestamp_ms = sensor_data.timestamp_ms;
    result.vehicle_id = sensor_data.vehicle_id;

    MaterialProperties mat = get_material_properties(sensor_data.protection_material);
    JohnsonCookParams jc = get_jc_params(sensor_data.protection_material);
    double thickness_m = sensor_data.protection_thickness / 1000.0;

    double strain_rate = calc_strain_rate(sensor_data.rock_velocity, thickness_m);
    double temperature_K = sensor_data.ambient_temp + 273.15;

    double impact_energy_j = calc_impact_energy(sensor_data.rock_mass, sensor_data.rock_velocity);
    result.impact_energy_j = impact_energy_j;

    double impactor_radius_m = std::cbrt(3.0 * sensor_data.rock_mass / (4.0 * PI * 2600.0));
    double contact_area_m2 = PI * impactor_radius_m * impactor_radius_m * 0.5;

    double contact_force_n = calc_contact_force(impact_energy_j, contact_area_m2, impactor_radius_m);
    double contact_stress_pa = contact_force_n / std::max(contact_area_m2, 1e-6);

    double bending_deformation_mm = calc_bending_deformation(
        sensor_data.rock_impact_force * 1000.0,
        roof_length_m_, roof_width_m_, thickness_m,
        mat.youngs_modulus_pa(), mat.poisson_ratio
    );

    double shear_stress_pa = 0.6 * sensor_data.rock_impact_force * 1000.0 / (roof_width_m_ * thickness_m);
    double bending_stress_pa = 0.75 * sensor_data.rock_impact_force * 1000.0 * roof_length_m_ /
                               (roof_width_m_ * thickness_m * thickness_m);

    double von_mises_pa = calc_von_mises_stress(
        sensor_data.rock_impact_force * 1000.0, contact_area_m2, bending_stress_pa
    );
    result.roof_von_mises_stress_mpa = von_mises_pa / 1e6;

    double plastic_strain = calc_plastic_strain_jc(
        von_mises_pa, mat, jc, strain_rate, temperature_K, mat.youngs_modulus_pa()
    );
    result.roof_plastic_strain = plastic_strain;

    double additional_plastic_deformation_mm = plastic_strain * thickness_m * 1000.0 * 5.0;
    result.roof_max_deformation_mm = bending_deformation_mm + additional_plastic_deformation_mm;

    result.penetration_depth_mm = calc_penetration_depth_jc(
        impact_energy_j, mat, jc, thickness_m, contact_area_m2, strain_rate, temperature_K
    );
    result.is_penetrated = (result.penetration_depth_mm >= sensor_data.protection_thickness * penetration_threshold_ratio_);

    result.absorbed_energy_j = calc_absorbed_energy(impact_energy_j, mat, thickness_m,
                                                     contact_area_m2 + roof_length_m_ * roof_width_m_ * 0.1);

    double penetration_ratio = result.penetration_depth_mm / std::max(sensor_data.protection_thickness, 1.0);
    double ultimate_stress_jc = calc_johnson_cook_flow_stress(mat, jc, 0.1, strain_rate, temperature_K);
    double von_mises_ratio = von_mises_pa / std::max(ultimate_stress_jc, 1.0);
    result.damage_level = determine_damage_level(plastic_strain, penetration_ratio, von_mises_ratio);

    result.failure_mode = determine_failure_mode(bending_stress_pa, shear_stress_pa, contact_stress_pa, mat);

    double impact_x = std::max(0.0, std::min(sensor_data.impact_location_x, roof_length_m_));
    double impact_y = std::max(0.0, std::min(sensor_data.impact_location_y, roof_width_m_));

    generate_deformation_field(result.deformation_field, result.roof_max_deformation_mm, impact_x, impact_y);
    generate_stress_field(result.stress_field, result.roof_von_mises_stress_mpa, impact_x, impact_y);

    result.strain_rate = strain_rate;
    result.dynamic_yield_strength_mpa = calc_johnson_cook_flow_stress(mat, jc, 0.001, strain_rate, temperature_K) / 1e6;
    result.temperature_K = temperature_K;
    result.protection_material = sensor_data.protection_material;

    return result;
}

}
