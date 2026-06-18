#include "formation_optimizer/formation_optimizer.h"

#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace fenyun {

namespace {

constexpr double WALL_Y = 0.0;
constexpr double START_Y = 10.0;
constexpr double DEFAULT_WALL_DISTANCE = 10.0;

struct FormationWeights {
    double survival = 0.45;
    double coverage = 0.30;
    double progress = 0.25;
};

const FormationWeights DEFAULT_WEIGHTS{};

}

FormationOptimizer::FormationOptimizer(std::shared_ptr<ConfigLoader> config)
    : config_(std::move(config)) {
}

FormationOptimizer::~FormationOptimizer() = default;

FormationType FormationOptimizer::formation_type_from_string(const std::string& s) const {
    if (s == "LINE") return FormationType::LINE;
    if (s == "WEDGE") return FormationType::WEDGE;
    if (s == "ECHELON") return FormationType::ECHELON;
    if (s == "V_SHAPE") return FormationType::V_SHAPE;
    if (s == "COLUMN") return FormationType::COLUMN;
    if (s == "DIAMOND") return FormationType::DIAMOND;
    return FormationType::LINE;
}

std::string FormationOptimizer::formation_type_to_string(FormationType t) const {
    switch (t) {
        case FormationType::LINE: return "LINE";
        case FormationType::WEDGE: return "WEDGE";
        case FormationType::ECHELON: return "ECHELON";
        case FormationType::V_SHAPE: return "V_SHAPE";
        case FormationType::COLUMN: return "COLUMN";
        case FormationType::DIAMOND: return "DIAMOND";
        default: return "LINE";
    }
}

FormationVehicle FormationOptimizer::create_vehicle_in_formation(
    int index,
    FormationType type,
    int total,
    double spacing,
    const std::string& vehicle_type,
    double wall_distance) {

    FormationVehicle v{};
    v.vehicle_id = static_cast<uint32_t>(index + 1);
    v.vehicle_type = vehicle_type.empty() ? "FENYUN" : vehicle_type;
    v.spacing_m = spacing;
    v.is_lead = false;
    v.heading_deg = 180.0;

    double y_base = START_Y - (DEFAULT_WALL_DISTANCE - wall_distance);
    if (y_base < 1.0) y_base = 1.0;

    switch (type) {
        case FormationType::LINE: {
            int half = (total - 1) / 2;
            int offset = index - half;
            v.position_x = offset * spacing;
            v.position_y = y_base;
            v.is_lead = (total % 2 == 1 && index == half);
            break;
        }
        case FormationType::WEDGE: {
            if (index == 0) {
                v.position_x = 0.0;
                v.position_y = y_base - spacing * 0.5;
                v.is_lead = true;
            } else {
                int row = 0;
                int count_in_row = 1;
                int cumulative = 1;
                int idx = index;
                while (idx >= count_in_row && cumulative < total) {
                    idx -= count_in_row;
                    row++;
                    count_in_row = 2;
                    cumulative += count_in_row;
                }
                double row_y = y_base - spacing * 0.5 - (row + 1) * spacing;
                int side = (idx % 2 == 0) ? -1 : 1;
                v.position_x = side * (row + 1) * spacing;
                v.position_y = row_y;
            }
            break;
        }
        case FormationType::ECHELON: {
            double depth_step = spacing * 0.6;
            if (total <= 1) {
                v.position_x = 0.0;
                v.position_y = y_base;
                v.is_lead = true;
            } else {
                int half = (total - 1) / 2;
                int offset = index - half;
                v.position_x = offset * spacing;
                v.position_y = y_base - std::abs(offset) * depth_step;
                v.is_lead = (offset == 0);
            }
            break;
        }
        case FormationType::V_SHAPE: {
            if (index == 0) {
                v.position_x = 0.0;
                v.position_y = y_base;
                v.is_lead = true;
            } else {
                int pair_idx = (index - 1) / 2;
                int side = ((index - 1) % 2 == 0) ? -1 : 1;
                v.position_x = side * (pair_idx + 1) * spacing;
                v.position_y = y_base + (pair_idx + 1) * spacing * 0.5;
            }
            break;
        }
        case FormationType::COLUMN: {
            v.position_x = 0.0;
            v.position_y = y_base - index * spacing;
            v.is_lead = (index == 0);
            break;
        }
        case FormationType::DIAMOND: {
            if (total == 1) {
                v.position_x = 0.0;
                v.position_y = y_base;
                v.is_lead = true;
            } else {
                int layers = 1;
                int total_in_diamond = 1;
                while (total_in_diamond < total) {
                    layers++;
                    total_in_diamond += 2;
                    if (total_in_diamond >= total) break;
                    total_in_diamond += 2;
                }
                int idx = index;
                int current_layer = 0;
                int layer_start = 0;
                int layer_count = 1;
                bool top_half = true;
                while (idx >= layer_start + layer_count) {
                    layer_start += layer_count;
                    current_layer++;
                    if (top_half && current_layer < layers) {
                        layer_count = current_layer * 2 + 1;
                    } else {
                        top_half = false;
                        int mirror_layer = layers - current_layer;
                        if (mirror_layer < 0) mirror_layer = 0;
                        layer_count = mirror_layer * 2 + 1;
                    }
                }
                int pos_in_layer = idx - layer_start;
                double center_offset = (pos_in_layer - (layer_count - 1) / 2.0) * spacing;
                double y_offset;
                if (top_half) {
                    y_offset = -current_layer * spacing * 0.5;
                } else {
                    int dist_from_middle = current_layer - layers;
                    y_offset = dist_from_middle * spacing * 0.5;
                }
                v.position_x = center_offset;
                v.position_y = y_base + y_offset;
                v.is_lead = (index == 0);
            }
            break;
        }
        default: {
            v.position_x = 0.0;
            v.position_y = y_base;
            v.is_lead = true;
            break;
        }
    }

    return v;
}

