#pragma once

#include "common/data_types.h"
#include "common/lockfree_queue.h"
#include "common/json.h"
#include "config/config_loader.h"

#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <functional>

namespace fenyun {

enum class DtuValidationResult {
    OK = 0,
    VEHICLE_ID_INVALID = 1,
    TIMESTAMP_INVALID = 2,
    STRESS_OUT_OF_RANGE = 3,
    DEFORMATION_NEGATIVE = 4,
    IMPACT_FORCE_NEGATIVE = 5,
    THICKNESS_INVALID = 6,
    MATERIAL_UNKNOWN = 7,
    ROCK_PARAMS_INVALID = 8,
    TEMPERATURE_OUT_OF_RANGE = 9,
};

class DtuReceiver {
public:
    using SensorQueue = LockFreeQueue<SensorData>;
    using ValidationCallback =
        std::function<void(DtuValidationResult, const SensorData&)>;

    explicit DtuReceiver(std::shared_ptr<ConfigLoader> config);
    ~DtuReceiver();

    void set_output_queue(std::shared_ptr<SensorQueue> queue);

    bool ingest_sensor_data(const SensorData& data);
    bool ingest_sensor_json(const std::string& json_body,
                            std::string& error_msg);

    DtuValidationResult validate(const SensorData& data) const;

    int ingest_batch(const std::vector<SensorData>& batch);
    int ingest_json_batch(const std::string& json_body, std::string& error_msg);

    uint64_t total_received() const { return total_received_.load(); }
    uint64_t total_valid() const { return total_valid_.load(); }
    uint64_t total_invalid() const { return total_invalid_.load(); }
    uint64_t queue_dropped() const { return queue_dropped_.load(); }

    void set_validation_callback(ValidationCallback cb) { on_validation_ = std::move(cb); }

    const std::vector<std::string>& supported_materials() const { return supported_materials_; }

private:
    std::shared_ptr<ConfigLoader> config_;
    std::shared_ptr<SensorQueue> output_queue_;

    std::atomic<uint64_t> total_received_ {0};
    std::atomic<uint64_t> total_valid_ {0};
    std::atomic<uint64_t> total_invalid_ {0};
    std::atomic<uint64_t> queue_dropped_ {0};

    std::vector<std::string> supported_materials_;
    ValidationCallback on_validation_;

    bool parse_sensor_json(const json::Value& root, SensorData& out) const;
};

}
