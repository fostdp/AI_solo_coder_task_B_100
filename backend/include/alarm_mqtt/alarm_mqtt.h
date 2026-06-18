#pragma once

#include "common/data_types.h"
#include "common/lockfree_queue.h"
#include "config/config_loader.h"

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>

namespace fenyun {

enum class AlertType {
    DEFORMATION_EXCEEDED = 0,
    PENETRATION = 1,
    STRESS_EXCEEDED = 2,
};

struct AlertThresholds {
    double deformation_warn_mm = 10.5;
    double deformation_critical_mm = 15.0;
    double penetration_ratio_warn = 0.7;
    double penetration_ratio_critical = 0.9;
    double stress_warn_mpa = 140.0;
    double stress_critical_mpa = 200.0;
};

class AlarmMqtt {
public:
    using InputQueue = LockFreeQueue<SimulationResult>;

    explicit AlarmMqtt(std::shared_ptr<ConfigLoader> config);
    ~AlarmMqtt();

    void set_input_queue(std::shared_ptr<InputQueue> q);

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    bool connect();
    void disconnect();
    bool is_connected() const { return connected_.load(); }

    std::vector<AlertRecord> check_and_alert(const SimulationResult& sim,
                                              const SensorData& sensor);

    void set_alert_callback(AlertCallback cb) { alert_callback_ = std::move(cb); }

    uint64_t alerts_published() const { return alerts_published_.load(); }
    uint64_t checks_performed() const { return checks_performed_.load(); }

    void set_thresholds(const AlertThresholds& t) { thresholds_ = t; }
    AlertThresholds thresholds() const { return thresholds_; }

    static std::string alert_type_to_string(AlertType type);
    static uint8_t alert_level(uint8_t level) { return level; }

private:
    void worker_loop();

    bool publish_alert(const AlertRecord& alert);

    std::string build_topic(uint32_t vehicle_id, AlertType type) const;
    std::string build_message_id(uint64_t alert_id) const;

    AlertRecord make_alert(uint32_t vehicle_id,
                           int64_t timestamp_ms,
                           AlertType type,
                           uint8_t level,
                           const std::string& message,
                           double measured,
                           double threshold) const;

    std::shared_ptr<ConfigLoader> config_;
    AlertThresholds thresholds_;

    std::shared_ptr<InputQueue> input_queue_;

    std::atomic<bool> running_ {false};
    std::atomic<bool> connected_ {false};
    std::thread worker_;

    std::atomic<uint64_t> alerts_published_ {0};
    std::atomic<uint64_t> checks_performed_ {0};
    std::atomic<uint64_t> alert_id_counter_ {0};

    AlertCallback alert_callback_;

    std::string broker_url_;
    std::string client_id_;
    std::string topic_prefix_;
    int qos_ = 1;
};

}
