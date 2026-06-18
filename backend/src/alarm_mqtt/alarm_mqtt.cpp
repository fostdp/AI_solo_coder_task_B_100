#include "alarm_mqtt/alarm_mqtt.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace fenyun {

AlarmMqtt::AlarmMqtt(std::shared_ptr<ConfigLoader> config)
    : config_(std::move(config)) {
    if (config_) {
        const auto& ac = config_->system_config().alarm;
        thresholds_.deformation_critical_mm = ac.deformation_threshold_mm;
        thresholds_.deformation_warn_mm = ac.deformation_threshold_mm * ac.deformation_warn_ratio;
        thresholds_.penetration_ratio_critical = ac.penetration_threshold_ratio;
        thresholds_.penetration_ratio_warn = ac.penetration_threshold_ratio * 0.75;
        thresholds_.stress_critical_mpa = ac.stress_threshold_mpa;
        thresholds_.stress_warn_mpa = ac.stress_threshold_mpa * 0.7;

        const auto& mc = config_->system_config().mqtt;
        broker_url_ = mc.broker_url;
        client_id_ = mc.client_id;
        topic_prefix_ = mc.topic_prefix;
        qos_ = mc.qos;
    }
}

AlarmMqtt::~AlarmMqtt() {
    stop();
    disconnect();
}

void AlarmMqtt::set_input_queue(std::shared_ptr<InputQueue> q) {
    input_queue_ = std::move(q);
}

bool AlarmMqtt::connect() {
    std::cout << "[AlarmMQTT] Connecting to " << broker_url_
              << " as " << client_id_ << " ..." << std::endl;

    // TODO: 接入 paho.mqtt.cpp 或其他 MQTT 库
    // 当前为桩实现

    connected_.store(true);
    std::cout << "[AlarmMQTT] Connected (stub implementation)" << std::endl;
    return true;
}

void AlarmMqtt::disconnect() {
    if (!connected_.exchange(false)) return;
    std::cout << "[AlarmMQTT] Disconnected" << std::endl;
}

void AlarmMqtt::start() {
    if (running_.exchange(true)) return;
    if (!connected_.load()) connect();
    worker_ = std::thread(&AlarmMqtt::worker_loop, this);
}

void AlarmMqtt::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

