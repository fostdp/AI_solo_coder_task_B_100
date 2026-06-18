#include "protection_optimizer/protection_optimizer.h"

#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>
#include <numeric>

namespace fenyun {

static const double RI_SAATY[] = {
    0.00, 0.00, 0.58, 0.90, 1.12, 1.24, 1.32, 1.41, 1.45, 1.49,
    1.51, 1.48, 1.56, 1.57, 1.59
};

ProtectionOptimizer::ProtectionOptimizer(std::shared_ptr<ConfigLoader> config)
    : config_(std::move(config)) {
    if (config_) {
        const auto& gc = config_->ahp_config().group_decision;
        group_decision_enabled_.store(gc.enabled);
        expert_count_.store(gc.default_expert_count);
    }
    auto weights = get_criteria_weights();
    current_criteria_weights_ = weights;
}

ProtectionOptimizer::~ProtectionOptimizer() {
    stop();
}

void ProtectionOptimizer::set_input_queue(std::shared_ptr<InputQueue> q) {
    input_queue_ = std::move(q);
}

void ProtectionOptimizer::set_output_queue(std::shared_ptr<OutputQueue> q) {
    output_queue_ = std::move(q);
}

void ProtectionOptimizer::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread(&ProtectionOptimizer::worker_loop, this);
}

void ProtectionOptimizer::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

void ProtectionOptimizer::worker_loop() {
    OptimizerTrigger trigger{};
    while (running_.load()) {
        if (input_queue_ && input_queue_->pop(trigger)) {
            auto results = evaluate_all(trigger.vehicle_id,
                                         trigger.use_group_decision,
                                         trigger.expert_count);
            if (output_queue_) {
                std::vector<ProtectionEvaluation> copy = results;
                output_queue_->push(std::move(copy));
            }
            evals_run_.fetch_add(1);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void ProtectionOptimizer::enable_group_decision(bool enabled) {
    group_decision_enabled_.store(enabled);
}

void ProtectionOptimizer::set_expert_count(int n) {
    if (n < 1) n = 1;
    if (n > 10) n = 10;
    expert_count_.store(n);
}

std::vector<double> ProtectionOptimizer::calc_eigenvector(
    const std::vector<std::vector<double>>& matrix) const {
    int n = static_cast<int>(matrix.size());
    if (n == 0) return {};

    std::vector<double> w(n, 1.0 / n);
    std::vector<double> w_new(n);

    const int max_iter = 100;
    const double eps = 1e-8;

    for (int iter = 0; iter < max_iter; ++iter) {
        for (int i = 0; i < n; ++i) {
            double sum = 0.0;
            for (int j = 0; j < n; ++j) {
                sum += matrix[i][j] * w[j];
            }
            w_new[i] = sum;
        }
        double norm = 0.0;
        for (double v : w_new) norm += v;
        if (norm > 0) {
            for (double& v : w_new) v /= norm;
        }
        double diff = 0.0;
        for (int i = 0; i < n; ++i) {
            diff += std::abs(w_new[i] - w[i]);
        }
        w.swap(w_new);
        if (diff < eps) break;
    }
    return w;
}

double ProtectionOptimizer::calc_consistency_ratio(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& weights) const {
    int n = static_cast<int>(matrix.size());
    if (n <= 2) return 0.0;

    std::vector<double> aw(n, 0.0);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            aw[i] += matrix[i][j] * weights[j];
        }
    }

    double lambda_max = 0.0;
    for (int i = 0; i < n; ++i) {
        if (weights[i] > 1e-12) {
            lambda_max += aw[i] / weights[i];
        }
    }
    lambda_max /= n;

    if (n <= 1) return 0.0;
    double ci = (lambda_max - n) / (n - 1);
    double ri = (n <= 15) ? RI_SAATY[n - 1] : 1.59;
    if (ri < 1e-12) return 0.0;
    return ci / ri;
}

double ProtectionOptimizer::calc_consensus_index(
    const std::vector<ExpertOpinion>& experts) const {
    if (experts.size() < 2) return 1.0;

    std::vector<std::vector<double>> weight_vectors;
    for (const auto& e : experts) {
        weight_vectors.push_back(calc_eigenvector(e.pairwise_matrix));
    }

    double total_sim = 0.0;
    int pairs = 0;
    int n = static_cast<int>(weight_vectors.size());
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dot = 0.0, ni = 0.0, nj = 0.0;
            for (size_t k = 0; k < weight_vectors[i].size(); ++k) {
                dot += weight_vectors[i][k] * weight_vectors[j][k];
                ni += weight_vectors[i][k] * weight_vectors[i][k];
                nj += weight_vectors[j][k] * weight_vectors[j][k];
            }
            double cos = 0.0;
            if (ni > 1e-12 && nj > 1e-12) {
                cos = dot / (std::sqrt(ni) * std::sqrt(nj));
            }
            total_sim += 0.5 + 0.5 * cos;
            pairs++;
        }
    }
    return pairs > 0 ? total_sim / pairs : 0.0;
}

