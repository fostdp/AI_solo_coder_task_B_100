#include "impact_simulator/impact_simulator.h"

#include <cmath>
#include <iostream>
#include <algorithm>

namespace fenyun {

ImpactSimulator::ImpactSimulator(std::shared_ptr<ConfigLoader> config)
    : config_(std::move(config)) {
    if (config_) {
        const auto& sc = config_->system_config().simulation;
        sim_config_.roof_length_m = sc.default_roof_length_m;
        sim_config_.roof_width_m = sc.default_roof_width_m;
        sim_config_.grid_size = sc.default_grid_size;
        sim_config_.jc_bisection_iterations = sc.jc_bisection_iterations;
        sim_config_.worker_threads = sc.threads;
    }
}

ImpactSimulator::~ImpactSimulator() {
    stop();
}

void ImpactSimulator::set_input_queue(std::shared_ptr<InputQueue> q) {
    input_queue_ = std::move(q);
}

void ImpactSimulator::set_output_queue(std::shared_ptr<OutputQueue> q) {
    output_queue_ = std::move(q);
}

void ImpactSimulator::start() {
    if (running_.exchange(true)) return;
    int n = std::max(1, sim_config_.worker_threads);
    for (int i = 0; i < n; ++i) {
        workers_.emplace_back(&ImpactSimulator::worker_loop, this, i);
    }
}

void ImpactSimulator::stop() {
    if (!running_.exchange(false)) return;
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void ImpactSimulator::worker_loop(int /*thread_id*/) {
    SensorData data{};
    while (running_.load()) {
        if (input_queue_ && input_queue_->pop(data)) {
            SimulationResult result = run_simulation(data);
            if (output_queue_ && !output_queue_->push(result)) {
                output_dropped_.fetch_add(1);
            }
            sims_run_.fetch_add(1);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

JohnsonCookParams ImpactSimulator::get_jc_params(const std::string& material) const {
    if (config_) return config_->get_jc_params(material);
    return JohnsonCookParams{};
}

MaterialProperties ImpactSimulator::get_material(const std::string& name) const {
    if (config_) return config_->get_material(name);
    return MaterialProperties{};
}

double ImpactSimulator::calc_johnson_cook_flow_stress(const std::string& /*material*/,
                                                      const JohnsonCookParams& jc,
                                                      double plastic_strain,
                                                      double strain_rate,
                                                      double temperature_K) const {
    if (plastic_strain <= 0) return jc.A;

    double strain_hardening = jc.A + jc.B * std::pow(std::max(plastic_strain, 1e-6), jc.n);
    double rate_factor = 1.0 + jc.C * std::log(std::max(strain_rate / jc.eps_dot_0, 1.0));

    double T_star = (temperature_K - jc.T_ref) / std::max(jc.T_melt - jc.T_ref, 1.0);
    T_star = std::max(0.0, std::min(0.99, T_star));
    double thermal_factor = 1.0 - std::pow(T_star, jc.m);

    return strain_hardening * rate_factor * thermal_factor;
}

double ImpactSimulator::calc_plastic_strain_jc(const std::string& material,
                                               double von_mises_stress_pa,
                                               double strain_rate,
                                               double temperature_K) const {
    JohnsonCookParams jc = get_jc_params(material);

    double rate_factor = 1.0 + jc.C * std::log(std::max(strain_rate / jc.eps_dot_0, 1.0));
    double T_star = (temperature_K - jc.T_ref) / std::max(jc.T_melt - jc.T_ref, 1.0);
    T_star = std::max(0.0, std::min(0.99, T_star));
    double thermal_factor = 1.0 - std::pow(T_star, jc.m);

    double dynamic_factor = rate_factor * thermal_factor;
    if (dynamic_factor < 1e-9) return 0.0;

    double equiv_static_stress = von_mises_stress_pa / dynamic_factor;
    if (equiv_static_stress <= jc.A) return 0.0;

    double target = equiv_static_stress - jc.A;
    if (target <= 0) return 0.0;

    double low = 1e-6;
    double high = 1.0;
    int max_iter = sim_config_.jc_bisection_iterations;

    for (int i = 0; i < max_iter; ++i) {
        double mid = (low + high) * 0.5;
        double f_val = jc.B * std::pow(mid, jc.n);
        if (f_val < target) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return (low + high) * 0.5;
}

double ImpactSimulator::calc_penetration_depth_jc(const std::string& material,
                                                   double impact_energy_j,
                                                   double thickness_m,
                                                   double strain_rate,
                                                   double temperature_K) const {
    JohnsonCookParams jc = get_jc_params(material);
    MaterialProperties mat = get_material(material);

    double ref_strain = 0.05;
    double dynamic_flow_stress = calc_johnson_cook_flow_stress(material, jc, ref_strain,
                                                                strain_rate, temperature_K);

    double density = mat.density;
    if (density < 1) density = 2700;

    double dynamic_toughness = dynamic_flow_stress * 0.05 + mat.yield_strength_pa() * 0.02;
    if (dynamic_toughness <= 0) dynamic_toughness = mat.toughness_j_m3();

    double volume_absorbed = impact_energy_j / std::max(dynamic_toughness, 1.0);
    double area = sim_config_.roof_length_m * sim_config_.roof_width_m * 0.02;
    double depth = volume_absorbed / std::max(area, 1e-6);

    depth *= 0.85;
    return std::min(depth, thickness_m * 1.5);
}

SimulationResult ImpactSimulator::run_simulation(const SensorData& data) {
    SimulationResult result{};
    result.simulation_id = sim_id_counter_.fetch_add(1) + 1;
    result.vehicle_id = data.vehicle_id;
    result.timestamp_ms = data.timestamp_ms > 0 ? data.timestamp_ms : current_timestamp_ms();
    result.protection_material = data.protection_material;
    result.temperature_K = data.ambient_temp + 273.15;

    MaterialProperties mat = get_material(data.protection_material);
    JohnsonCookParams jc = get_jc_params(data.protection_material);

    double thickness_m = data.protection_thickness / 1000.0;
    if (thickness_m <= 0) thickness_m = 0.08;

    double impact_energy = 0.5 * std::max(data.rock_mass, 1.0) * data.rock_velocity * data.rock_velocity;
    result.impact_energy_j = impact_energy;

    double strain_rate = data.rock_velocity / thickness_m;
    if (strain_rate < 1.0) strain_rate = 1.0;
    result.strain_rate = strain_rate;

    double dynamic_yield_pa = calc_johnson_cook_flow_stress(data.protection_material, jc,
                                                             0.001, strain_rate, result.temperature_K);
    result.dynamic_yield_strength_mpa = dynamic_yield_pa / 1e6;

    double L = sim_config_.roof_length_m;
    double W = sim_config_.roof_width_m;
    double t_p = thickness_m;
    double E = mat.youngs_modulus_pa();
    double nu = mat.poisson_ratio;

    double D = E * std::pow(t_p, 3) / (12 * (1 - nu * nu));
    double max_bending_stress = 0.75 * impact_energy * t_p / (L * W * t_p * t_p);
    double shear_stress = 0.6 * impact_energy / (L * t_p * t_p);
    double von_mises_stress = std::sqrt(max_bending_stress * max_bending_stress +
                                        3 * shear_stress * shear_stress);

    result.roof_von_mises_stress_mpa = von_mises_stress / 1e6;

    double plastic_strain = calc_plastic_strain_jc(data.protection_material, von_mises_stress,
                                                    strain_rate, result.temperature_K);
    result.roof_plastic_strain = plastic_strain;

    double elastic_deflection = (5.0 / 384.0) * (impact_energy / (L * W)) * std::pow(L, 4) / D * 1000;
    double plastic_deflection = plastic_strain * thickness_m * 5 * 1000;
    result.roof_max_deformation_mm = elastic_deflection + plastic_deflection;

    if (result.roof_max_deformation_mm < 0.1) result.roof_max_deformation_mm = 0.1;

    double penetration_m = calc_penetration_depth_jc(data.protection_material, impact_energy,
                                                      thickness_m, strain_rate, result.temperature_K);
    result.penetration_depth_mm = penetration_m * 1000;
    result.is_penetrated = result.penetration_depth_mm > data.protection_thickness * 0.9;

    double yield_ratio = von_mises_stress / dynamic_yield_pa;
    double pen_ratio = result.penetration_depth_mm / std::max(data.protection_thickness, 1.0);
    double strain_ratio = plastic_strain / 0.1;

    double max_ratio = std::max({yield_ratio, pen_ratio, strain_ratio});
    if (max_ratio < 0.4) result.damage_level = 0;
    else if (max_ratio < 0.7) result.damage_level = 1;
    else if (max_ratio < 1.0) result.damage_level = 2;
    else if (max_ratio < 1.5) result.damage_level = 3;
    else result.damage_level = 4;

    if (result.damage_level < 2) result.failure_mode = "bending";
    else if (result.damage_level == 2) result.failure_mode = "shear";
    else if (result.damage_level == 3) result.failure_mode = "punching";
    else result.failure_mode = "combined";

    result.absorbed_energy_j = impact_energy * (0.35 + 0.45 * (1 - max_ratio));

    double impact_x = data.impact_location_x;
    double impact_y = data.impact_location_y;

    generate_deformation_field(result.deformation_field, result.roof_max_deformation_mm,
                                impact_x, impact_y);
    generate_stress_field(result.stress_field, result.roof_von_mises_stress_mpa,
                          impact_x, impact_y);

    return result;
}

void ImpactSimulator::generate_deformation_field(std::vector<double>& field,
                                                  double max_deformation,
                                                  double impact_x,
                                                  double impact_y) const {
    int n = sim_config_.grid_size;
    field.resize(static_cast<size_t>(n * n));
    double L = sim_config_.roof_length_m;
    double W = sim_config_.roof_width_m;
    double cx = std::max(0.0, std::min(L, impact_x)) / L * (n - 1);
    double cy = std::max(0.0, std::min(W, impact_y)) / W * (n - 1);
    double sigma = 1.8 + (max_deformation / 50.0);

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            double d2 = (i - cx) * (i - cx) + (j - cy) * (j - cy);
            field[static_cast<size_t>(i * n + j)] = max_deformation * std::exp(-d2 / (2 * sigma * sigma));
        }
    }
}

void ImpactSimulator::generate_stress_field(std::vector<double>& field,
                                             double max_stress,
                                             double impact_x,
                                             double impact_y) const {
    int n = sim_config_.grid_size;
    field.resize(static_cast<size_t>(n * n));
    double L = sim_config_.roof_length_m;
    double W = sim_config_.roof_width_m;
    double cx = std::max(0.0, std::min(L, impact_x)) / L * (n - 1);
    double cy = std::max(0.0, std::min(W, impact_y)) / W * (n - 1);
    double sigma = 2.0;

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            double d2 = (i - cx) * (i - cx) + (j - cy) * (j - cy);
            field[static_cast<size_t>(i * n + j)] = max_stress * std::exp(-d2 / (2 * sigma * sigma));
        }
    }
}

}
