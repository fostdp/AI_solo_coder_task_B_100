#include "ahp_evaluator.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <map>

namespace fenyun {

AHPEvaluator::AHPEvaluator() {
    init_default_criteria();
    build_pairwise_matrix();

    material_candidates_ = {
        {"cowhide",   20.0},
        {"wood",      80.0},
        {"iron",       5.0},
        {"composite", 50.0}
    };
}

void AHPEvaluator::init_default_criteria() {
    criteria_names_ = {
        "energy_absorption",
        "structural_strength",
        "weight_factor",
        "cost_factor",
        "durability"
    };

    criteria_weights_ = {
        {"energy_absorption",   0.30},
        {"structural_strength", 0.25},
        {"weight_factor",       0.15},
        {"cost_factor",         0.15},
        {"durability",          0.15}
    };
}

void AHPEvaluator::build_pairwise_matrix() {
    int n = criteria_names_.size();
    pairwise_matrix_.assign(n, std::vector<double>(n, 1.0));

    std::map<std::pair<std::string, std::string>, double> comparisons = {
        {{"energy_absorption", "structural_strength"}, 2.0},
        {{"energy_absorption", "weight_factor"},       3.0},
        {{"energy_absorption", "cost_factor"},         3.0},
        {{"energy_absorption", "durability"},          2.0},
        {{"structural_strength", "weight_factor"},     2.0},
        {{"structural_strength", "cost_factor"},       2.0},
        {{"structural_strength", "durability"},        1.5},
        {{"weight_factor", "cost_factor"},             1.0},
        {{"weight_factor", "durability"},              1.0},
        {{"cost_factor", "durability"},                1.0}
    };

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const auto& ci = criteria_names_[i];
            const auto& cj = criteria_names_[j];
            auto it = comparisons.find({ci, cj});
            double val = 1.0;
            if (it != comparisons.end()) {
                val = it->second;
            } else {
                auto it2 = comparisons.find({cj, ci});
                if (it2 != comparisons.end()) {
                    val = 1.0 / it2->second;
                }
            }
            pairwise_matrix_[i][j] = val;
            pairwise_matrix_[j][i] = 1.0 / val;
        }
    }

    std::vector<double> ev = calc_eigenvector(pairwise_matrix_);
    for (int i = 0; i < n; ++i) {
        criteria_weights_[criteria_names_[i]] = ev[i];
    }

    check_consistency(pairwise_matrix_, consistency_ratio_);
}

std::vector<double> AHPEvaluator::calc_eigenvector(const std::vector<std::vector<double>>& matrix) const {
    int n = matrix.size();
    std::vector<double> eigenvector(n, 1.0 / n);

    for (int iter = 0; iter < 100; ++iter) {
        std::vector<double> next(n, 0.0);
        double sum = 0.0;

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                next[i] += matrix[i][j] * eigenvector[j];
            }
            sum += next[i];
        }

        for (int i = 0; i < n; ++i) {
            next[i] /= sum;
        }

        double diff = 0.0;
        for (int i = 0; i < n; ++i) {
            diff += std::abs(next[i] - eigenvector[i]);
        }

        eigenvector = next;
        if (diff < 1e-6) break;
    }

    return eigenvector;
}

bool AHPEvaluator::check_consistency(const std::vector<std::vector<double>>& matrix,
                                      double& cr) const {
    int n = matrix.size();
    if (n < 3) {
        cr = 0.0;
        return true;
    }

    std::vector<double> ev = calc_eigenvector(matrix);

    double lambda_max = 0.0;
    for (int i = 0; i < n; ++i) {
        double row_sum = 0.0;
        for (int j = 0; j < n; ++j) {
            row_sum += matrix[i][j] * ev[j];
        }
        lambda_max += row_sum / (ev[i] * n);
    }

    double ci = (lambda_max - n) / (n - 1);
    double ri = (n <= 10) ? static_cast<double>(RI_TABLE[n - 1]) / 10.0 : 1.5;

    if (ri < 0.01) {
        cr = 0.0;
        return true;
    }

    cr = ci / ri;
    return cr < 0.1;
}

void AHPEvaluator::set_criteria_weights(const std::map<std::string, double>& weights) {
    criteria_weights_ = weights;
}

std::map<std::string, double> AHPEvaluator::get_criteria_weights() const {
    return criteria_weights_;
}

std::vector<std::vector<double>> AHPEvaluator::get_pairwise_matrix() const {
    return pairwise_matrix_;
}

double AHPEvaluator::get_consistency_ratio() const {
    return consistency_ratio_;
}

double AHPEvaluator::normalize_value(double value, double min, double max, bool higher_is_better) const {
    if (std::abs(max - min) < 1e-10) return 0.5;
    double normalized = (value - min) / (max - min);
    return higher_is_better ? normalized : (1.0 - normalized);
}