bool ProtectionOptimizer::apply_auto_correction(std::vector<std::vector<double>>& matrix,
                                                 double target_cr,
                                                 int max_iter) const {
    int n = static_cast<int>(matrix.size());
    if (n <= 2) return true;

    auto ahp_cfg = config_ ? config_->ahp_config() : AHPConfig{};
    double step_ratio = ahp_cfg.consistency.correction_step_ratio;
    if (step_ratio < 0.01) step_ratio = 0.15;

    for (int iter = 0; iter < max_iter; ++iter) {
        auto weights = calc_eigenvector(matrix);
        double cr = calc_consistency_ratio(matrix, weights);
        if (cr <= target_cr) return true;

        int worst_i = 0, worst_j = 0;
        double worst_deviation = 0.0;

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                if (weights[j] < 1e-12) continue;
                double ideal = weights[i] / weights[j];
                double actual = matrix[i][j];
                double deviation = std::abs(actual - ideal) / ideal;
                if (deviation > worst_deviation) {
                    worst_deviation = deviation;
                    worst_i = i;
                    worst_j = j;
                }
            }
        }

        if (weights[worst_j] < 1e-12) break;
        double ideal = weights[worst_i] / weights[worst_j];
        double& a_ij = matrix[worst_i][worst_j];
        double delta = (ideal - a_ij) * step_ratio;
        a_ij += delta;

        if (a_ij < 1.0 / 9.0) a_ij = 1.0 / 9.0;
        if (a_ij > 9.0) a_ij = 9.0;

        matrix[worst_j][worst_i] = 1.0 / a_ij;
    }

    auto weights = calc_eigenvector(matrix);
    return calc_consistency_ratio(matrix, weights) <= target_cr * 1.5;
}

std::vector<std::vector<double>> ProtectionOptimizer::weighted_geometric_aggregation(
    const std::vector<ExpertOpinion>& experts) const {
    if (experts.empty()) return {};
    int n = static_cast<int>(experts.front().pairwise_matrix.size());

    double total_weight = 0.0;
    for (const auto& e : experts) total_weight += e.authority_weight;
    if (total_weight < 1e-12) total_weight = 1.0;

    std::vector<std::vector<double>> result(n, std::vector<double>(n, 1.0));

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) { result[i][j] = 1.0; continue; }
            double log_sum = 0.0;
            for (const auto& e : experts) {
                double w_norm = e.authority_weight / total_weight;
                log_sum += w_norm * std::log(std::max(e.pairwise_matrix[i][j], 1e-9));
            }
            result[i][j] = std::exp(log_sum);
        }
    }
    return result;
}