FormationConfig FormationOptimizer::get_formation_template(const std::string& type_name, int vehicle_count) {
    FormationConfig cfg{};
    cfg.formation_type = type_name;
    cfg.vehicle_count = vehicle_count;
    cfg.wall_distance_m = DEFAULT_WALL_DISTANCE;

    FormationType type = formation_type_from_string(type_name);

    switch (type) {
        case FormationType::LINE:
            cfg.spacing_m = 2.5;
            cfg.attack_width_m = (vehicle_count - 1) * cfg.spacing_m + 3.0;
            break;
        case FormationType::WEDGE:
            cfg.spacing_m = 2.0;
            cfg.attack_width_m = (static_cast<int>(std::ceil(vehicle_count / 2.0))) * cfg.spacing_m * 2;
            break;
        case FormationType::ECHELON:
            cfg.spacing_m = 2.2;
            cfg.attack_width_m = (vehicle_count - 1) * cfg.spacing_m + 3.0;
            break;
        case FormationType::V_SHAPE:
            cfg.spacing_m = 2.2;
            cfg.attack_width_m = (static_cast<int>(std::ceil(vehicle_count / 2.0))) * cfg.spacing_m * 2;
            break;
        case FormationType::COLUMN:
            cfg.spacing_m = 3.0;
            cfg.attack_width_m = 3.0;
            break;
        case FormationType::DIAMOND:
            cfg.spacing_m = 2.3;
            cfg.attack_width_m = (static_cast<int>(std::ceil(vehicle_count / 2.0))) * cfg.spacing_m * 2;
            break;
        default:
            cfg.spacing_m = 2.5;
            cfg.attack_width_m = (vehicle_count - 1) * cfg.spacing_m + 3.0;
            break;
    }

    cfg.vehicle_types.clear();
    for (int i = 0; i < vehicle_count; ++i) {
        cfg.vehicle_types.push_back("FENYUN");
    }

    return cfg;
}

std::vector<FormationVehicle> FormationOptimizer::layout_formation(const FormationConfig& config) {
    std::vector<FormationVehicle> vehicles;
    FormationType type = formation_type_from_string(config.formation_type);
    int total = config.vehicle_count;
    double spacing = config.spacing_m;
    double wall_dist = config.wall_distance_m > 0 ? config.wall_distance_m : DEFAULT_WALL_DISTANCE;

    for (int i = 0; i < total; ++i) {
        std::string vtype = "FENYUN";
        if (i < static_cast<int>(config.vehicle_types.size())) {
            vtype = config.vehicle_types[i];
        }
        vehicles.push_back(create_vehicle_in_formation(i, type, total, spacing, vtype, wall_dist));
    }

    return vehicles;
}

