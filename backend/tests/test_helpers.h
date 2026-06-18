#pragma once

#include "common/data_types.h"
#include "config/config_loader.h"
#include "vehicle_comparator/vehicle_comparator.h"
#include "formation_optimizer/formation_optimizer.h"
#include "user_session/user_session_manager.h"
#include "impact_simulator/impact_simulator.h"

#include <memory>
#include <string>
#include <cmath>

namespace fenyun {
namespace test {

inline std::shared_ptr<ConfigLoader> create_test_config_loader() {
    auto config = std::make_shared<ConfigLoader>();
    std::string config_dir =
#ifdef FENYUN_TEST_CONFIG_DIR
        FENYUN_TEST_CONFIG_DIR;
#else
        "./config";
#endif

    std::string system_path = config_dir + "/system.json";
    std::string materials_path = config_dir + "/materials.json";
    std::string ahp_path = config_dir + "/ahp_weights.json";
    std::string vehicles_path = config_dir + "/vehicles.json";

    if (config->load_from_file(system_path, materials_path, ahp_path, vehicles_path)) {
        return config;
    }

    return config;
}

inline SensorData create_mock_sensor_data(uint32_t vehicle_id,
                                          std::string material,
                                          double rock_mass,
                                          double rock_velocity) {
    SensorData data{};
    data.vehicle_id = vehicle_id;
    data.timestamp_ms = current_timestamp_ms();
    data.roof_stress = 0.0;
    data.wheel_deformation = 0.0;
    data.rock_impact_force = 0.0;
    data.protection_thickness = 80.0;
    data.protection_material = std::move(material);
    data.ambient_temp = 293.15;
    data.impact_location_x = 0.0;
    data.impact_location_y = 0.0;
    data.rock_mass = rock_mass;
    data.rock_velocity = rock_velocity;
    return data;
}

inline bool is_close(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) <= tol;
}

inline std::shared_ptr<VehicleComparator> create_test_vehicle_comparator() {
    auto config = create_test_config_loader();
    auto simulator = std::make_shared<ImpactSimulator>(config);
    return std::make_shared<VehicleComparator>(config, simulator);
}

inline std::shared_ptr<FormationOptimizer> create_test_formation_optimizer() {
    auto config = create_test_config_loader();
    return std::make_shared<FormationOptimizer>(config);
}

inline std::shared_ptr<UserSessionManager> create_test_user_session_manager() {
    auto config = create_test_config_loader();
    auto simulator = std::make_shared<ImpactSimulator>(config);
    return std::make_shared<UserSessionManager>(config, simulator);
}

}
}
