#include "ahp_evaluator.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <map>
#include <random>
#include <iomanip>
#include <sstream>

namespace fenyun {

static const double SAATY_RI[16] = {
    0.00, 0.00, 0.00, 0.58, 0.90, 1.12, 1.24, 1.32,
    1.41, 1.45, 1.49, 1.51, 1.54, 1.56, 1.57, 1.59
};

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
            if (it != comparisons.end()) val = it->second;
            else {
                auto it2 = comparisons.find({cj, ci});
                if (it2 != comparisons.end()) val = 1.0 / it2->second;
            }
            pairwise_matrix_[i][j] = val;
            pairwise_matrix_[j][i] = 1.0 / val;
        }
    }

    apply_auto_correction(pairwise_matrix_);
    std::vector<double> ev = calc_eigenvector(pairwise_matrix_);
    for (int i = 0; i < n; ++i) {
        criteria_weights_[criteria_names_[i]] = ev[i];
    }
    check_consistency(pairwise_matrix_, consistency_ratio_);
}

std::vector<double> AHPEvaluator::calc_eigenvector(const std::vector<std::vector<double>>& matrix) const {
    int n = matrix.size();
    std::vector<double> eigenvector(n, 1.0 / n);

    for (int iter = 0; iter < 200; ++iter) {
        std::vector<double> next(n, 0.0);
        double sum = 0.0;

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                next[i] += matrix[i][j] * eigenvector[j];
            }
            sum += next[i];
        }

        if (sum < 1e-12) break;
        for (int i = 0; i < n; ++i) next[i] /= sum;

        double diff = 0.0;
        for (int i = 0; i < n; ++i) diff += std::abs(next[i] - eigenvector[i]);

        eigenvector = next;
        if (diff < 1e-8) break;
    }
    return eigenvector;
}

double AHPEvaluator::calc_lambda_max(const std::vector<std::vector<double>>& matrix,
                                       const std::vector<double>& eigenvector) const {
    int n = matrix.size();
    double lambda_max = 0.0;
    for (int i = 0; i < n; ++i) {
        double row_sum = 0.0;
        for (int j = 0; j < n; ++j) row_sum += matrix[i][j] * eigenvector[j];
        if (eigenvector[i] > 1e-12) lambda_max += row_sum / eigenvector[i];
    }
    return lambda_max / n;
}

bool AHPEvaluator::check_consistency(const std::vector<std::vector<double>>& matrix,
                                      double& cr) const {
    int n = static_cast<int>(matrix.size());
    if (n < 3) { cr = 0.0; return true; }

    auto ev = calc_eigenvector(matrix);
    double lambda_max = calc_lambda_max(matrix, ev);
    double ci = (lambda_max - n) / std::max(n - 1, 1);
    double ri = (n <= 15) ? SAATY_RI[n] : 1.6;

    if (ri < 1e-6) { cr = 0.0; return true; }
    cr = ci / ri;
    return cr < 0.10;
}

double AHPEvaluator::calc_saaty_cr(const std::vector<std::vector<double>>& matrix) const {
    double cr = 0.0;
    check_consistency(matrix, cr);
    return cr;
}

// ============================================================
// 一致性自动修正：Saaty 矩阵迭代微调算法
// 对最不一致的 a_ij 向 w_i/w_j 方向小步修正
// ============================================================
void AHPEvaluator::apply_auto_correction(std::vector<std::vector<double>>& matrix, int max_iter) {
    int n = static_cast<int>(matrix.size());
    if (n < 3) return;

    for (int iter = 0; iter < max_iter; ++iter) {
        double cr = 0.0;
        if (check_consistency(matrix, cr) && cr < 0.08) break;

        auto w = calc_eigenvector(matrix);

        int worst_i = 0, worst_j = 0;
        double worst_dev = 0.0;

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                if (w[j] < 1e-12) continue;
                double ideal = w[i] / w[j];
                double dev = std::abs(std::log(matrix[i][j] / ideal));
                if (dev > worst_dev) {
                    worst_dev = dev;
                    worst_i = i;
                    worst_j = j;
                }
            }
        }

        if (w[worst_j] < 1e-12) break;
        double ideal = w[worst_i] / w[worst_j];
        double correction = 0.15;
        double new_val = matrix[worst_i][worst_j] * (1.0 - correction) + ideal * correction;
        new_val = std::max(1.0 / 9.0, std::min(9.0, new_val));

        matrix[worst_i][worst_j] = new_val;
        matrix[worst_j][worst_i] = 1.0 / new_val;
    }
}