double FormationOptimizer::calc_survival_probability(
    const FormationConfig& config,
    double rock_fall_rate,
    double avg_rock_mass) const {

    FormationType type = formation_type_from_string(config.formation_type);
    int n = config.vehicle_count;
    if (n <= 0) return 0.0;

    double spacing = config.spacing_m;
    double spacing_factor = std::min(1.0, (spacing - 1.0) / 3.0);
    spacing_factor = 0.3 + 0.7 * spacing_factor;

    double area_per_vehicle = spacing * spacing;
    if (type == FormationType::LINE) area_per_vehicle = spacing * 3.0;
    if (type == FormationType::COLUMN) area_per_vehicle = 3.0 * spacing;
    double density = 1.0 / area_per_vehicle;
    double density_factor = std::max(0.0, 1.0 - density * 2.0);
    density_factor = 0.4 + 0.6 * density_factor;

    double formation_survival_bonus = 1.0;
    switch (type) {
        case FormationType::LINE:
            formation_survival_bonus = 0.88;
            break;
        case FormationType::WEDGE: {
            formation_survival_bonus = 0.75;
            break;
        }
        case FormationType::ECHELON:
            formation_survival_bonus = 0.90;
            break;
        case FormationType::V_SHAPE:
            formation_survival_bonus = 0.82;
            break;
        case FormationType::COLUMN:
            formation_survival_bonus = 0.92;
            break;
        case FormationType::DIAMOND:
            formation_survival_bonus = 0.86;
            break;
        default:
            break;
    }

    double threat_factor = std::exp(-rock_fall_rate * 0.15);
    double mass_factor = std::exp(-avg_rock_mass * 0.008);
    double threat = 0.3 + 0.7 * threat_factor * mass_factor;

    double survival = 0.25 * spacing_factor + 0.25 * density_factor
                     + 0.3 * formation_survival_bonus + 0.2 * threat;

    return std::min(1.0, std::max(0.0, survival));
}

double FormationOptimizer::calc_coverage_score(
    const FormationConfig& config,
    double wall_length,
    double wall_height) const {

    if (wall_length < 1e-6) return 0.5;

    FormationType type = formation_type_from_string(config.formation_type);
    double attack_width = config.attack_width_m;
    int n = config.vehicle_count;

    double effective_width = attack_width;
    if (effective_width < 1e-6) {
        double spacing = config.spacing_m;
        switch (type) {
            case FormationType::LINE:
            case FormationType::ECHELON:
                effective_width = (n - 1) * spacing + 3.0;
                break;
            case FormationType::WEDGE:
            case FormationType::V_SHAPE:
            case FormationType::DIAMOND:
                effective_width = (static_cast<int>(std::ceil(n / 2.0))) * spacing * 2;
                break;
            case FormationType::COLUMN:
                effective_width = 3.0;
                break;
            default:
                effective_width = 3.0;
                break;
        }
    }

    double width_ratio = effective_width / wall_length;
    double width_score = std::min(1.0, width_ratio * 1.1);

    double type_coverage_bonus = 1.0;
    switch (type) {
        case FormationType::LINE:
            type_coverage_bonus = 1.00;
            break;
        case FormationType::ECHELON:
            type_coverage_bonus = 0.92;
            break;
        case FormationType::WEDGE:
            type_coverage_bonus = 0.80;
            break;
        case FormationType::V_SHAPE:
            type_coverage_bonus = 0.78;
            break;
        case FormationType::DIAMOND:
            type_coverage_bonus = 0.75;
            break;
        case FormationType::COLUMN:
            type_coverage_bonus = 0.45;
            break;
        default:
            break;
    }

    double vehicle_density_score = std::min(1.0, n * 1.0 / 8.0);

    double coverage = 0.55 * width_score + 0.30 * type_coverage_bonus + 0.15 * vehicle_density_score;

    return std::min(1.0, std::max(0.0, coverage));
}

