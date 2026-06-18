#pragma once

#include "common/data_types.h"
#include "common/lockfree_queue.h"
#include "config/config_loader.h"
#include "dtu_receiver/dtu_receiver.h"
#include "impact_simulator/impact_simulator.h"
#include "protection_optimizer/protection_optimizer.h"
#include "vehicle_comparator/vehicle_comparator.h"
#include "formation_optimizer/formation_optimizer.h"
#include "user_session/user_session_manager.h"
#include "era_comparator/era_comparator.h"
#include "vr_assault/vr_assault_engine.h"
#include "alarm_mqtt/alarm_mqtt.h"
#include "storage/clickhouse_client.h"

#include <memory>
#include <atomic>
#include <vector>
#include <thread>
#include <string>
#include <map>

namespace fenyun {

class FenyunApplication {
public:
    FenyunApplication();
    ~FenyunApplication();

    bool load_config(const std::string& config_dir);
    bool initialize();

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    std::shared_ptr<ConfigLoader> config_loader() { return config_; }
    std::shared_ptr<DtuReceiver> dtu_receiver() { return dtu_receiver_; }
    std::shared_ptr<ImpactSimulator> impact_simulator() { return impact_simulator_; }
    std::shared_ptr<ProtectionOptimizer> protection_optimizer() { return protection_optimizer_; }
    std::shared_ptr<VehicleComparator> vehicle_comparator() { return vehicle_comparator_; }
    std::shared_ptr<FormationOptimizer> formation_optimizer() { return formation_optimizer_; }
    std::shared_ptr<UserSessionManager> user_session_manager() { return user_session_manager_; }
    std::shared_ptr<EraComparator> era_comparator() { return era_comparator_; }
    std::shared_ptr<VRAssaultEngine> vr_assault_engine() { return vr_assault_engine_; }
    std::shared_ptr<AlarmMqtt> alarm_mqtt() { return alarm_mqtt_; }
    std::shared_ptr<ClickHouseClient> clickhouse_client() { return clickhouse_client_; }

    using SensorQueue = LockFreeQueue<SensorData>;
    using SimulationQueue = LockFreeQueue<SimulationResult>;
    using OptimizerQueue = LockFreeQueue<OptimizerTrigger>;
    using EvaluationQueue = LockFreeQueue<std::vector<ProtectionEvaluation>>;

    std::shared_ptr<SensorQueue> sensor_queue() { return sensor_queue_; }
    std::shared_ptr<SimulationQueue> simulation_queue() { return simulation_queue_; }
    std::shared_ptr<OptimizerQueue> optimizer_queue() { return optimizer_queue_; }
    std::shared_ptr<EvaluationQueue> evaluation_queue() { return evaluation_queue_; }

private:
    void setup_pipelines();

    void storage_writer_loop();
    void evaluation_writer_loop();
    void session_cleanup_loop();

    std::shared_ptr<ConfigLoader> config_;
    std::shared_ptr<DtuReceiver> dtu_receiver_;
    std::shared_ptr<ImpactSimulator> impact_simulator_;
    std::shared_ptr<ProtectionOptimizer> protection_optimizer_;
    std::shared_ptr<VehicleComparator> vehicle_comparator_;
    std::shared_ptr<FormationOptimizer> formation_optimizer_;
    std::shared_ptr<UserSessionManager> user_session_manager_;
    std::shared_ptr<EraComparator> era_comparator_;
    std::shared_ptr<VRAssaultEngine> vr_assault_engine_;
    std::shared_ptr<AlarmMqtt> alarm_mqtt_;
    std::shared_ptr<ClickHouseClient> clickhouse_client_;

    std::shared_ptr<SensorQueue> sensor_queue_;
    std::shared_ptr<SimulationQueue> simulation_queue_;
    std::shared_ptr<OptimizerQueue> optimizer_queue_;
    std::shared_ptr<EvaluationQueue> evaluation_queue_;

    std::atomic<bool> running_ {false};
    std::thread storage_writer_;
    std::thread eval_writer_;
    std::thread session_cleanup_;

    std::atomic<uint64_t> sims_stored_ {0};
    std::atomic<uint64_t> alerts_stored_ {0};
    std::atomic<uint64_t> evaluations_stored_ {0};
    std::atomic<uint64_t> sensors_stored_ {0};
};

}
