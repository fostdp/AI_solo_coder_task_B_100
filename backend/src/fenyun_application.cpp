#include "fenyun_application.h"

#include <iostream>
#include <filesystem>

namespace fenyun {

FenyunApplication::FenyunApplication() = default;

FenyunApplication::~FenyunApplication() {
    stop();
}

bool FenyunApplication::load_config(const std::string& config_dir) {
    std::string sys_path = config_dir + "/system.json";
    std::string mat_path = config_dir + "/materials.json";
    std::string ahp_path = config_dir + "/ahp_weights.json";

    config_ = std::make_shared<ConfigLoader>();
    if (!config_->load_from_file(sys_path, mat_path, ahp_path)) {
        std::cerr << "[Application] Failed to load config from " << config_dir << std::endl;
        return false;
    }

    std::cout << "[Application] Config loaded: " << config_->materials().size()
              << " materials, " << config_->ahp_config().criteria.size()
              << " AHP criteria" << std::endl;
    return true;
}

bool FenyunApplication::initialize() {
    if (!config_) {
        std::cerr << "[Application] Config not loaded" << std::endl;
        return false;
    }

    const auto& sc = config_->system_config();

    sensor_queue_ = std::make_shared<SensorQueue>(sc.simulation.queue_capacity);
    simulation_queue_ = std::make_shared<SimulationQueue>(sc.alarm.queue_capacity);
    optimizer_queue_ = std::make_shared<OptimizerQueue>(sc.optimizer.queue_capacity);
    evaluation_queue_ = std::make_shared<EvaluationQueue>(sc.optimizer.queue_capacity);

    dtu_receiver_ = std::make_shared<DtuReceiver>(config_);
    dtu_receiver_->set_output_queue(sensor_queue_);
    dtu_receiver_->set_validation_callback([](DtuValidationResult r, const SensorData& d) {
        if (r != DtuValidationResult::OK) {
            std::cerr << "[DTU] Validation failed for vehicle " << d.vehicle_id
                      << ": code " << static_cast<int>(r) << std::endl;
        }
    });

    impact_simulator_ = std::make_shared<ImpactSimulator>(config_);
    impact_simulator_->set_input_queue(sensor_queue_);
    impact_simulator_->set_output_queue(simulation_queue_);

    protection_optimizer_ = std::make_shared<ProtectionOptimizer>(config_);
    protection_optimizer_->set_input_queue(optimizer_queue_);
    protection_optimizer_->set_output_queue(evaluation_queue_);

    alarm_mqtt_ = std::make_shared<AlarmMqtt>(config_);
    alarm_mqtt_->set_input_queue(simulation_queue_);
    alarm_mqtt_->set_alert_callback([](const AlertRecord& a) {
        (void)a;
    });

    clickhouse_client_ = std::make_shared<ClickHouseClient>(config_);

    std::cout << "[Application] Modules initialized" << std::endl;
    return true;
}

void FenyunApplication::setup_pipelines() {
}

void FenyunApplication::start() {
    if (running_.exchange(true)) return;

    if (clickhouse_client_) {
        clickhouse_client_->connect();
    }

    if (impact_simulator_) impact_simulator_->start();
    if (protection_optimizer_) protection_optimizer_->start();
    if (alarm_mqtt_) alarm_mqtt_->start();

    storage_writer_ = std::thread(&FenyunApplication::storage_writer_loop, this);
    eval_writer_ = std::thread(&FenyunApplication::evaluation_writer_loop, this);

    std::cout << "[Application] All modules started" << std::endl;
}

void FenyunApplication::stop() {
    if (!running_.exchange(false)) return;

    if (impact_simulator_) impact_simulator_->stop();
    if (protection_optimizer_) protection_optimizer_->stop();
    if (alarm_mqtt_) alarm_mqtt_->stop();

    if (storage_writer_.joinable()) storage_writer_.join();
    if (eval_writer_.joinable()) eval_writer_.join();

    if (clickhouse_client_) clickhouse_client_->disconnect();

    std::cout << "[Application] All modules stopped" << std::endl;
    std::cout << "[Application] Stats: sensors=" << sensors_stored_.load()
              << " sims=" << sims_stored_.load()
              << " alerts=" << alerts_stored_.load()
              << " evals=" << evaluations_stored_.load() << std::endl;
}

void FenyunApplication::storage_writer_loop() {
    SimulationResult sim{};
    std::vector<SimulationResult> buffer;
    buffer.reserve(32);

    while (running_.load()) {
        bool has_data = false;
        if (simulation_queue_ && simulation_queue_->pop(sim)) {
            has_data = true;
            if (clickhouse_client_ && clickhouse_client_->is_connected()) {
                clickhouse_client_->insert_simulation_result(sim);
                sims_stored_.fetch_add(1);
            }
        }

        SensorData sd{};
        if (sensor_queue_) {
            // 注意：SensorData 已经被 impact_simulator 消费了，
            // 这里我们通过 dtu_receiver 直接落库（旁路写）
        }

        if (!has_data) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

void FenyunApplication::evaluation_writer_loop() {
    std::vector<ProtectionEvaluation> evals;
    while (running_.load()) {
        if (evaluation_queue_ && evaluation_queue_->pop(evals)) {
            if (clickhouse_client_ && clickhouse_client_->is_connected()) {
                for (const auto& e : evals) {
                    clickhouse_client_->insert_evaluation(e);
                }
                evaluations_stored_.fetch_add(evals.size());
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

}