double FormationOptimizer::calc_progress_rate(const FormationConfig& config) const {
    FormationType type = formation_type_from_string(config.formation_type);
    double spacing = config.spacing_m;
    int n = config.vehicle_count;

    double type_progress = 1.0;
    switch (type) {
        case FormationType::WEDGE:
            type_progress = 1.00;
            break;
        case FormationType::V_SHAPE:
            type_progress = 0.92;
            break;
        case FormationType::COLUMN:
            type_progress = 0.88;
            break;
        case FormationType::DIAMOND:
            type_progress = 0.82;
            break;
        case FormationType::ECHELON:
            type_progress = 0.75;
            break;
        case FormationType::LINE:
            type_progress = 0.60;
            break;
        default:
            break;
    }

    double spacing_penalty = std::max(0.8, 1.0 - (spacing - 1.5) * 0.08);

    double size_penalty = std::max(0.75, 1.0 - (n - 1) * 0.025);

    double progress = type_progress * spacing_penalty * size_penalty;

    return std::min(1.0, std::max(0.0, progress));
}

double FormationOptimizer::terrain_speed_penalty(const TerrainConstraint& t) const {
    switch (t.terrain_type) {
        case TerrainType::FLAT:          return 1.0;
        case TerrainType::GENTLE_SLOPE:  return 0.85;
        case TerrainType::STEEP_SLOPE:   return 0.55;
        case TerrainType::MUDDY:         return 0.45;
        case TerrainType::ROCKY:         return 0.65;
        case TerrainType::TRENCH_FIELD:  return 0.5;
        default:                         return 1.0;
    }
}

double FormationOptimizer::terrain_survival_penalty(const TerrainConstraint& t) const {
    switch (t.terrain_type) {
        case TerrainType::FLAT:          return 1.0;
        case TerrainType::GENTLE_SLOPE:  return 0.95;
        case TerrainType::STEEP_SLOPE:   return 0.75;
        case TerrainType::MUDDY:         return 0.60;
        case TerrainType::ROCKY:         return 0.80;
        case TerrainType::TRENCH_FIELD:  return 0.70;
        default:                         return 1.0;
    }
}

double FormationOptimizer::terrain_coverage_penalty(const TerrainConstraint& t, double formation_width) const {
    double base_penalty = 1.0;
    switch (t.terrain_type) {
        case TerrainType::FLAT:
            base_penalty = 1.0;
            break;
        case TerrainType::GENTLE_SLOPE:
            base_penalty = 0.90;
            break;
        case TerrainType::STEEP_SLOPE: {
            double width_factor = std::max(0.3, 1.0 - (formation_width - 3.0) * 0.05);
            base_penalty = 0.50 * width_factor;
            break;
        }
        case TerrainType::MUDDY:
            base_penalty = 0.70;
            break;
        case TerrainType::ROCKY:
            base_penalty = 0.75;
            break;
        case TerrainType::TRENCH_FIELD: {
            double width_factor = std::max(0.25, 1.0 - (formation_width - 3.0) * 0.07);
            base_penalty = 0.40 * width_factor;
            break;
        }
        default:
            base_penalty = 1.0;
            break;
    }
    return std::min(1.0, std::max(0.1, base_penalty));
}

double FormationOptimizer::calculate_terrain_feasibility(const FormationConfig& cfg,
                                                         const TerrainConstraint& t,
                                                         int vehicle_index) const {
    FormationType type = formation_type_from_string(cfg.formation_type);
    double feasibility = 1.0;

    switch (t.terrain_type) {
        case TerrainType::FLAT:
            feasibility = 1.0;
            break;
        case TerrainType::GENTLE_SLOPE:
            feasibility = 0.90;
            break;
        case TerrainType::STEEP_SLOPE: {
            bool is_high_cog = false;
            if (vehicle_index < static_cast<int>(cfg.vehicle_types.size())) {
                const std::string& vtype = cfg.vehicle_types[vehicle_index];
                if (vtype == "YUNTI" || vtype == "楼车") {
                    is_high_cog = true;
                }
            }
            feasibility = is_high_cog ? 0.35 : 0.65;

            if (type == FormationType::COLUMN || type == FormationType::DIAMOND) {
                feasibility *= 1.2;
            } else if (type == FormationType::LINE) {
                feasibility *= 0.7;
            }
            break;
        }
        case TerrainType::MUDDY:
            feasibility = 0.55;
            if (type == FormationType::COLUMN) feasibility *= 1.15;
            break;
        case TerrainType::ROCKY:
            feasibility = 0.70;
            break;
        case TerrainType::TRENCH_FIELD: {
            if (type == FormationType::COLUMN) {
                feasibility = 0.85;
            } else if (type == FormationType::DIAMOND) {
                feasibility = 0.70;
            } else if (type == FormationType::LINE || type == FormationType::ECHELON) {
                feasibility = 0.35;
            } else {
                feasibility = 0.55;
            }
            break;
        }
        default:
            feasibility = 1.0;
            break;
    }

    return std::min(1.0, std::max(0.0, feasibility));
}

