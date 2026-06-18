#pragma once

#include "common/data_types.h"
#include "config/config_loader.h"

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace fenyun {

class ClickHouseClient {
public:
    explicit ClickHouseClient(std::shared_ptr<ConfigLoader> config);
    ~ClickHouseClient();

    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }

    bool insert_sensor_data(const SensorData& data);
    bool insert_sensor_batch(const std::vector<SensorData>& batch);

    bool insert_simulation_result(const SimulationResult& result);
    bool insert_alert_record(const AlertRecord& alert);
    bool insert_evaluation(const ProtectionEvaluation& eval);

    std::vector<SensorData> query_sensor_history(uint32_t vehicle_id,
                                                  int64_t start_ms,
                                                  int64_t end_ms,
                                                  int limit);

    std::vector<SimulationResult> query_simulation_results(uint32_t vehicle_id, int limit);
    std::vector<AlertRecord> query_alerts(uint32_t vehicle_id, int limit);
    std::vector<ProtectionEvaluation> query_evaluations(uint32_t vehicle_id, int limit);

private:
    bool send_http_request(const std::string& method,
                           const std::string& path,
                           const std::string& body,
                           std::string& response);

    std::string build_insert_sql(const std::string& table, const std::string& format);

    static std::string escape_for_sql(const std::string& s);

    std::shared_ptr<ConfigLoader> config_;
    std::string host_;
    uint16_t port_ = 8123;
    std::string database_;
    std::string user_;
    std::string password_;
    int sock_fd_ = -1;
    bool connected_ = false;
};

}
