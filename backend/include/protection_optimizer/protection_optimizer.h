#pragma once

#include "common/data_types.h"
#include "common/lockfree_queue.h"
#include "common/json.h"
#include "config/config_loader.h"

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>

namespace fenyun {

struct ExpertOpinion {
    std::string name;
    std::string title;
    double authority_weight;
    std::vector<std::vector<double>> pairwise_matrix;
    double consistency_ratio;
    bool passed;
};

struct GroupDecisionResult {
    std::vector<double> aggregated_weights;
    double group_cr;
    double consensus_index;
    int passed_experts;
    int total_experts;
    std::vector<ExpertOpinion> experts;
    std::vector<std::vector<double>> aggregated_matrix;
};

struct OptimizerTrigger {
    uint32_t vehicle_id = 1;
    int64_t trigger_timestamp_ms = 0;
    std::string trigger_reason;
    bool use_group_decision = true;
    int expert_count = 5;
};

class ProtectionOptimizer {
public:
    using InputQueue = LockFreeQueue<OptimizerTrigger>;
    using OutputQueue = LockFreeQueue<std::vector<ProtectionEvaluation>>;

    explicit ProtectionOptimizer(std::shared_ptr<ConfigLoader> config);
    ~ProtectionOptimizer();

    void set_input_queue(std::shared_ptr<InputQueue> q);
    void set_output_queue(std::shared_ptr<OutputQueue> q);

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    std::vector<ProtectionEvaluation> evaluate_all(uint32_t vehicle_id, bool use_group = true, int experts = 5);
    GroupDecisionResult get_group_decision_report() const;

    std::vector<double> get_criteria_weights() const;
    double get_consistency_ratio() const;

    void enable_group_decision(bool enabled);
    bool is_group_decision_enabled() const { return group_decision_enabled_.load(); }

    void set_expert_count(int n);
    int get_expert_count() const { return expert_count_.load(); }

    uint64_t evaluations_run() const { return evals_run_.load(); }

    bool generate_expert_opinions(int n);

private:
    void worker_loop();

    std::vector<double> calc_eigenvector(const std::vector<std::vector<double>>& matrix) const;
    double calc_consistency_ratio(const std::vector<std::vector<double>>& matrix,
                                  const std::vector<double>& weights) const;

    double calc_consensus_index(const std::vector<ExpertOpinion>& experts) const;

    bool apply_auto_correction(std::vector<std::vector<double>>& matrix,
                               double target_cr,
                               int max_iter) const;

    std::vector<std::vector<double>> weighted_geometric_aggregation(
        const std::vector<ExpertOpinion>& experts) const;

    double evaluate_single_criterion(const MaterialProperties& mat,
                                     double thickness_mm,
                                     const std::string& criterion) const;

    std::shared_ptr<ConfigLoader> config_;

    std::shared_ptr<InputQueue> input_queue_;
    std::shared_ptr<OutputQueue> output_queue_;

    std::atomic<bool> running_ {false};
    std::thread worker_;

    std::atomic<bool> group_decision_enabled_ {true};
    std::atomic<int> expert_count_ {5};

    std::atomic<uint64_t> evals_run_ {0};
    std::atomic<uint64_t> eval_id_counter_ {0};

    mutable std::mutex mutex_;
    std::vector<ExpertOpinion> cached_experts_;
    GroupDecisionResult cached_group_result_;
    std::vector<double> current_criteria_weights_;
    double current_cr_ = 0.0;
    mutable std::mutex result_mutex_;
};

}
