#pragma once

#include "common/data_types.h"
#include "config/config_loader.h"

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <random>

namespace fenyun {

enum class FormationType {
    LINE = 0,
    WEDGE = 1,
    ECHELON = 2,
    V_SHAPE = 3,
    COLUMN = 4,
    DIAMOND = 5
};

class FormationOptimizer {
public:
    explicit FormationOptimizer(std::shared_ptr<ConfigLoader> config);
    ~FormationOptimizer();

    FormationOptimizationResult optimize(const FormationOptimizationRequest& req);

    std::vector<FormationConfig> generate_candidates(const FormationOptimizationRequest& req,
                                                      int candidate_count = 8);

    std::vector<FormationVehicle> layout_formation(const FormationConfig& config);

    double evaluate_formation(const FormationConfig& config,
                               const FormationOptimizationRequest& req) const;

    FormationConfig get_formation_template(const std::string& type_name, int vehicle_count);

    uint64_t optimizations_run() const { return optimizations_run_.load(); }

private:
    double calc_survival_probability(const FormationConfig& config,
                                      double rock_fall_rate,
                                      double avg_rock_mass) const;

    double calc_coverage_score(const FormationConfig& config,
                                double wall_length,
                                double wall_height) const;

    double calc_progress_rate(const FormationConfig& config) const;

    FormationVehicle create_vehicle_in_formation(int index,
                                                  FormationType type,
                                                  int total,
                                                  double spacing,
                                                  const std::string& vehicle_type,
                                                  double wall_distance);

    FormationType formation_type_from_string(const std::string& s) const;
    std::string formation_type_to_string(FormationType t) const;

    std::shared_ptr<ConfigLoader> config_;

    std::atomic<uint64_t> optimizations_run_ {0};
    std::atomic<uint64_t> optimization_id_counter_ {0};
    mutable std::mutex mutex_;
    mutable std::mt19937 rng_ {std::random_device{}()};
};

}