double AHPEvaluator::calc_energy_absorption_score(const MaterialProperties& mat, double thickness_mm) const {
    double energy = mat.toughness_mj_m3 * thickness_mm / 1000.0 * 1000.0;
    return normalize_value(energy, 10.0, 500.0, true);
}

double AHPEvaluator::calc_structural_strength_score(const MaterialProperties& mat, double thickness_mm) const {
    double strength = mat.ultimate_strength_mpa * thickness_mm / 100.0;
    return normalize_value(strength, 5.0, 200.0, true);
}

double AHPEvaluator::calc_weight_factor_score(const MaterialProperties& mat, double thickness_mm) const {
    double areal_density = mat.density * thickness_mm / 1000.0;
    return normalize_value(areal_density, 5.0, 500.0, false);
}

double AHPEvaluator::calc_cost_factor_score(const MaterialProperties& mat, double thickness_mm) const {
    double cost = mat.cost_per_unit * thickness_mm / 10.0;
    return normalize_value(cost, 0.5, 100.0, false);
}

double AHPEvaluator::calc_durability_score(const MaterialProperties& mat, double thickness_mm) const {
    double base = 0.0;
    if (mat.name == "wood") base = 0.6;
    else if (mat.name == "cowhide") base = 0.4;
    else if (mat.name == "iron") base = 0.9;
    else if (mat.name == "composite") base = 0.8;

    double thickness_factor = normalize_value(thickness_mm, 5.0, 100.0, true);
    return std::min(1.0, base * 0.6 + thickness_factor * 0.4);
}

ProtectionEvaluation AHPEvaluator::evaluate_single(const std::string& material_type,
                                                    double thickness_mm,
                                                    uint32_t vehicle_id) {
    static std::map<std::string, MaterialProperties> mat_db = {
        {"cowhide",   {"cowhide",   860.0,   0.15, 0.40,  25.0,  60.0, 15.0,  80.0, 3.0}},
        {"wood",      {"wood",      650.0,  10.0,  0.35,  60.0, 120.0, 10.0,  50.0, 1.0}},
        {"iron",      {"iron",     7850.0, 206.0,  0.29, 235.0, 400.0, 80.0, 200.0, 10.0}},
        {"composite", {"composite", 900.0,  15.0,  0.33, 120.0, 250.0, 45.0, 300.0, 5.0}}
    };

    ProtectionEvaluation eval;
    eval.vehicle_id = vehicle_id;
    eval.timestamp_ms = current_timestamp_ms();
    eval.material_type = material_type;
    eval.material_thickness_mm = thickness_mm;

    auto it = mat_db.find(material_type);
    MaterialProperties mat = (it != mat_db.end()) ? it->second : mat_db["wood"];

    eval.energy_absorption_score = calc_energy_absorption_score(mat, thickness_mm);
    eval.structural_strength_score = calc_structural_strength_score(mat, thickness_mm);
    eval.weight_factor_score = calc_weight_factor_score(mat, thickness_mm);
    eval.cost_factor_score = calc_cost_factor_score(mat, thickness_mm);
    eval.durability_score = calc_durability_score(mat, thickness_mm);

    eval.ahp_weight_score =
        criteria_weights_["energy_absorption"]   * eval.energy_absorption_score +
        criteria_weights_["structural_strength"] * eval.structural_strength_score +
        criteria_weights_["weight_factor"]       * eval.weight_factor_score +
        criteria_weights_["cost_factor"]         * eval.cost_factor_score +
        criteria_weights_["durability"]          * eval.durability_score;

    return eval;
}

std::vector<ProtectionEvaluation> AHPEvaluator::evaluate_all(uint32_t vehicle_id,
                                                              double reference_thickness_mm) {
    std::vector<ProtectionEvaluation> results;

    for (const auto& [material, base_thickness] : material_candidates_) {
        double thickness = base_thickness;
        if (material == "iron") {
            thickness = 5.0;
        } else if (material == "cowhide") {
            thickness = 20.0;
        } else if (material == "wood") {
            thickness = reference_thickness_mm * 0.8;
        } else if (material == "composite") {
            thickness = reference_thickness_mm * 0.6;
        }

        auto eval = evaluate_single(material, thickness, vehicle_id);
        results.push_back(eval);
    }

    std::sort(results.begin(), results.end(),
              [](const ProtectionEvaluation& a, const ProtectionEvaluation& b) {
                  return a.ahp_weight_score > b.ahp_weight_score;
              });

    for (size_t i = 0; i < results.size(); ++i) {
        results[i].rank_position = static_cast<uint8_t>(i + 1);
        results[i].is_recommended = (i == 0);
    }

    return results;
}

}
