#pragma once

#include "common.h"
#include <vector>
#include <array>
#include <string>
#include <map>

namespace fenyun {

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

private:
    void init_default_criteria();
    void build_pairwise_matrix();
    bool check_consistency(const std::vector<std::vector<double>>& matrix,
                           double& cr) const;

    std::vector<double> calc_eigenvector(const std::vector<std::vector<double>>& matrix) const;

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

    std::vector<std::tuple<std::string, double>> material_candidates_;

    static constexpr int RI_TABLE[10] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
};

}