// ============================================================
// 群决策算法：加权几何平均聚合 (WGGM)
// A_ij(群) = Π_k [ A_ij(k) ^ w_k ]   （对数最小二乘意义下最优）
// ============================================================
std::vector<std::vector<double>> AHPEvaluator::weighted_geometric_aggregation(
    const std::vector<ExpertOpinion>& experts) const {

    int n = static_cast<int>(experts.empty() ? 0 : experts[0].pairwise_matrix.size());
    std::vector<std::vector<double>> result(n, std::vector<double>(n, 1.0));

    if (experts.empty()) return result;

    double total_weight = 0.0;
    for (const auto& e : experts) total_weight += e.authority_weight;
    if (total_weight < 1e-12) total_weight = 1.0;

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) { result[i][j] = 1.0; continue; }

            double log_sum = 0.0;
            for (const auto& e : experts) {
                double aij = std::max(e.pairwise_matrix[i][j], 1e-6);
                log_sum += (e.authority_weight / total_weight) * std::log(aij);
            }
            result[i][j] = std::exp(log_sum);
        }
    }
    return result;
}

std::vector<std::vector<double>> AHPEvaluator::geometric_aggregation(
    const std::vector<ExpertOpinion>& experts) const {

    int n = static_cast<int>(experts.empty() ? 0 : experts[0].pairwise_matrix.size());
    std::vector<std::vector<double>> result(n, std::vector<double>(n, 1.0));
    if (experts.empty()) return result;

    int K = static_cast<int>(experts.size());
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            double log_sum = 0.0;
            for (const auto& e : experts) {
                log_sum += std::log(std::max(e.pairwise_matrix[i][j], 1e-6));
            }
            result[i][j] = std::exp(log_sum / K);
        }
    }
    return result;
}

// ============================================================
// 专家共识指数：基于权重向量间余弦相似度的平均值
// ============================================================
double AHPEvaluator::calc_consensus_index(const std::vector<ExpertOpinion>& experts) const {
    if (experts.size() < 2) return 1.0;

    std::vector<std::vector<double>> weight_vecs;
    for (const auto& e : experts) {
        weight_vecs.push_back(calc_eigenvector(e.pairwise_matrix));
    }

    double total_sim = 0.0;
    int pairs = 0;
    int K = static_cast<int>(experts.size());
    int n = static_cast<int>(weight_vecs[0].size());

    for (int a = 0; a < K; ++a) {
        for (int b = a + 1; b < K; ++b) {
            double dot = 0.0, na = 0.0, nb = 0.0;
            for (int i = 0; i < n; ++i) {
                dot += weight_vecs[a][i] * weight_vecs[b][i];
                na += weight_vecs[a][i] * weight_vecs[a][i];
                nb += weight_vecs[b][i] * weight_vecs[b][i];
            }
            double cos_sim = dot / (std::sqrt(na) * std::sqrt(nb) + 1e-12);
            total_sim += 0.5 + 0.5 * cos_sim;
            pairs++;
        }
    }
    return pairs > 0 ? total_sim / pairs : 1.0;
}

// ============================================================
// 生成 N 位专家判断矩阵（加入分歧 + 自修正）
// ============================================================
std::vector<ExpertOpinion> AHPEvaluator::generate_expert_opinions(int num_experts, double divergence) {
    static const char* names[] = {
        "张教授", "李研究员", "王工程师", "赵博士", "陈院士",
        "刘高工", "周总工", "吴教授", "郑研究员", "孙博士"
    };
    static const char* titles[] = {
        "军事史专家", "材料力学专家", "结构工程师", "考古学博士", "古建筑院士",
        "防护工程高工", "土木总工", "复合材料教授", "冲击力学研究员", "兵器博士"
    };

    int n = static_cast<int>(criteria_names_.size());
    std::mt19937 rng(42 + num_experts);
    std::normal_distribution<double> noise(0.0, divergence);
    std::uniform_real_distribution<double> authority(0.5, 1.0);

    std::vector<ExpertOpinion> experts;

    for (int k = 0; k < num_experts; ++k) {
        ExpertOpinion exp;
        exp.expert_name = names[k % 10];
        exp.expert_title = titles[k % 10];
        exp.authority_weight = authority(rng);
        exp.pairwise_matrix.assign(n, std::vector<double>(n, 1.0));

        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                double base = pairwise_matrix_[i][j];
                double jitter = std::exp(noise(rng));
                double raw = std::max(1.0 / 9.0, std::min(9.0, base * jitter));

                int rounded = static_cast<int>(std::round(raw * 10.0));
                double saaty_val = (rounded / 10.0);
                if (saaty_val < 1.0) saaty_val = std::max(1.0 / 9.0, 1.0 / (9.0 / saaty_val));
                saaty_val = std::max(1.0 / 9.0, std::min(9.0, saaty_val));

                exp.pairwise_matrix[i][j] = saaty_val;
                exp.pairwise_matrix[j][i] = 1.0 / saaty_val;
            }
        }

        apply_auto_correction(exp.pairwise_matrix, 80);
        exp.consistency_ratio = calc_saaty_cr(exp.pairwise_matrix);
        exp.is_consistent = exp.consistency_ratio < 0.10;
        experts.push_back(exp);
    }

    return experts;
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