double FormationOptimizer::evaluate_formation(
    const FormationConfig& config,
    const FormationOptimizationRequest& req) const {

    TerrainConstraint terrain = req.terrain;
    if (terrain.terrain_type == TerrainType::FLAT
        && config.terrain.terrain_type != TerrainType::FLAT) {
        terrain = config.terrain;
    }

    double t_surv_pen = terrain_survival_penalty(terrain);
    double t_cov_pen = terrain_coverage_penalty(terrain, config.attack_width_m);
    double t_speed_pen = terrain_speed_penalty(terrain);

    double feasibility_avg = 0.0;
    for (int i = 0; i < config.vehicle_count; ++i) {
        feasibility_avg += calculate_terrain_feasibility(config, terrain, i);
    }
    if (config.vehicle_count > 0) {
        feasibility_avg /= config.vehicle_count;
    }

    double survival = calc_survival_probability(config, req.rock_fall_rate_per_sec, req.avg_rock_mass_kg);
    double coverage = calc_coverage_score(config, req.wall_length_m, req.wall_height_m);
    double progress = calc_progress_rate(config);

    survival *= t_surv_pen;
    coverage *= t_cov_pen;
    progress *= t_speed_pen;

    survival *= feasibility_avg;
    coverage *= feasibility_avg;
    progress *= feasibility_avg;

    double total = DEFAULT_WEIGHTS.survival * survival
                 + DEFAULT_WEIGHTS.coverage * coverage
                 + DEFAULT_WEIGHTS.progress * progress;

    return std::min(1.0, std::max(0.0, total));
}

