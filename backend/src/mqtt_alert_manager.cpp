#include "mqtt_alert_manager.h"
#include <sstream>
#include <random>
#include <iomanip>
#include <ctime>
#include <iostream>
#include <algorithm>

namespace fenyun {

struct MQTTAlertManager::Impl {
    std::atomic<uint64_t> msg_counter {0};
};

MQTTAlertManager::MQTTAlertManager(const std::string& broker_url,
                                     const std::string& client_id,
                                     const std::string& username,
                                     const std::string& password)
    : impl_(std::make_unique<Impl>()),
      broker_url_(broker_url),
      client_id_(client_id),
      username_(username),
      password_(password) {}

MQTTAlertManager::~MQTTAlertManager() = default;

bool MQTTAlertManager::connect() {
    std::cout << "[MQTT] Connecting to broker: " << broker_url_
              << " as client: " << client_id_ << std::endl;
    connected_ = true;
    return true;
}

void MQTTAlertManager::disconnect() {
    connected_ = false;
    std::cout << "[MQTT] Disconnected from broker" << std::endl;
}

bool MQTTAlertManager::is_connected() const {
    return connected_;
}

void MQTTAlertManager::set_deformation_threshold(double mm) {
    std::lock_guard<std::mutex> lock(mutex_);
    deformation_threshold_mm_ = mm;
}

void MQTTAlertManager::set_penetration_threshold(double ratio) {
    std::lock_guard<std::mutex> lock(mutex_);
    penetration_threshold_ratio_ = ratio;
}

double MQTTAlertManager::get_deformation_threshold() const {
    return deformation_threshold_mm_;
}

double MQTTAlertManager::get_penetration_threshold() const {
    return penetration_threshold_ratio_;
}

std::string MQTTAlertManager::generate_mqtt_message_id() {
    uint64_t cnt = impl_->msg_counter.fetch_add(1);
    std::ostringstream oss;
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    oss << "MSG-" << std::put_time(&tm_buf, "%Y%m%d%H%M%S")
        << "-" << std::setw(6) << std::setfill('0') << cnt;
    return oss.str();
}

bool MQTTAlertManager::publish_alert(const AlertRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) {
        std::cerr << "[MQTT] Not connected, cannot publish alert" << std::endl;
        return false;
    }

    std::ostringstream payload;
    payload << "{"
            << "\"alert_id\":" << record.alert_id << ","
            << "\"vehicle_id\":" << record.vehicle_id << ","
            << "\"timestamp\":" << record.timestamp_ms << ","
            << "\"alert_type\":\"" << record.alert_type << "\","
            << "\"alert_level\":" << static_cast<int>(record.alert_level) << ","
            << "\"message\":\"" << record.alert_message << "\","
            << "\"measured_value\":" << record.measured_value << ","
            << "\"threshold_value\":" << record.threshold_value << ","
            << "\"mqtt_message_id\":\"" << record.mqtt_message_id << "\""
            << "}";

    std::cout << "[MQTT] Publish [" << record.mqtt_topic << "]: " << payload.str() << std::endl;
    return true;
}

std::vector<AlertRecord> MQTTAlertManager::check_and_alert(const SensorData& sensor,
                                                            const SimulationResult& sim,
                                                            uint64_t alert_id_counter) {
    std::vector<AlertRecord> alerts;
    std::lock_guard<std::mutex> lock(mutex_);

    if (sim.roof_max_deformation_mm > deformation_threshold_mm_) {
        AlertRecord alert;
        alert.alert_id = alert_id_counter++;
        alert.vehicle_id = sensor.vehicle_id;
        alert.timestamp_ms = sensor.timestamp_ms;
        alert.alert_type = "deformation_exceed";
        alert.alert_level = sim.roof_max_deformation_mm > deformation_threshold_mm_ * 1.5 ? 3 : 2;
        std::ostringstream msg;
        msg << "车辆" << sensor.vehicle_id << "顶棚变形超限: "
            << std::fixed << std::setprecision(2) << sim.roof_max_deformation_mm
            << "mm > 阈值 " << deformation_threshold_mm_ << "mm";
        alert.alert_message = msg.str();
        alert.measured_value = sim.roof_max_deformation_mm;
        alert.threshold_value = deformation_threshold_mm_;
        alert.mqtt_topic = "fenyun/vehicle/" + std::to_string(sensor.vehicle_id) + "/alert/deformation";
        alert.mqtt_message_id = generate_mqtt_message_id();
        alerts.push_back(alert);
    }

    if (sim.is_penetrated || sim.penetration_depth_mm > sensor.protection_thickness * penetration_threshold_ratio_) {
        AlertRecord alert;
        alert.alert_id = alert_id_counter++;
        alert.vehicle_id = sensor.vehicle_id;
        alert.timestamp_ms = sensor.timestamp_ms;
        alert.alert_type = "penetration";
        alert.alert_level = 4;
        std::ostringstream msg;
        msg << "车辆" << sensor.vehicle_id << "防护层击穿预警: 侵彻深度 "
            << std::fixed << std::setprecision(2) << sim.penetration_depth_mm
            << "mm, 防护厚度 " << sensor.protection_thickness << "mm";
        alert.alert_message = msg.str();
        alert.measured_value = sim.penetration_depth_mm;
        alert.threshold_value = sensor.protection_thickness * penetration_threshold_ratio_;
        alert.mqtt_topic = "fenyun/vehicle/" + std::to_string(sensor.vehicle_id) + "/alert/penetration";
        alert.mqtt_message_id = generate_mqtt_message_id();
        alerts.push_back(alert);
    }

    if (sim.roof_von_mises_stress_mpa > stress_threshold_mpa_) {
        AlertRecord alert;
        alert.alert_id = alert_id_counter++;
        alert.vehicle_id = sensor.vehicle_id;
        alert.timestamp_ms = sensor.timestamp_ms;
        alert.alert_type = "stress_exceed";
        alert.alert_level = 3;
        std::ostringstream msg;
        msg << "车辆" << sensor.vehicle_id << "顶棚应力超限: "
            << std::fixed << std::setprecision(2) << sim.roof_von_mises_stress_mpa
            << "MPa > 阈值 " << stress_threshold_mpa_ << "MPa";
        alert.alert_message = msg.str();
        alert.measured_value = sim.roof_von_mises_stress_mpa;
        alert.threshold_value = stress_threshold_mpa_;
        alert.mqtt_topic = "fenyun/vehicle/" + std::to_string(sensor.vehicle_id) + "/alert/stress";
        alert.mqtt_message_id = generate_mqtt_message_id();
        alerts.push_back(alert);
    }

    for (auto& alert : alerts) {
        publish_alert(alert);
        if (alert_callback_) {
            alert_callback_(alert);
        }
    }

    return alerts;
}

void MQTTAlertManager::set_alert_callback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    alert_callback_ = std::move(callback);
}

void MQTTAlertManager::subscribe_alerts(const std::string& topic_filter) {
    std::cout << "[MQTT] Subscribed to topic filter: " << topic_filter << std::endl;
}

}
