#pragma once

#include "common/data_types.h"
#include "common/json.h"

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace fenyun {

struct SystemConfig {
    struct HttpConfig {
        std::string host = "0.0.0.0";
        uint16_t port = 8080;
        size_t max_request_size_bytes = 65536;
    };
    struct ClickHouseConfig {
        std::string host = "127.0.0.1";
        uint16_t http_port = 8123;
        uint16_t tcp_port = 9000;
        std::string user = "default";
        std::string password = "";
        std::string database = "fenyun_vehicle";
        int connection_timeout_ms = 3000;
    };
    struct MqttConfig {
        std::string broker_url = "tcp://127.0.0.1:1883";
        std::string client_id = "fenyun_backend";
        std::string username = "";
        std::string password = "";
        int qos = 1;
        std::string topic_prefix = "fenyun/vehicle";
    };
    struct SimulationConfig {
        int threads = 2;
        size_t queue_capacity = 1024;
        double default_roof_length_m = 6.5;
        double default_roof_width_m = 2.8;
        int default_grid_size = 10;
        int jc_bisection_iterations = 60;
    };
    struct AlarmConfig {
        int threads = 1;
        size_t queue_capacity = 512;
        double deformation_threshold_mm = 15.0;
        double penetration_threshold_ratio = 0.9;
        double stress_threshold_mpa = 200.0;
        double deformation_warn_ratio = 0.7;
    };
    struct OptimizerConfig {
        size_t queue_capacity = 128;
        int threads = 1;
        int auto_reevaluate_seconds = 300;
    };
    struct DtuConfig {
        int report_interval_seconds = 60;
        int vehicle_count = 3;
        std::string default_material = "wood";
        double default_protection_thickness_mm = 80.0;
    };

    HttpConfig http;
    ClickHouseConfig clickhouse;
    MqttConfig mqtt;
    SimulationConfig simulation;
    AlarmConfig alarm;
    OptimizerConfig optimizer;
    DtuConfig dtu;

    std::string version;
    std::string description;
};

struct AHPConfig {
    struct ConsistencyConfig {
        double cr_threshold = 0.10;
        bool auto_correction = true;
        double auto_correction_cr_target = 0.08;
        int max_correction_iterations = 80;
        double correction_step_ratio = 0.15;
    };
    struct ExpertPoolEntry {
        std::string name;
        std::string title;
        double authority_weight = 0.8;
    };
    struct GroupDecisionConfig {
        bool enabled = true;
        int default_expert_count = 5;
        int max_expert_count = 10;
        double expert_divergence = 0.22;
        double consensus_threshold = 0.75;
        std::string aggregation_method = "wggm";
        std::vector<ExpertPoolEntry> experts_pool;
    };
    struct EvaluationScheme {
        std::string material_type;
        double thickness_mm;
    };

    std::vector<std::string> criteria;
    std::map<std::string, std::string> criteria_display;
    std::vector<std::vector<double>> reference_pairwise_matrix;
    ConsistencyConfig consistency;
    GroupDecisionConfig group_decision;
    std::vector<EvaluationScheme> evaluation_schemes;

    std::string version;
};

class ConfigLoader {
public:
    ConfigLoader();
    ~ConfigLoader();

    bool load_from_file(const std::string& system_path,
                        const std::string& materials_path,
                        const std::string& ahp_path);

    bool load_from_string(const std::string& system_json,
                          const std::string& materials_json,
                          const std::string& ahp_json);

    const SystemConfig& system_config() const { return system_; }
    const AHPConfig& ahp_config() const { return ahp_; }
    const std::map<std::string, MaterialProperties>& materials() const { return materials_; }
    const std::map<std::string, JohnsonCookParams>& jc_params() const { return jc_params_; }

    MaterialProperties get_material(const std::string& name) const;
    JohnsonCookParams get_jc_params(const std::string& material_name) const;

    bool has_material(const std::string& name) const;

private:
    bool parse_system_config(const json::Value& root);
    bool parse_materials_config(const json::Value& root);
    bool parse_ahp_config(const json::Value& root);

    SystemConfig system_;
    AHPConfig ahp_;
    std::map<std::string, MaterialProperties> materials_;
    std::map<std::string, JohnsonCookParams> jc_params_;
    bool loaded_ = false;
};

std::shared_ptr<ConfigLoader> create_default_config_loader();

}
