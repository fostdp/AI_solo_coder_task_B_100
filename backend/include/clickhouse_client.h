#pragma once

#include "common.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace fenyun {

class ClickHouseClient {
public:
    ClickHouseClient(const std::string& host, uint16_t port,
                     const std::string& user, const std::string& password,
                     const std::string& database);
    ~ClickHouseClient();

    bool connect();
    void disconnect();
    bool is_connected() const;

    bool insert_sensor_data(const SensorData& data);
    bool insert_sensor_batch(const std::vector<SensorData>& batch);

    bool insert_simulation_result(const SimulationResult& result);

    bool insert_alert_record(const AlertRecord& record);

    bool insert_protection_evaluation(const ProtectionEvaluation& eval);
    bool insert_protection_evaluation_batch(const std::vector<ProtectionEvaluation>& batch);

    std::vector<SensorData> query_sensor_data(uint32_t vehicle_id,
                                               int64_t start_ts_ms,
                                               int64_t end_ts_ms,
                                               int limit = 1000);

    std::vector<SimulationResult> query_simulation_results(uint32_t vehicle_id,
                                                            int limit = 100);

    std::vector<AlertRecord> query_alerts(uint32_t vehicle_id,
                                          int limit = 100);

    std::vector<ProtectionEvaluation> query_evaluations(uint32_t vehicle_id,
                                                         int limit = 50);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mutex_;
    std::string host_;
    uint16_t port_;
    std::string user_;
    std::string password_;
    std::string database_;
    bool connected_ = false;
};

}