bool ProtectionOptimizer::generate_expert_opinions(int n) {
    if (!config_) return false;

    const auto& ahp = config_->ahp_config();
    const auto& ref_matrix = ahp.reference_pairwise_matrix;
    if (ref_matrix.empty()) return false;

    const auto& pool = ahp.group_decision.experts_pool;
    double divergence = ahp.group_decision.expert_divergence;
    double target_cr = ahp.consistency.auto_correction_cr_target;
    int max_corr = ahp.consistency.max_correction_iterations;

    std::vector<ExpertOpinion> experts;
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::normal_distribution<double> noise(0.0, divergence);

    for (int i = 0; i < n; ++i) {
        ExpertOpinion exp;
        if (i < static_cast<int>(pool.size())) {
            exp.name = pool[i].name;
            exp.title = pool[i].title;
            exp.authority_weight = pool[i].authority_weight;
        } else {
            exp.name = "专家" + std::to_string(i + 1);
            exp.title = "综合评审";
            exp.authority_weight = 0.75 + (i % 5) * 0.05;
        }

        exp.pairwise_matrix = ref_matrix;
        int dim = static_cast<int>(ref_matrix.size());
        for (int r = 0; r < dim; ++r) {
            for (int c = r + 1; c < dim; ++c) {
                double factor = std::exp(noise(rng));
                exp.pairwise_matrix[r][c] *= factor;
                if (exp.pairwise_matrix[r][c] < 1.0 / 9.0) exp.pairwise_matrix[r][c] = 1.0 / 9.0;
                if (exp.pairwise_matrix[r][c] > 9.0) exp.pairwise_matrix[r][c] = 9.0;
                exp.pairwise_matrix[c][r] = 1.0 / exp.pairwise_matrix[r][c];
            }
        }

        if (ahp.consistency.auto_correction) {
            exp.passed = apply_auto_correction(exp.pairwise_matrix, target_cr, max_corr);
        } else {
            auto w = calc_eigenvector(exp.pairwise_matrix);
            exp.passed = calc_consistency_ratio(exp.pairwise_matrix, w) <= ahp.consistency.cr_threshold;
        }
        auto w = calc_eigenvector(exp.pairwise_matrix);
        exp.consistency_ratio = calc_consistency_ratio(exp.pairwise_matrix, w);

        experts.push_back(std::move(exp));
    }

    std::lock_guard<std::mutex> lock(result_mutex_);
    cached_experts_ = std::move(experts);
    return true;
}

GroupDecisionResult ProtectionOptimizer::get_group_decision_report() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return cached_group_result_;
}

std::vector<double> ProtectionOptimizer::get_criteria_weights() const {
    if (!config_) return {};

    auto ahp_cfg = config_->ahp_config();
    if (group_decision_enabled_.load() && expert_count_.load() > 1) {
        if (cached_experts_.empty()) {
            auto* self = const_cast<ProtectionOptimizer*>(this);
            self->generate_expert_opinions(expert_count_.load());
        }
        auto agg = weighted_geometric_aggregation(cached_experts_);
        apply_auto_correction(agg, ahp_cfg.consistency.auto_correction_cr_target,
                              ahp_cfg.consistency.max_correction_iterations);
        auto weights = calc_eigenvector(agg);

        GroupDecisionResult gr;
        gr.aggregated_weights = weights;
        gr.group_cr = calc_consistency_ratio(agg, weights);
        gr.consensus_index = calc_consensus_index(cached_experts_);
        gr.experts = cached_experts_;
        gr.total_experts = static_cast<int>(cached_experts_.size());
        gr.passed_experts = 0;
        for (const auto& e : cached_experts_) if (e.passed) gr.passed_experts++;
        gr.aggregated_matrix = agg;

        auto* self = const_cast<ProtectionOptimizer*>(this);
        std::lock_guard<std::mutex> lock(self->result_mutex_);
        self->cached_group_result_ = gr;
        return weights;
    }

    const auto& mat = ahp_cfg.reference_pairwise_matrix;
    auto matrix = mat;
    if (ahp_cfg.consistency.auto_correction) {
        apply_auto_correction(matrix, ahp_cfg.consistency.auto_correction_cr_target,
                              ahp_cfg.consistency.max_correction_iterations);
    }
    return calc_eigenvector(matrix);
}

