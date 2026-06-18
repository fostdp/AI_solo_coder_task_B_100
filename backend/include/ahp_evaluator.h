#pragma once

#include "common.h"
#include <vector>
#include <array>
#include <string>
#include <map>

namespace fenyun {

struct ExpertOpinion {
    std::string expert_name;
    std::string expert_title;
    double authority_weight;
    std::vector<std::vector<double>> pairwise_matrix;
    double consistency_ratio;
    bool is_consistent;
};

struct GroupDecisionResult {
    std::vector<double> aggregated_weights;
    double group_consistency_ratio;
    std::vector<ExpertOpinion> expert_reports;
    double expert_consensus_index;
    int passed_experts;
    int total_experts;
    std::vector<std::vector<double>> aggregated_pairwise_matrix;
};

class AHPEvaluator {
public:
    AHPEvaluator();

    std::vector<ProtectionEvaluation> evaluate_all(uint32_t vehicle_id,
                                                    double reference_thickness_mm = 80.0);

    ProtectionEvaluation evaluate_single(const std::string& material_type,
                                          double thickness_mm,
                                          uint32_t vehicle_id);

    void set_criteria_weights(const std::map<std::string, double>& weights);
    std::map<std::string, double> get_criteria_weights() const;

    std::vector<std::vector<double>> get_pairwise_matrix() const;
    double get_consistency_ratio() const;

    void enable_group_decision(bool enable);
    bool is_group_decision_enabled() const;

    void set_expert_count(int count);
    int get_expert_count() const;

    GroupDecisionResult get_group_decision_report() const;

    double calc_saaty_cr(const std::vector<std::vector<double>>& matrix) const;
    double calc_consensus_index(const std::vector<ExpertOpinion>& experts) const;

    std::vector<ExpertOpinion> generate_expert_opinions(int num_experts,
                                                         double divergence = 0.2);

private:
    void init_default_criteria();
    void build_pairwise_matrix();
    bool check_consistency(const std::vector<std::vector<double>>& matrix,
                           double& cr) const;

    std::vector<double> calc_eigenvector(const std::vector<std::vector<double>>& matrix) const;
    double calc_lambda_max(const std::vector<std::vector<double>>& matrix,
                           const std::vector<double>& eigenvector) const;

    std::vector<std::vector<double>> geometric_aggregation(
        const std::vector<ExpertOpinion>& experts) const;

    std::vector<std::vector<double>> weighted_geometric_aggregation(
        const std::vector<ExpertOpinion>& experts) const;

    void apply_auto_correction(std::vector<std::vector<double>>& matrix,
                                int max_iter = 50);

    double calc_energy_absorption_score(const MaterialProperties& mat, double thickness_mm) const;
    double calc_structural_strength_score(const MaterialProperties& mat, double thickness_mm) const;
    double calc_weight_factor_score(const MaterialProperties& mat, double thickness_mm) const;
    double calc_cost_factor_score(const MaterialProperties& mat, double thickness_mm) const;
    double calc_durability_score(const MaterialProperties& mat, double thickness_mm) const;

    double normalize_value(double value, double min, double max, bool higher_is_better) const;

    std::vector<std::string> criteria_names_;
    std::map<std::string, double> criteria_weights_;
    std::vector<std::vector<double>> pairwise_matrix_;
    double consistency_ratio_ = 0.0;

    bool group_decision_enabled_ = true;
    int expert_count_ = 5;
    GroupDecisionResult last_group_result_;

    std::vector<std::tuple<std::string, double>> material_candidates_;
};

}
