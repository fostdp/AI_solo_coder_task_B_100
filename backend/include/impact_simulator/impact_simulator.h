#pragma once

#include "common/data_types.h"
#include "common/lockfree_queue.h"
#include "config/config_loader.h"

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <condition_variable>

namespace fenyun {

struct ImpactSimulatorConfig {
    double roof_length_m = 6.5;
    double roof_width_m = 2.8;
    int grid_size = 10;
    int jc_bisection_iterations = 60;
    int worker_threads = 2;
    double default_temperature_K = 293.15;
};

class ImpactSimulator {
public:
    using InputQueue = LockFreeQueue<SensorData>;
    using OutputQueue = LockFreeQueue<SimulationResult>;

    explicit ImpactSimulator(std::shared_ptr<ConfigLoader> config);
    ~ImpactSimulator();

    void set_input_queue(std::shared_ptr<InputQueue> q);
    void set_output_queue(std::shared_ptr<OutputQueue> q);

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    SimulationResult run_simulation(const SensorData& data);

    void set_strain_rate(double sr) { default_strain_rate_ = sr; }
    void set_temperature(double T_K) { default_temperature_K_ = T_K; }

    JohnsonCookParams get_jc_params(const std::string& material) const;

    uint64_t simulations_run() const { return sims_run_.load(); }
    uint64_t output_dropped() const { return output_dropped_.load(); }

    struct PlasticityTask {
        uint64_t task_id;
        SensorData sensor_data;
        MaterialProperties material;
        JohnsonCookParams jc;
        std::atomic<bool> completed {false};
        SimulationResult result;
        std::atomic<bool> cancelled {false};
    };

    using PlasticityTaskPtr = std::shared_ptr<PlasticityTask>;

    PlasticityTaskPtr submit_plasticity_task(const SensorData& data);
    bool is_task_complete(uint64_t task_id) const;
    SimulationResult get_task_result(uint64_t task_id, int timeout_ms = -1);
    size_t pending_tasks() const;
    void set_thread_pool_size(size_t n);
    void cleanup_completed_tasks();

private:
    void worker_loop(int thread_id);
    void worker_thread();
    PlasticityTaskPtr pop_next_task();
    void run_plasticity_calc(PlasticityTask& task);
    void start_workers();
    void stop_workers();
    void remove_task(uint64_t task_id);

    double calc_johnson_cook_flow_stress(const std::string& material,
                                         const JohnsonCookParams& jc,
                                         double plastic_strain,
                                         double strain_rate,
                                         double temperature_K) const;

    double calc_plastic_strain_jc(const std::string& material,
                                  double von_mises_stress_pa,
                                  double strain_rate,
                                  double temperature_K) const;

    double calc_penetration_depth_jc(const std::string& material,
                                     double impact_energy_j,
                                     double thickness_m,
                                     double strain_rate,
                                     double temperature_K) const;

    void generate_deformation_field(std::vector<double>& field,
                                    double max_deformation,
                                    double impact_x,
                                    double impact_y) const;

    void generate_stress_field(std::vector<double>& field,
                               double max_stress,
                               double impact_x,
                               double impact_y) const;

    MaterialProperties get_material(const std::string& name) const;

    std::shared_ptr<ConfigLoader> config_;
    ImpactSimulatorConfig sim_config_;

    std::shared_ptr<InputQueue> input_queue_;
    std::shared_ptr<OutputQueue> output_queue_;

    std::atomic<bool> running_ {false};
    std::vector<std::thread> workers_;

    std::atomic<uint64_t> sims_run_ {0};
    std::atomic<uint64_t> output_dropped_ {0};
    std::atomic<uint64_t> sim_id_counter_ {0};

    double default_strain_rate_ = 100.0;
    double default_temperature_K_ = 293.15;

    mutable std::mutex task_mutex_;
    std::unordered_map<uint64_t, PlasticityTaskPtr> tasks_;
    std::vector<std::thread> worker_threads_;
    std::condition_variable task_cv_;
    std::deque<uint64_t> task_queue_;
    std::atomic<uint64_t> next_task_id_ {1};
    std::atomic<bool> workers_running_ {false};
    size_t thread_pool_size_ = 2;
};

}