double ProtectionOptimizer::get_consistency_ratio() const {
    auto w = get_criteria_weights();
    if (!config_ || w.empty()) return 0.0;
    auto ahp_cfg = config_->ahp_config();

    if (group_decision_enabled_.load() && expert_count_.load() > 1) {
        std::lock_guard<std::mutex> lock(result_mutex_);
        return cached_group_result_.group_cr;
    }

    return calc_consistency_ratio(ahp_cfg.reference_pairwise_matrix, w);
}

double ProtectionOptimizer::evaluate_single_criterion(const MaterialProperties& mat,
                                                       double thickness_mm,
                                                       const std::string& criterion) const {
    if (criterion == "energy_absorption") {
        double sea = mat.specific_energy_absorption_kj_kg;
        double mass = mat.density * (thickness_mm / 1000.0) * 1.0;
        double total_sea = sea * mass * 0.1;
        return std::min(1.0, total_sea / 100.0);
    }
    if (criterion == "structural_strength") {
        double strength = mat.ultimate_strength_mpa;
        double bending_stiffness = mat.youngs_modulus_gpa * std::pow(thickness_mm / 1000.0, 3) / 12.0;
        double score = 0.5 * (strength / 400.0) + 0.5 * std::min(1.0, bending_stiffness * 100.0);
        return std::min(1.0, std::max(0.0, score));
    }
    if (criterion == "weight_factor") {
        double mass = mat.density * (thickness_mm / 1000.0) * 6.5 * 2.8;
        return std::min(1.0, std::max(0.0, 1.0 - mass / 1000.0));
    }
    if (criterion == "cost_factor") {
        double cost = mat.cost_per_unit * thickness_mm / 10.0;
        return std::min(1.0, std::max(0.0, 1.0 - cost / 20.0));
    }
    if (criterion == "durability") {
        return std::min(1.0, std::max(0.0, mat.durability_base));
    }
    return 0.5;
}

std::vector<ProtectionEvaluation> ProtectionOptimizer::evaluate_all(
    uint32_t vehicle_id, bool use_group, int experts) {
    if (use_group && experts > 1) {
        enable_group_decision(true);
        set_expert_count(experts);
        generate_expert_opinions(experts);
    } else {
        enable_group_decision(false);
    }

    auto crit_weights = get_criteria_weights();
    const auto& ahp_cfg = config_->ahp_config();
    const auto& criteria = ahp_cfg.criteria;

    std::vector<ProtectionEvaluation> results;

    for (const auto& scheme : ahp_cfg.evaluation_schemes) {
        MaterialProperties mat = config_->get_material(scheme.material_type);
        ProtectionEvaluation eval{};
        eval.eval_id = eval_id_counter_.fetch_add(1) + 1;
        eval.vehicle_id = vehicle_id;
        eval.timestamp_ms = current_timestamp_ms();
        eval.material_type = scheme.material_type;
        eval.material_thickness_mm = scheme.thickness_mm;

        eval.energy_absorption_score = evaluate_single_criterion(mat, scheme.thickness_mm, "energy_absorption");
        eval.structural_strength_score = evaluate_single_criterion(mat, scheme.thickness_mm, "structural_strength");
        eval.weight_factor_score = evaluate_single_criterion(mat, scheme.thickness_mm, "weight_factor");
        eval.cost_factor_score = evaluate_single_criterion(mat, scheme.thickness_mm, "cost_factor");
        eval.durability_score = evaluate_single_criterion(mat, scheme.thickness_mm, "durability");

        double total = 0.0;
        for (size_t k = 0; k < crit_weights.size() && k < criteria.size(); ++k) {
            double score = 0.0;
            if (criteria[k] == "energy_absorption") score = eval.energy_absorption_score;
            else if (criteria[k] == "structural_strength") score = eval.structural_strength_score;
            else if (criteria[k] == "weight_factor") score = eval.weight_factor_score;
            else if (criteria[k] == "cost_factor") score = eval.cost_factor_score;
            else if (criteria[k] == "durability") score = eval.durability_score;
            total += crit_weights[k] * score;
        }
        eval.ahp_weight_score = total;
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

    current_cr_ = get_consistency_ratio();
    current_criteria_weights_ = crit_weights;

    return results;
}

}