void AlarmMqtt::worker_loop() {
    SimulationResult sim{};
    while (running_.load()) {
        if (input_queue_ && input_queue_->pop(sim)) {
            SensorData sensor{};
            sensor.vehicle_id = sim.vehicle_id;
            sensor.timestamp_ms = sim.timestamp_ms;
            sensor.protection_thickness = sim.roof_max_deformation_mm * 0.1 + 80.0;
            sensor.protection_material = sim.protection_material;

            auto alerts = check_and_alert(sim, sensor);
            for (const auto& a : alerts) {
                publish_alert(a);
                if (alert_callback_) alert_callback_(a);
            }
            checks_performed_.fetch_add(1);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

std::string AlarmMqtt::alert_type_to_string(AlertType type) {
    switch (type) {
        case AlertType::DEFORMATION_EXCEEDED: return "deformation_exceed";
        case AlertType::PENETRATION:          return "penetration";
        case AlertType::STRESS_EXCEEDED:      return "stress_exceed";
    }
    return "unknown";
}

std::string AlarmMqtt::build_topic(uint32_t vehicle_id, AlertType type) const {
    std::string type_str = alert_type_to_string(type);
    std::ostringstream oss;
    oss << topic_prefix_ << "/" << vehicle_id << "/alert/" << type_str;
    return oss.str();
}

std::string AlarmMqtt::build_message_id(uint64_t alert_id) const {
    std::time_t t = std::time(nullptr);
    struct tm tm_info {};
#if defined(_WIN32)
    localtime_s(&tm_info, &t);
#else
    localtime_r(&t, &tm_info);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &tm_info);
    std::ostringstream oss;
    oss << "MSG-" << buf << "-" << std::setw(6) << std::setfill('0') << alert_id;
    return oss.str();
}

AlertRecord AlarmMqtt::make_alert(uint32_t vehicle_id,
                                   int64_t timestamp_ms,
                                   AlertType type,
                                   uint8_t level,
                                   const std::string& message,
                                   double measured,
                                   double threshold) const {
    AlertRecord a{};
    a.alert_id = const_cast<std::atomic<uint64_t>&>(alert_id_counter_).fetch_add(1) + 1;
    a.vehicle_id = vehicle_id;
    a.timestamp_ms = timestamp_ms;
    a.alert_type = alert_type_to_string(type);
    a.alert_level = level;
    a.alert_message = message;
    a.measured_value = measured;
    a.threshold_value = threshold;
    a.mqtt_topic = build_topic(vehicle_id, type);
    a.mqtt_message_id = build_message_id(a.alert_id);
    return a;
}

std::vector<AlertRecord> AlarmMqtt::check_and_alert(const SimulationResult& sim,
                                                     const SensorData& sensor) {
    std::vector<AlertRecord> alerts;

    if (sim.roof_max_deformation_mm > thresholds_.deformation_critical_mm) {
        std::ostringstream msg;
        msg << "车辆" << sim.vehicle_id << "顶棚变形超限(临界): "
            << std::fixed << std::setprecision(2) << sim.roof_max_deformation_mm
            << "mm > " << thresholds_.deformation_critical_mm << "mm";
        alerts.push_back(make_alert(sim.vehicle_id, sim.timestamp_ms,
                                     AlertType::DEFORMATION_EXCEEDED, 3,
                                     msg.str(),
                                     sim.roof_max_deformation_mm,
                                     thresholds_.deformation_critical_mm));
    } else if (sim.roof_max_deformation_mm > thresholds_.deformation_warn_mm) {
        std::ostringstream msg;
        msg << "车辆" << sim.vehicle_id << "顶棚变形预警: "
            << std::fixed << std::setprecision(2) << sim.roof_max_deformation_mm << "mm";
        alerts.push_back(make_alert(sim.vehicle_id, sim.timestamp_ms,
                                     AlertType::DEFORMATION_EXCEEDED, 2,
                                     msg.str(),
                                     sim.roof_max_deformation_mm,
                                     thresholds_.deformation_warn_mm));
    }

    double pen_ratio = sensor.protection_thickness > 0
        ? sim.penetration_depth_mm / sensor.protection_thickness : 0.0;

    if (sim.is_penetrated || pen_ratio > thresholds_.penetration_ratio_critical) {
        std::ostringstream msg;
        msg << "车辆" << sim.vehicle_id << "防护层击穿预警: 侵彻深度 "
            << std::fixed << std::setprecision(2) << sim.penetration_depth_mm
            << "mm, 防护厚度 " << sensor.protection_thickness << "mm";
        alerts.push_back(make_alert(sim.vehicle_id, sim.timestamp_ms,
                                     AlertType::PENETRATION, 4,
                                     msg.str(),
                                     sim.penetration_depth_mm,
                                     sensor.protection_thickness * thresholds_.penetration_ratio_critical));
    } else if (pen_ratio > thresholds_.penetration_ratio_warn) {
        std::ostringstream msg;
        msg << "车辆" << sim.vehicle_id << "侵彻预警: 深度 "
            << std::fixed << std::setprecision(2) << sim.penetration_depth_mm << "mm";
        alerts.push_back(make_alert(sim.vehicle_id, sim.timestamp_ms,
                                     AlertType::PENETRATION, 2,
                                     msg.str(),
                                     sim.penetration_depth_mm,
                                     sensor.protection_thickness * thresholds_.penetration_ratio_warn));
    }

    if (sim.roof_von_mises_stress_mpa > thresholds_.stress_critical_mpa) {
        std::ostringstream msg;
        msg << "车辆" << sim.vehicle_id << "应力超限(临界): "
            << std::fixed << std::setprecision(2) << sim.roof_von_mises_stress_mpa
            << "MPa > " << thresholds_.stress_critical_mpa << "MPa";
        alerts.push_back(make_alert(sim.vehicle_id, sim.timestamp_ms,
                                     AlertType::STRESS_EXCEEDED, 3,
                                     msg.str(),
                                     sim.roof_von_mises_stress_mpa,
                                     thresholds_.stress_critical_mpa));
    } else if (sim.roof_von_mises_stress_mpa > thresholds_.stress_warn_mpa) {
        std::ostringstream msg;
        msg << "车辆" << sim.vehicle_id << "应力预警: "
            << std::fixed << std::setprecision(2) << sim.roof_von_mises_stress_mpa << "MPa";
        alerts.push_back(make_alert(sim.vehicle_id, sim.timestamp_ms,
                                     AlertType::STRESS_EXCEEDED, 2,
                                     msg.str(),
                                     sim.roof_von_mises_stress_mpa,
                                     thresholds_.stress_warn_mpa));
    }

    return alerts;
}

bool AlarmMqtt::publish_alert(const AlertRecord& alert) {
    if (!connected_.load()) return false;

    std::ostringstream payload;
    payload << "{\"alert_id\":" << alert.alert_id
            << ",\"vehicle_id\":" << alert.vehicle_id
            << ",\"timestamp\":" << alert.timestamp_ms
            << ",\"alert_type\":\"" << alert.alert_type << "\""
            << ",\"alert_level\":" << static_cast<int>(alert.alert_level)
            << ",\"message\":\"" << alert.alert_message << "\""
            << ",\"measured_value\":" << alert.measured_value
            << ",\"threshold_value\":" << alert.threshold_value
            << ",\"mqtt_message_id\":\"" << alert.mqtt_message_id << "\""
            << "}";

    // TODO: 实际 MQTT publish 调用
    // mqtt_client_->publish(alert.mqtt_topic, payload.str(), qos_);

    std::cout << "[AlarmMQTT] [" << alert.mqtt_topic << "] "
              << "L" << static_cast<int>(alert.alert_level) << " "
              << alert.alert_message << std::endl;

    alerts_published_.fetch_add(1);
    return true;
}

}