void AHPEvaluator::enable_group_decision(bool enable) {
    group_decision_enabled_ = enable;
}

bool AHPEvaluator::is_group_decision_enabled() const {
    return group_decision_enabled_;
}

void AHPEvaluator::set_expert_count(int count) {
    expert_count_ = std::max(1, std::min(count, 10));
}

int AHPEvaluator::get_expert_count() const {
    return expert_count_;
}

GroupDecisionResult AHPEvaluator::get_group_decision_report() const {
    return last_group_result_;
}

double AHPEvaluator::normalize_value(double value, double min, double max, bool higher_is_better) const {
    if (std::abs(max - min) < 1e-10) return 0.5;
    double n = (value - min) / (max - min);
    return higher_is_better ? n : (1.0 - n);
}

double AHPEvaluator::calc_energy_absorption_score(const MaterialProperties& mat, double t) const {
    return normalize_value(mat.toughness_mj_m3 * t / 1000.0 * 1000.0, 10, 500, true);
}
double AHPEvaluator::calc_structural_strength_score(const MaterialProperties& mat, double t) const {
    return normalize_value(mat.ultimate_strength_mpa * t / 100.0, 5, 200, true);
}
double AHPEvaluator::calc_weight_factor_score(const MaterialProperties& mat, double t) const {
    return normalize_value(mat.density * t / 1000.0, 5, 500, false);
}
double AHPEvaluator::calc_cost_factor_score(const MaterialProperties& mat, double t) const {
    return normalize_value(mat.cost_per_unit * t / 10.0, 0.5, 100, false);
}
double AHPEvaluator::calc_durability_score(const MaterialProperties& mat, double t) const {
    double base = 0.5;
    if (mat.name == "wood") base = 0.6;
    else if (mat.name == "cowhide") base = 0.4;
    else if (mat.name == "iron") base = 0.9;
    else if (mat.name == "composite") base = 0.8;
    double f = normalize_value(t, 5, 100, true);
    return std::min(1.0, base * 0.6 + f * 0.4);
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
    (void)vehicle_id;

    ProtectionEvaluation eval{};
    eval.material_type = material_type;
    eval.material_thickness_mm = thickness_mm;

    auto it = mat_db.find(material_type);
    MaterialProperties mat = (it != mat_db.end()) ? it->second : mat_db["wood"];

    eval.energy_absorption_score   = calc_energy_absorption_score(mat, thickness_mm);
    eval.structural_strength_score = calc_structural_strength_score(mat, thickness_mm);
    eval.weight_factor_score       = calc_weight_factor_score(mat, thickness_mm);
    eval.cost_factor_score         = calc_cost_factor_score(mat, thickness_mm);
    eval.durability_score          = calc_durability_score(mat, thickness_mm);

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
    if (group_decision_enabled_ && expert_count_ > 1) {
        auto experts = generate_expert_opinions(expert_count_, 0.22);

        int passed = 0;
        for (const auto& e : experts) if (e.is_consistent) passed++;

        auto aggregated = weighted_geometric_aggregation(experts);
        apply_auto_correction(aggregated, 100);

        auto weights = calc_eigenvector(aggregated);
        double gcr = calc_saaty_cr(aggregated);
        double consensus = calc_consensus_index(experts);

        for (size_t i = 0; i < criteria_names_.size(); ++i) {
            criteria_weights_[criteria_names_[i]] = weights[i];
        }
        pairwise_matrix_ = aggregated;
        consistency_ratio_ = gcr;

        last_group_result_ = {
            weights, gcr, experts, consensus, passed,
            static_cast<int>(experts.size()), aggregated
        };
    }

    std::vector<ProtectionEvaluation> results;
    for (const auto& [material, base_thickness] : material_candidates_) {
        double thickness = base_thickness;
        if (material == "iron") thickness = 5.0;
        else if (material == "cowhide") thickness = 20.0;
        else if (material == "wood") thickness = reference_thickness_mm * 0.8;
        else if (material == "composite") thickness = reference_thickness_mm * 0.6;

        auto eval = evaluate_single(material, thickness, vehicle_id);
        eval.timestamp_ms = current_timestamp_ms();
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