std::vector<FormationConfig> FormationOptimizer::generate_candidates(
    const FormationOptimizationRequest& req,
    int candidate_count) {

    std::vector<FormationConfig> candidates;
    std::vector<FormationType> types = {
        FormationType::LINE,
        FormationType::WEDGE,
        FormationType::ECHELON,
        FormationType::V_SHAPE,
        FormationType::COLUMN,
        FormationType::DIAMOND
    };

    TerrainConstraint terrain = req.terrain;
    if (terrain.terrain_type == TerrainType::FLAT
        && req.baseline.terrain.terrain_type != TerrainType::FLAT) {
        terrain = req.baseline.terrain;
    }

    std::vector<double> type_weights(types.size(), 1.0);
    switch (terrain.terrain_type) {
        case TerrainType::FLAT:
        case TerrainType::GENTLE_SLOPE:
            break;
        case TerrainType::STEEP_SLOPE:
        case TerrainType::MUDDY:
            type_weights[static_cast<size_t>(FormationType::LINE)] = 0.4;
            type_weights[static_cast<size_t>(FormationType::ECHELON)] = 0.7;
            type_weights[static_cast<size_t>(FormationType::COLUMN)] = 1.8;
            type_weights[static_cast<size_t>(FormationType::DIAMOND)] = 1.6;
            type_weights[static_cast<size_t>(FormationType::WEDGE)] = 1.3;
            break;
        case TerrainType::ROCKY:
            type_weights[static_cast<size_t>(FormationType::LINE)] = 0.6;
            type_weights[static_cast<size_t>(FormationType::COLUMN)] = 1.4;
            type_weights[static_cast<size_t>(FormationType::DIAMOND)] = 1.3;
            break;
        case TerrainType::TRENCH_FIELD:
            type_weights[static_cast<size_t>(FormationType::LINE)] = 0.25;
            type_weights[static_cast<size_t>(FormationType::ECHELON)] = 0.4;
            type_weights[static_cast<size_t>(FormationType::WEDGE)] = 0.9;
            type_weights[static_cast<size_t>(FormationType::V_SHAPE)] = 0.8;
            type_weights[static_cast<size_t>(FormationType::COLUMN)] = 2.2;
            type_weights[static_cast<size_t>(FormationType::DIAMOND)] = 1.8;
            break;
        default:
            break;
    }

    double total_weight = 0.0;
    for (double w : type_weights) total_weight += w;
    std::vector<double> cumulative_weights;
    double cum = 0.0;
    for (double w : type_weights) {
        cum += w / total_weight;
        cumulative_weights.push_back(cum);
    }

    std::vector<double> spacings = {1.5, 2.0, 2.5, 3.0, 3.5, 4.0};
    std::vector<double> width_factors = {0.8, 1.0, 1.2, 1.4};

    std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);
    std::uniform_int_distribution<size_t> spacing_dist(0, spacings.size() - 1);
    std::uniform_int_distribution<size_t> width_dist(0, width_factors.size() - 1);

    FormationType base_type = formation_type_from_string(req.baseline.formation_type);
    double base_spacing = req.baseline.spacing_m > 0 ? req.baseline.spacing_m : 2.5;

    for (FormationType t : types) {
        FormationConfig cfg = get_formation_template(formation_type_to_string(t), req.vehicle_count);
        cfg.wall_distance_m = req.baseline.wall_distance_m > 0 ? req.baseline.wall_distance_m : DEFAULT_WALL_DISTANCE;
        cfg.spacing_m = base_spacing;
        cfg.terrain = terrain;
        if (cfg.attack_width_m < 1e-6) {
            cfg.attack_width_m = (req.vehicle_count - 1) * cfg.spacing_m + 3.0;
        }
        candidates.push_back(cfg);
    }

    while (static_cast<int>(candidates.size()) < candidate_count) {
        double r = uniform_dist(rng_);
        FormationType t = FormationType::LINE;
        for (size_t i = 0; i < cumulative_weights.size(); ++i) {
            if (r <= cumulative_weights[i] || i == cumulative_weights.size() - 1) {
                t = types[i];
                break;
            }
        }

        double spacing = spacings[spacing_dist(rng_)];
        double wf = width_factors[width_dist(rng_)];

        FormationConfig cfg;
        cfg.formation_type = formation_type_to_string(t);
        cfg.vehicle_count = req.vehicle_count;
        cfg.spacing_m = spacing;
        cfg.wall_distance_m = req.baseline.wall_distance_m > 0 ? req.baseline.wall_distance_m : DEFAULT_WALL_DISTANCE;
        cfg.terrain = terrain;

        switch (t) {
            case FormationType::LINE:
            case FormationType::ECHELON:
                cfg.attack_width_m = ((req.vehicle_count - 1) * spacing + 3.0) * wf;
                break;
            case FormationType::WEDGE:
            case FormationType::V_SHAPE:
            case FormationType::DIAMOND:
                cfg.attack_width_m = (static_cast<int>(std::ceil(req.vehicle_count / 2.0))) * spacing * 2 * wf;
                break;
            case FormationType::COLUMN:
                cfg.attack_width_m = 3.0 * wf;
                break;
            default:
                cfg.attack_width_m = (req.vehicle_count - 1) * spacing + 3.0;
                break;
        }

        for (int i = 0; i < req.vehicle_count; ++i) {
            cfg.vehicle_types.push_back("FENYUN");
        }

        bool duplicate = false;
        for (const auto& existing : candidates) {
            if (existing.formation_type == cfg.formation_type
                && std::abs(existing.spacing_m - cfg.spacing_m) < 0.1
                && std::abs(existing.attack_width_m - cfg.attack_width_m) < 0.5) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            candidates.push_back(cfg);
        } else if (static_cast<int>(candidates.size()) >= static_cast<int>(types.size()) + candidate_count) {
            break;
        }
    }

    if (static_cast<int>(candidates.size()) > candidate_count) {
        std::vector<std::pair<double, FormationConfig>> scored;
        for (const auto& c : candidates) {
            scored.emplace_back(evaluate_formation(c, req), c);
        }
        std::sort(scored.begin(), scored.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        candidates.clear();
        for (int i = 0; i < candidate_count && i < static_cast<int>(scored.size()); ++i) {
            candidates.push_back(scored[i].second);
        }
    }

    return candidates;
}

