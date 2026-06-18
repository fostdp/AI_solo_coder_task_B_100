#pragma once

#include "common.h"
#include "structural_simulation.h"
#include "ahp_evaluator.h"
#include "clickhouse_client.h"
#include "mqtt_alert_manager.h"

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

namespace fenyun {

struct ServerConfig {
    std::string http_host = "0.0.0.0";
    uint16_t http_port = 8080;

    std::string clickhouse_host = "127.0.0.1";
    uint16_t clickhouse_port = 9000;
    std::string clickhouse_user = "default";
    std::string clickhouse_password = "";
    std::string clickhouse_database = "fenyun_vehicle";

    std::string mqtt_broker = "tcp://127.0.0.1:1883";
    std::string mqtt_client_id = "fenyun_backend";
    std::string mqtt_username = "";
    std::string mqtt_password = "";

    double deformation_threshold_mm = 15.0;
    double penetration_threshold_ratio = 0.9;

    int simulation_threads = 2;
    int alert_threads = 1;
};

class FenYunServer {
public:
    explicit FenYunServer(const ServerConfig& config);
    ~FenYunServer();

    bool start();
    void stop();
    bool is_running() const;

    bool ingest_sensor_data(const SensorData& data);
    std::vector<ProtectionEvaluation> run_ahp_evaluation(uint32_t vehicle_id);

    std::vector<SensorData> get_sensor_history(uint32_t vehicle_id,
                                                int64_t start_ts,
                                                int64_t end_ts,
                                                int limit);
    std::vector<SimulationResult> get_simulation_history(uint32_t vehicle_id, int limit);
    std::vector<AlertRecord> get_alert_history(uint32_t vehicle_id, int limit);

    ServerConfig get_config() const;

private:
    void simulation_worker();
    void alert_worker();
    void http_server_worker();

    void process_sensor_data(const SensorData& data);

    ServerConfig config_;
    std::atomic<bool> running_ {false};

    std::unique_ptr<ClickHouseClient> db_client_;
    std::unique_ptr<MQTTAlertManager> mqtt_manager_;
    std::unique_ptr<StructuralSimulation> simulator_;
    std::unique_ptr<AHPEvaluator> ahp_evaluator_;

    std::vector<std::thread> sim_threads_;
    std::vector<std::thread> alert_threads_;
    std::thread http_thread_;

    std::queue<SensorData> sim_queue_;
    std::mutex sim_queue_mutex_;
    std::condition_variable sim_queue_cv_;

    struct AlertTask {
        SensorData sensor;
        SimulationResult sim;
        uint64_t alert_id;
    };
    std::queue<AlertTask> alert_queue_;
    std::mutex alert_queue_mutex_;
    std::condition_variable alert_queue_cv_;

    std::atomic<uint64_t> simulation_id_counter_ {1};
    std::atomic<uint64_t> alert_id_counter_ {1};

    struct HttpImpl;
    std::unique_ptr<HttpImpl> http_impl_;
};

}
