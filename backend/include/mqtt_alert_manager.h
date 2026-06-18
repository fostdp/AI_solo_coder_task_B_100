#pragma once

#include "common.h"
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>

namespace fenyun {

using AlertCallback = std::function<void(const AlertRecord&)>;

class MQTTAlertManager {
public:
    MQTTAlertManager(const std::string& broker_url,
                     const std::string& client_id,
                     const std::string& username = "",
                     const std::string& password = "");
    ~MQTTAlertManager();

    bool connect();
    void disconnect();
    bool is_connected() const;

    void set_deformation_threshold(double mm);
    void set_penetration_threshold(double ratio);

    double get_deformation_threshold() const;
    double get_penetration_threshold() const;

    bool publish_alert(const AlertRecord& record);

    std::vector<AlertRecord> check_and_alert(const SensorData& sensor,
                                              const SimulationResult& sim,
                                              uint64_t alert_id_counter);

    void set_alert_callback(AlertCallback callback);

    void subscribe_alerts(const std::string& topic_filter);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mutex_;
    AlertCallback alert_callback_;

    std::string broker_url_;
    std::string client_id_;
    std::string username_;
    std::string password_;
    std::atomic<bool> connected_ {false};

    double deformation_threshold_mm_ = 15.0;
    double penetration_threshold_ratio_ = 0.9;
    double stress_threshold_mpa_ = 200.0;

    std::string generate_mqtt_message_id();
};

}
