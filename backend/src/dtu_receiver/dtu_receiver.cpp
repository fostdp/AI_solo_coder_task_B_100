#include "dtu_receiver/dtu_receiver.h"

#include <iostream>
#include <algorithm>

namespace fenyun {

DtuReceiver::DtuReceiver(std::shared_ptr<ConfigLoader> config)
    : config_(std::move(config)) {
    if (config_) {
        const auto& mats = config_->materials();
        for (const auto& [k, v] : mats) {
            supported_materials_.push_back(k);
        }
    }
    if (supported_materials_.empty()) {
        supported_materials_ = {"wood", "cowhide", "iron", "composite"};
    }
}

DtuReceiver::~DtuReceiver() = default;

void DtuReceiver::set_output_queue(std::shared_ptr<SensorQueue> queue) {
    output_queue_ = std::move(queue);
}

bool DtuReceiver::ingest_sensor_data(const SensorData& data) {
    total_received_.fetch_add(1);

    DtuValidationResult vr = validate(data);
    if (vr != DtuValidationResult::OK) {
        total_invalid_.fetch_add(1);
        if (on_validation_) on_validation_(vr, data);
        return false;
    }

    if (output_queue_ && !output_queue_->push(data)) {
        queue_dropped_.fetch_add(1);
        return false;
    }

    total_valid_.fetch_add(1);
    return true;
}

bool DtuReceiver::ingest_sensor_json(const std::string& json_body,
                                      std::string& error_msg) {
    json::Value root = json::parse(json_body);
    if (root.isNull()) {
        error_msg = "Invalid JSON";
        total_invalid_.fetch_add(1);
        return false;
    }
    SensorData data{};
    if (!parse_sensor_json(root, data)) {
        error_msg = "Missing required fields";
        total_invalid_.fetch_add(1);
        return false;
    }
    if (data.timestamp_ms == 0) data.timestamp_ms = current_timestamp_ms();
    return ingest_sensor_data(data);
}

DtuValidationResult DtuReceiver::validate(const SensorData& data) const {
    if (data.vehicle_id == 0 || data.vehicle_id > 1000)
        return DtuValidationResult::VEHICLE_ID_INVALID;

    if (data.timestamp_ms < 0)
        return DtuValidationResult::TIMESTAMP_INVALID;

    if (data.roof_stress < 0 || data.roof_stress > 2000)
        return DtuValidationResult::STRESS_OUT_OF_RANGE;

    if (data.wheel_deformation < 0)
        return DtuValidationResult::DEFORMATION_NEGATIVE;

    if (data.rock_impact_force < 0)
        return DtuValidationResult::IMPACT_FORCE_NEGATIVE;

    if (data.protection_thickness <= 0 || data.protection_thickness > 500)
        return DtuValidationResult::THICKNESS_INVALID;

    if (data.protection_material.empty())
        return DtuValidationResult::MATERIAL_UNKNOWN;

    bool mat_found = false;
    for (const auto& m : supported_materials_) {
        if (m == data.protection_material) { mat_found = true; break; }
    }
    if (!mat_found && config_ && !config_->has_material(data.protection_material))
        return DtuValidationResult::MATERIAL_UNKNOWN;

    if (data.rock_mass < 0 || data.rock_velocity < 0)
        return DtuValidationResult::ROCK_PARAMS_INVALID;

    if (data.ambient_temp < -60 || data.ambient_temp > 200)
        return DtuValidationResult::TEMPERATURE_OUT_OF_RANGE;

    return DtuValidationResult::OK;
}

int DtuReceiver::ingest_batch(const std::vector<SensorData>& batch) {
    int ok = 0;
    for (const auto& d : batch) {
        if (ingest_sensor_data(d)) ok++;
    }
    return ok;
}

int DtuReceiver::ingest_json_batch(const std::string& json_body, std::string& error_msg) {
    json::Value root = json::parse(json_body);
    if (root.isNull() || !root.isArray()) {
        error_msg = "Invalid JSON array";
        return 0;
    }
    int ok = 0;
    for (size_t i = 0; i < root.size(); ++i) {
        SensorData d{};
        if (parse_sensor_json(root[i], d)) {
            if (d.timestamp_ms == 0) d.timestamp_ms = current_timestamp_ms();
            if (ingest_sensor_data(d)) ok++;
        }
    }
    return ok;
}

bool DtuReceiver::parse_sensor_json(const json::Value& root, SensorData& out) const {
    if (!root.isObject()) return false;

    out.vehicle_id = static_cast<uint32_t>(
        root.has("vehicle_id") ? root["vehicle_id"].asUInt() : 1);
    out.timestamp_ms = root.has("timestamp_ms") ? root["timestamp_ms"].asInt64() : 0;
    out.roof_stress = root.has("roof_stress") ? root["roof_stress"].asDouble() : 0.0;
    out.wheel_deformation = root.has("wheel_deformation") ? root["wheel_deformation"].asDouble() : 0.0;
    out.rock_impact_force = root.has("rock_impact_force") ? root["rock_impact_force"].asDouble() : 0.0;
    out.protection_thickness = root.has("protection_thickness")
        ? root["protection_thickness"].asDouble()
        : (config_ ? config_->system_config().dtu.default_protection_thickness_mm : 80.0);
    out.protection_material = root.has("protection_material")
        ? root["protection_material"].asString()
        : (config_ ? config_->system_config().dtu.default_material : "wood");
    out.ambient_temp = root.has("ambient_temp") ? root["ambient_temp"].asDouble() : 20.0;
    out.impact_location_x = root.has("impact_location_x") ? root["impact_location_x"].asDouble() : 3.0;
    out.impact_location_y = root.has("impact_location_y") ? root["impact_location_y"].asDouble() : 1.25;
    out.rock_mass = root.has("rock_mass") ? root["rock_mass"].asDouble() : 50.0;
    out.rock_velocity = root.has("rock_velocity") ? root["rock_velocity"].asDouble() : 15.0;

    return true;
}

}