FormationOptimizationResult FormationOptimizer::optimize(const FormationOptimizationRequest& req) {
    FormationOptimizationResult result{};
    result.optimization_id = optimization_id_counter_.fetch_add(1) + 1;
    result.timestamp_ms = current_timestamp_ms();

    int target_count = 8;
    auto candidates = generate_candidates(req, target_count);
    result.candidate_formations = candidates;

    TerrainConstraint terrain = req.terrain;
    if (terrain.terrain_type == TerrainType::FLAT
        && req.baseline.terrain.terrain_type != TerrainType::FLAT) {
        terrain = req.baseline.terrain;
    }

    double t_surv_pen = terrain_survival_penalty(terrain);
    double t_cov_pen = terrain_coverage_penalty(terrain, req.baseline.attack_width_m > 0 ? req.baseline.attack_width_m : 10.0);
    double t_speed_pen = terrain_speed_penalty(terrain);

    double best_score = -1.0;
    FormationConfig best_cfg;
    double best_survival = 0.0;
    double best_coverage = 0.0;
    double best_progress = 0.0;

    for (const auto& cfg : candidates) {
        double cfg_t_cov_pen = terrain_coverage_penalty(terrain, cfg.attack_width_m);
        double feasibility_avg = 0.0;
        for (int i = 0; i < cfg.vehicle_count; ++i) {
            feasibility_avg += calculate_terrain_feasibility(cfg, terrain, i);
        }
        if (cfg.vehicle_count > 0) {
            feasibility_avg /= cfg.vehicle_count;
        }

        double survival = calc_survival_probability(cfg, req.rock_fall_rate_per_sec, req.avg_rock_mass_kg);
        double coverage = calc_coverage_score(cfg, req.wall_length_m, req.wall_height_m);
        double progress = calc_progress_rate(cfg);

        survival *= t_surv_pen;
        coverage *= cfg_t_cov_pen;
        progress *= t_speed_pen;

        survival *= feasibility_avg;
        coverage *= feasibility_avg;
        progress *= feasibility_avg;

        double score = DEFAULT_WEIGHTS.survival * survival
                     + DEFAULT_WEIGHTS.coverage * coverage
                     + DEFAULT_WEIGHTS.progress * progress;

        if (score > best_score) {
            best_score = score;
            best_cfg = cfg;
            best_survival = survival;
            best_coverage = coverage;
            best_progress = progress;
        }
    }

    best_cfg.terrain = terrain;
    result.best_formation = best_cfg;
    result.survival_probability = best_survival;
    result.avg_coverage_score = best_coverage;
    result.total_progress_rate = best_progress;

    result.recommendations.clear();

    FormationType best_type = formation_type_from_string(best_cfg.formation_type);

    {
        std::ostringstream oss;
        oss << "推荐采用" << best_cfg.formation_type << "队形，";
        if (best_type == FormationType::LINE) {
            oss << "该队形在城墙覆盖面上表现最佳，适合宽城墙正面突破。";
        } else if (best_type == FormationType::WEDGE) {
            oss << "该队形推进速度最快，集中兵力于一点可快速突破城墙防御。";
        } else if (best_type == FormationType::COLUMN) {
            oss << "该队形纵深排列生存性最佳，适合狭窄通道或集中突击。";
        } else if (best_type == FormationType::ECHELON) {
            oss << "该队形兼顾覆盖与推进，适合中等宽度城墙攻击。";
        } else if (best_type == FormationType::V_SHAPE) {
            oss << "该队形后方展开便于两翼支援，适合多方向作战。";
        } else if (best_type == FormationType::DIAMOND) {
            oss << "该队形全方位防御均衡，适合敌情不明的战场环境。";
        }
        result.recommendations.push_back(oss.str());
    }

    switch (terrain.terrain_type) {
        case TerrainType::TRENCH_FIELD: {
            std::ostringstream oss;
            oss << "壕沟众多，建议采用纵队(COLUMN)逐次通过，避免横排(LINE)队形跨越壕沟。";
            if (terrain.trench_width_m > 2.0) {
                oss << "壕沟宽度达" << std::fixed << std::setprecision(1) << terrain.trench_width_m
                    << "米，需确保车辆越壕能力足够。";
            }
            result.recommendations.push_back(oss.str());
            break;
        }
        case TerrainType::STEEP_SLOPE: {
            std::ostringstream oss;
            oss << "地形陡峭(坡度" << std::fixed << std::setprecision(1) << terrain.slope_deg
                << "°)，纵队或菱形队形爬坡稳定性更佳。";
            bool has_high_cog = false;
            for (const auto& vt : best_cfg.vehicle_types) {
                if (vt == "YUNTI" || vt == "楼车") { has_high_cog = true; break; }
            }
            if (has_high_cog) {
                oss << "编队中含高重心楼车，应降低速度并拉开前后间距防止倾覆。";
            }
            result.recommendations.push_back(oss.str());
            break;
        }
        case TerrainType::MUDDY: {
            std::ostringstream oss;
            oss << "泥泞地形需减小间距至1.5-2.0米以保持救援距离，防止车辆陷车无法互救。";
            if (terrain.mud_depth_cm > 20.0) {
                oss << "泥泞深度达" << std::fixed << std::setprecision(0) << terrain.mud_depth_cm
                    << "厘米，建议使用楔形(WEDGE)队形开辟通路。";
            }
            result.recommendations.push_back(oss.str());
            break;
        }
        case TerrainType::ROCKY: {
            std::ostringstream oss;
            oss << "岩石地带，建议采用菱形(DIAMOND)或楔形(WEDGE)队形分散障碍规避压力。";
            if (terrain.obstacle_density > 0.4) {
                oss << "障碍密度较高，需保证足够的车辆间距(" << std::fixed << std::setprecision(1)
                    << std::max(2.5, best_cfg.spacing_m) << "米以上)便于机动避让。";
            }
            result.recommendations.push_back(oss.str());
            break;
        }
        case TerrainType::GENTLE_SLOPE: {
            std::ostringstream oss;
            oss << "缓坡地形，整体影响较小，但高重心车辆仍需注意行驶稳定性。";
            result.recommendations.push_back(oss.str());
            break;
        }
        default:
            break;
    }

    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << "当前间距 " << best_cfg.spacing_m << "米，";
        if (terrain.terrain_type == TerrainType::MUDDY && best_cfg.spacing_m > 2.5) {
            oss << "泥泞地形建议减小间距至2.0米以内以保持救援距离。";
        } else if (best_survival < 0.75 && terrain.terrain_type == TerrainType::FLAT) {
            oss << "建议增大车辆间距至3.0-4.0米以降低被落石击中的密度，可提升生存率约"
                << std::setprecision(0) << (0.85 - best_survival) * 100 << "%。";
        } else if (best_coverage < 0.70 && best_cfg.attack_width_m < req.wall_length_m * 0.8
                   && (terrain.terrain_type == TerrainType::FLAT || terrain.terrain_type == TerrainType::GENTLE_SLOPE)) {
            oss << "建议扩大攻击宽度至" << req.wall_length_m * 0.9 << "米以上，可更全面地覆盖城墙防御面。";
        } else if (best_progress < 0.75 && best_type != FormationType::WEDGE
                   && terrain.terrain_type == TerrainType::FLAT) {
            oss << "如需加快推进速度，可考虑改用楔形(WEDGE)队形。";
        } else {
            oss << "该间距配置合理，兼顾了生存率、覆盖度与推进效率。";
        }
        result.recommendations.push_back(oss.str());
    }

    {
        std::ostringstream oss;
        oss << "综合评分: 生存率 " << std::fixed << std::setprecision(1)
            << best_survival * 100 << "%，覆盖度 "
            << best_coverage * 100 << "%，推进效率 "
            << best_progress * 100 << "%。";
        if (terrain.terrain_type != TerrainType::FLAT) {
            oss << "地形惩罚系数：速度×" << std::setprecision(2) << t_speed_pen
                << "，生存×" << t_surv_pen << "，覆盖×" << t_cov_pen << "。";
        }
        if (req.rock_fall_rate_per_sec > 2.0) {
            oss << "当前落石频率较高(" << req.rock_fall_rate_per_sec << "/秒)，优先保证队形疏散度。";
        }
        if (req.wall_length_m > 20.0 && (terrain.terrain_type == TerrainType::FLAT || terrain.terrain_type == TerrainType::GENTLE_SLOPE)) {
            oss << "城墙较宽(" << req.wall_length_m << "米)，建议优先考虑覆盖面大的队形。";
        }
        result.recommendations.push_back(oss.str());
    }

    optimizations_run_.fetch_add(1);
    return result;
}

}
