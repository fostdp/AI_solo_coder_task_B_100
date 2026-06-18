#include "config/config_loader.h"
#include "impact_simulator/impact_simulator.h"
#include "vehicle_comparator/vehicle_comparator.h"
#include "user_session/user_session_manager.h"
#include "formation_optimizer/formation_optimizer.h"
#include "common/data_types.h"

#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <cmath>

namespace fenyun {

constexpr const char* kSystemJson = R"({
  "version": "1.0.0",
  "http": { "host": "0.0.0.0", "port": 8080, "max_request_size_bytes": 65536 },
  "clickhouse": { "host": "127.0.0.1", "http_port": 8123, "tcp_port": 9000, "user": "default", "password": "", "database": "test", "connection_timeout_ms": 3000 },
  "mqtt": { "broker_url": "tcp://127.0.0.1:1883", "client_id": "test", "username": "", "password": "", "qos": 1, "topic_prefix": "test" },
  "simulation": { "threads": 2, "queue_capacity": 1024, "default_roof_length_m": 6.5, "default_roof_width_m": 2.8, "default_grid_size": 10, "jc_plastic_strain_bisection_iterations": 60 },
  "alarm": { "threads": 1, "queue_capacity": 512, "deformation_threshold_mm": 15.0, "penetration_threshold_ratio": 0.9, "stress_threshold_mpa": 200.0, "deformation_warn_ratio": 0.7 },
  "protection_optimizer": { "queue_capacity": 128, "threads": 1, "auto_reevaluate_seconds": 300 },
  "dtu_receiver": { "report_interval_seconds": 60, "vehicle_count": 3, "default_material": "wood", "default_protection_thickness_mm": 80.0 }
})";

constexpr const char* kAhpJson = R"({
  "version": "1.0.0",
  "criteria": ["energy_absorption", "structural_strength", "weight_factor", "cost_factor", "durability"],
  "criteria_display": { "energy_absorption": "吸能", "structural_strength": "强度" },
  "reference_pairwise_matrix": [
    [1.0, 1.5, 3.0, 4.0, 2.0],
    [0.667, 1.0, 2.0, 3.0, 1.5],
    [0.333, 0.5, 1.0, 2.0, 0.75],
    [0.25, 0.333, 0.5, 1.0, 0.5],
    [0.5, 0.667, 1.333, 2.0, 1.0]
  ],
  "consistency": { "cr_threshold": 0.10, "auto_correction": true, "auto_correction_cr_target": 0.08, "max_correction_iterations": 80, "correction_step_ratio": 0.15 },
  "group_decision": { "enabled": true, "default_expert_count": 5, "max_expert_count": 10, "expert_divergence": 0.22, "consensus_threshold": 0.75, "aggregation_method": "wggm", "experts_pool": [] },
  "evaluation_schemes": [{ "material_type": "wood", "thickness_mm": 50.0 }]
})";

constexpr const char* kMaterialsJson = R"({
  "version": "1.0.0",
  "materials": {
    "wood": {
      "display_name": "木材",
      "density": 650.0,
      "youngs_modulus_gpa": 10.0,
      "poisson_ratio": 0.35,
      "yield_strength_mpa": 60.0,
      "ultimate_strength_mpa": 120.0,
      "toughness_mj_m3": 10.0,
      "specific_energy_absorption_kj_kg": 50.0,
      "cost_per_unit": 1.0,
      "durability_base": 0.6,
      "johnson_cook": {
        "A_pa": 60000000.0,
        "B_pa": 120000000.0,
        "n": 0.45,
        "C": 0.06,
        "m": 1.0,
        "T_melt_K": 600.0,
        "T_ref_K": 298.15,
        "eps_dot_0": 1.0
      }
    },
    "iron": {
      "display_name": "铸铁",
      "density": 7850.0,
      "youngs_modulus_gpa": 206.0,
      "poisson_ratio": 0.29,
      "yield_strength_mpa": 235.0,
      "ultimate_strength_mpa": 400.0,
      "toughness_mj_m3": 80.0,
      "specific_energy_absorption_kj_kg": 200.0,
      "cost_per_unit": 10.0,
      "durability_base": 0.9,
      "johnson_cook": {
        "A_pa": 235000000.0,
        "B_pa": 380000000.0,
        "n": 0.32,
        "C": 0.022,
        "m": 0.55,
        "T_melt_K": 1811.0,
        "T_ref_K": 298.15,
        "eps_dot_0": 1.0
      }
    },
    "modern_aluminum": {
      "display_name": "5083铝合金装甲",
      "density": 2660.0,
      "youngs_modulus_gpa": 70.3,
      "poisson_ratio": 0.33,
      "yield_strength_mpa": 215.0,
      "ultimate_strength_mpa": 305.0,
      "toughness_mj_m3": 120.0,
      "specific_energy_absorption_kj_kg": 450.0,
      "cost_per_unit": 25.0,
      "durability_base": 0.92,
      "johnson_cook": {
        "A_pa": 215000000.0,
        "B_pa": 280000000.0,
        "n": 0.30,
        "C": 0.015,
        "m": 0.40,
        "T_melt_K": 925.0,
        "T_ref_K": 298.15,
        "eps_dot_0": 1.0
      }
    },
    "modern_steel": {
      "display_name": "RHA均质钢装甲",
      "density": 7850.0,
      "youngs_modulus_gpa": 210.0,
      "poisson_ratio": 0.29,
      "yield_strength_mpa": 1100.0,
      "ultimate_strength_mpa": 1350.0,
      "toughness_mj_m3": 350.0,
      "specific_energy_absorption_kj_kg": 445.0,
      "cost_per_unit": 40.0,
      "durability_base": 0.95,
      "johnson_cook": {
        "A_pa": 1100000000.0,
        "B_pa": 650000000.0,
        "n": 0.26,
        "C": 0.036,
        "m": 1.03,
        "T_melt_K": 1800.0,
        "T_ref_K": 298.15,
        "eps_dot_0": 1.0
      }
    },
    "modern_composite": {
      "display_name": "陶瓷-金属复合装甲",
      "density": 3800.0,
      "youngs_modulus_gpa": 350.0,
      "poisson_ratio": 0.25,
      "yield_strength_mpa": 2500.0,
      "ultimate_strength_mpa": 3200.0,
      "toughness_mj_m3": 800.0,
      "specific_energy_absorption_kj_kg": 2100.0,
      "cost_per_unit": 120.0,
      "durability_base": 0.88,
      "johnson_cook": {
        "A_pa": 2500000000.0,
        "B_pa": 900000000.0,
        "n": 0.18,
        "C": 0.025,
        "m": 0.70,
        "T_melt_K": 2800.0,
        "T_ref_K": 298.15,
        "eps_dot_0": 1.0
      }
    },
    "modern_du": {
      "display_name": "贫铀复合装甲",
      "density": 18600.0,
      "youngs_modulus_gpa": 200.0,
      "poisson_ratio": 0.28,
      "yield_strength_mpa": 900.0,
      "ultimate_strength_mpa": 1500.0,
      "toughness_mj_m3": 1200.0,
      "specific_energy_absorption_kj_kg": 645.0,
      "cost_per_unit": 500.0,
      "durability_base": 0.98,
      "johnson_cook": {
        "A_pa": 900000000.0,
        "B_pa": 1200000000.0,
        "n": 0.22,
        "C": 0.04,
        "m": 0.85,
        "T_melt_K": 1405.0,
        "T_ref_K": 298.15,
        "eps_dot_0": 1.0
      }
    },
    "modern_era": {
      "display_name": "爆炸反应装甲(ERA)",
      "density": 2500.0,
      "youngs_modulus_gpa": 50.0,
      "poisson_ratio": 0.35,
      "yield_strength_mpa": 80.0,
      "ultimate_strength_mpa": 200.0,
      "toughness_mj_m3": 5000.0,
      "specific_energy_absorption_kj_kg": 2000.0,
      "cost_per_unit": 150.0,
      "durability_base": 0.60,
      "johnson_cook": {
        "A_pa": 80000000.0,
        "B_pa": 150000000.0,
        "n": 0.35,
        "C": 0.05,
        "m": 0.60,
        "T_melt_K": 1500.0,
        "T_ref_K": 298.15,
        "eps_dot_0": 1.0
      }
    }
  }
})";

constexpr const char* kVehiclesJson = R"({
  "version": "1.0.0",
  "vehicles": {
    "fenyun_basic": {
      "display_name": "轒辒车(基础型)",
      "description": "春秋时期经典攻城车",
      "era": "ancient",
      "type": "FENYUN",
      "length_m": 6.5,
      "width_m": 2.8,
      "height_m": 2.5,
      "weight_ton": 3.2,
      "crew_count": 6,
      "max_speed_kmh": 4.0,
      "roof_thickness_mm": 80.0,
      "wall_thickness_mm": 60.0,
      "primary_material": "wood",
      "available_materials": ["wood", "iron"],
      "protection_area_m2": 18.2,
      "historical_year": -550,
      "origin": "春秋·齐国"
    },
    "chongche_ram": {
      "display_name": "冲车(撞城车)",
      "description": "重型攻城槌车",
      "era": "ancient",
      "type": "CHONGCHE",
      "length_m": 12.0,
      "width_m": 3.5,
      "height_m": 4.0,
      "weight_ton": 8.5,
      "crew_count": 20,
      "max_speed_kmh": 1.8,
      "roof_thickness_mm": 50.0,
      "wall_thickness_mm": 150.0,
      "primary_material": "wood",
      "available_materials": ["wood", "iron"],
      "protection_area_m2": 42.0,
      "historical_year": -480,
      "origin": "春秋·鲁国"
    },
    "yunti_basic": {
      "display_name": "云梯(基础型)",
      "description": "经典折叠云梯车",
      "era": "ancient",
      "type": "YUNTI",
      "length_m": 8.0,
      "width_m": 2.0,
      "height_m": 3.0,
      "weight_ton": 2.0,
      "crew_count": 4,
      "max_speed_kmh": 5.0,
      "roof_thickness_mm": 10.0,
      "wall_thickness_mm": 20.0,
      "primary_material": "wood",
      "available_materials": ["wood"],
      "protection_area_m2": 6.0,
      "historical_year": -600,
      "origin": "春秋·楚国"
    },
    "modern_m113": {
      "display_name": "M113 装甲运兵车",
      "description": "经典现代APC",
      "era": "modern",
      "type": "MODERN_APC",
      "length_m": 5.3,
      "width_m": 2.7,
      "height_m": 2.5,
      "weight_ton": 12.3,
      "crew_count": 13,
      "max_speed_kmh": 64.0,
      "roof_thickness_mm": 38.0,
      "wall_thickness_mm": 32.0,
      "primary_material": "modern_aluminum",
      "available_materials": ["modern_aluminum", "modern_steel", "modern_composite"],
      "protection_area_m2": 14.3,
      "historical_year": 1960,
      "origin": "美国"
    },
    "modern_m1a2": {
      "display_name": "M1A2 艾布拉姆斯坦克",
      "description": "第三代主战坦克",
      "era": "modern",
      "type": "MODERN_TANK",
      "length_m": 9.78,
      "width_m": 3.66,
      "height_m": 2.44,
      "weight_ton": 66.8,
      "crew_count": 4,
      "max_speed_kmh": 67.0,
      "roof_thickness_mm": 250.0,
      "wall_thickness_mm": 800.0,
      "primary_material": "modern_du",
      "available_materials": ["modern_steel", "modern_composite", "modern_du", "modern_era"],
      "protection_area_m2": 35.8,
      "historical_year": 1992,
      "origin": "美国"
    },
    "modern_m2_bradley": {
      "display_name": "M2 布雷德利步兵战车",
      "description": "重型IFV",
      "era": "modern",
      "type": "MODERN_IFV",
      "length_m": 6.55,
      "width_m": 3.29,
      "height_m": 2.98,
      "weight_ton": 27.6,
      "crew_count": 9,
      "max_speed_kmh": 56.0,
      "roof_thickness_mm": 80.0,
      "wall_thickness_mm": 150.0,
      "primary_material": "modern_composite",
      "available_materials": ["modern_steel", "modern_composite", "modern_era"],
      "protection_area_m2": 21.6,
      "historical_year": 1981,
      "origin": "美国"
    }
  }
})";

class IntegrationTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = std::make_shared<ConfigLoader>();
        ASSERT_TRUE(config_->load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
        simulator_ = std::make_shared<ImpactSimulator>(config_);
        comparator_ = std::make_shared<VehicleComparator>(config_, simulator_);
        session_manager_ = std::make_shared<UserSessionManager>(config_, simulator_);
        formation_optimizer_ = std::make_shared<FormationOptimizer>(config_);
    }

    std::shared_ptr<ConfigLoader> config_;
    std::shared_ptr<ImpactSimulator> simulator_;
    std::shared_ptr<VehicleComparator> comparator_;
    std::shared_ptr<UserSessionManager> session_manager_;
    std::shared_ptr<FormationOptimizer> formation_optimizer_;
};

TEST_F(IntegrationTestFixture, ComparatorSimulator_FullComparisonPipeline_EndToEnd) {
    // 测试：完整的对比器+仿真器端到端流程
    auto result = comparator_->compare_cross_era(50.0, 15.0);
    
    EXPECT_GT(result.items.size(), 0u);
    EXPECT_FALSE(result.best_vehicle_id.empty());
    
    for (const auto& item : result.items) {
        const auto& sim = item.simulation;
        EXPECT_GT(sim.roof_max_deformation_mm, 0.0) 
            << item.vehicle_id << " 变形量应大于0";
        EXPECT_GT(sim.roof_von_mises_stress_mpa, 0.0) 
            << item.vehicle_id << " 应力应大于0";
        EXPECT_GT(sim.impact_energy_j, 0.0) 
            << item.vehicle_id << " 冲击能量应大于0";
        EXPECT_GE(sim.absorbed_energy_j, 0.0) 
            << item.vehicle_id << " 吸收能量应非负";
        EXPECT_LE(sim.absorbed_energy_j, sim.impact_energy_j) 
            << item.vehicle_id << " 能量守恒：吸收能量不应超过冲击能量";
        EXPECT_GE(sim.roof_plastic_strain, 0.0) 
            << item.vehicle_id << " 塑性应变应 >= 0";
    }
}

TEST_F(IntegrationTestFixture, ComparatorSimulator_SimulatorConsistency_WithDifferentMaterials) {
    // 测试：同一种冲击下，不同材料产生不同的仿真结果
    SensorData data_wood{};
    data_wood.vehicle_id = 1;
    data_wood.timestamp_ms = 1000;
    data_wood.protection_thickness = 50.0;
    data_wood.protection_material = "wood";
    data_wood.rock_mass = 30.0;
    data_wood.rock_velocity = 10.0;
    data_wood.impact_location_x = 3.0;
    data_wood.impact_location_y = 1.4;
    data_wood.ambient_temp = 20.0;
    
    SensorData data_steel = data_wood;
    data_steel.protection_material = "modern_steel";
    
    auto result_wood = simulator_->run_simulation(data_wood);
    auto result_steel = simulator_->run_simulation(data_steel);
    
    EXPECT_NE(result_wood.roof_max_deformation_mm, result_steel.roof_max_deformation_mm)
        << "不同材料应有不同的变形结果";
    
    EXPECT_LT(result_steel.roof_max_deformation_mm, result_wood.roof_max_deformation_mm)
        << "高强度钢的变形应小于木材";
}

TEST_F(IntegrationTestFixture, FormationVehicles_FormationWithMixedVehicleTypes_Valid) {
    // 测试：不同类型车辆组成队形，布局和评估正常工作
    FormationConfig config;
    config.formation_type = "LINE";
    config.vehicle_count = 3;
    config.spacing_m = 4.0;
    config.wall_distance_m = 10.0;
    config.vehicle_types = {"fenyun_basic", "chongche_ram", "yunti_basic"};
    
    auto vehicles = formation_optimizer_->layout_formation(config);
    
    EXPECT_EQ(vehicles.size(), 3u);
    
    for (size_t i = 0; i < vehicles.size(); ++i) {
        EXPECT_EQ(vehicles[i].vehicle_type, config.vehicle_types[i]);
        EXPECT_GT(vehicles[i].vehicle_id, 0u);
    }
    
    FormationOptimizationRequest req;
    req.vehicle_count = 3;
    req.wall_height_m = 8.0;
    req.wall_length_m = 50.0;
    req.rock_fall_rate_per_sec = 0.5;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = config;
    
    auto opt_result = formation_optimizer_->optimize(req);
    
    EXPECT_GT(opt_result.candidate_formations.size(), 0u);
    EXPECT_GE(opt_result.survival_probability, 0.0);
    EXPECT_LE(opt_result.survival_probability, 1.0);
}

TEST_F(IntegrationTestFixture, FormationVehicles_FenyunFormation_OptimizesSpacing) {
    // 测试：全轒辒车编队，最优间距在合理范围内（2-5米）
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 30.0;
    req.rock_fall_rate_per_sec = 1.0;
    req.avg_rock_mass_kg = 50.0;
    req.baseline.formation_type = "LINE";
    req.baseline.vehicle_count = 5;
    req.baseline.spacing_m = 3.0;
    req.baseline.wall_distance_m = 10.0;
    
    auto result = formation_optimizer_->optimize(req);
    
    EXPECT_GE(result.best_formation.spacing_m, 2.0) 
        << "最优间距应至少为2米";
    EXPECT_LE(result.best_formation.spacing_m, 5.0) 
        << "最优间距应不超过5米";
    EXPECT_EQ(result.best_formation.vehicle_count, 5);
}

TEST_F(IntegrationTestFixture, DrivingAttacks_DrivingSession_EndToEnd) {
    // 测试：完整的驾驶+攻击端到端流程
    auto session = session_manager_->create_session("test_user", "fenyun_basic");
    
    EXPECT_FALSE(session.session_id.empty());
    EXPECT_TRUE(session_manager_->has_session(session.session_id));
    
    UserActionRequest throttle_req;
    throttle_req.session_id = session.session_id;
    throttle_req.action = "throttle";
    throttle_req.param1 = 1.0;
    throttle_req.param2 = 0.0;
    
    auto state_after_throttle = session_manager_->apply_action(throttle_req);
    EXPECT_GT(state_after_throttle.speed_ms, 0.0);
    
    auto attacks = session_manager_->process_time_step(session.session_id, 2.0);
    
    auto state_after = session_manager_->get_vehicle_state(session.session_id);
    
    EXPECT_GT(state_after.distance_traveled_m, 0.0) 
        << "行驶距离应大于0";
    EXPECT_GT(state_after.impacts_received, 0) 
        << "应受到攻击";
    EXPECT_LT(state_after.health_percent, 100.0) 
        << "HP在攻击后应下降";
}

TEST_F(IntegrationTestFixture, DrivingAttacks_MultipleSessions_Isolated) {
    // 测试：多个会话互相独立
    auto session1 = session_manager_->create_session("user1", "fenyun_basic");
    auto session2 = session_manager_->create_session("user2", "modern_m1a2");
    
    EXPECT_NE(session1.session_id, session2.session_id);
    
    UserActionRequest throttle1;
    throttle1.session_id = session1.session_id;
    throttle1.action = "throttle";
    throttle1.param1 = 1.0;
    session_manager_->apply_action(throttle1);
    
    auto state1 = session_manager_->get_vehicle_state(session1.session_id);
    auto state2 = session_manager_->get_vehicle_state(session2.session_id);
    
    EXPECT_GT(state1.speed_ms, 0.0);
    EXPECT_DOUBLE_EQ(state2.speed_ms, 0.0);
    
    auto attacks1 = session_manager_->trigger_rock_attack(session1.session_id, 2.0);
    
    state1 = session_manager_->get_vehicle_state(session1.session_id);
    state2 = session_manager_->get_vehicle_state(session2.session_id);
    
    EXPECT_LT(state1.health_percent, 100.0);
    EXPECT_DOUBLE_EQ(state2.health_percent, 100.0);
    EXPECT_GT(state1.impacts_received, 0);
    EXPECT_EQ(state2.impacts_received, 0);
    
    EXPECT_NE(state1.position_x, state2.position_x);
}

TEST_F(IntegrationTestFixture, CrossModuleConsistency_SameMaterialSameSimulation) {
    // 测试：VehicleComparator中的仿真 和 ImpactSimulator直接调用 的结果应该一致
    auto vp = config_->get_vehicle("fenyun_basic");
    
    SensorData sensor_data{};
    sensor_data.vehicle_id = 0;
    sensor_data.timestamp_ms = 12345;
    sensor_data.protection_thickness = vp.roof_thickness_mm;
    sensor_data.protection_material = vp.primary_material;
    sensor_data.ambient_temp = 20.0;
    sensor_data.impact_location_x = 3.25;
    sensor_data.impact_location_y = 1.4;
    sensor_data.rock_mass = 50.0;
    sensor_data.rock_velocity = 15.0;
    
    auto direct_result = simulator_->run_simulation(sensor_data);
    
    VehicleComparisonRequest req;
    req.vehicle_ids = {"fenyun_basic"};
    req.rock_mass_kg = 50.0;
    req.rock_velocity_ms = 15.0;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;
    
    auto comp_result = comparator_->compare_vehicles(req);
    
    ASSERT_EQ(comp_result.items.size(), 1u);
    const auto& comp_sim = comp_result.items[0].simulation;
    
    EXPECT_DOUBLE_EQ(direct_result.roof_max_deformation_mm, comp_sim.roof_max_deformation_mm)
        << "直接仿真与对比器仿真的变形量应一致";
    EXPECT_DOUBLE_EQ(direct_result.roof_von_mises_stress_mpa, comp_sim.roof_von_mises_stress_mpa)
        << "直接仿真与对比器仿真的应力应一致";
}

TEST_F(IntegrationTestFixture, CrossModuleConsistency_VehiclesConfigConsistency) {
    // 测试：ConfigLoader返回的VehicleProfile 与 VehicleComparator使用的一致
    auto config_ancient = config_->get_vehicles_by_era(VehicleEra::ANCIENT);
    auto comparator_ancient = comparator_->list_ancient_vehicles();
    
    EXPECT_EQ(config_ancient.size(), comparator_ancient.size())
        << "古代车辆数量应一致";
    
    auto config_modern = config_->get_vehicles_by_era(VehicleEra::MODERN);
    auto comparator_modern = comparator_->list_modern_vehicles();
    
    EXPECT_EQ(config_modern.size(), comparator_modern.size())
        << "现代车辆数量应一致";
    
    auto all_config = config_->vehicles();
    auto all_comparator = comparator_->list_all_vehicles();
    
    EXPECT_EQ(all_config.size(), all_comparator.size())
        << "所有车辆数量应一致";
}

TEST_F(IntegrationTestFixture, PerformanceSmoke_HundredComparisons_UnderOneSecond) {
    // 测试：性能冒烟 - 100次单车仿真耗时 < 1秒
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        SensorData data{};
        data.vehicle_id = static_cast<uint32_t>(i);
        data.timestamp_ms = static_cast<int64_t>(i);
        data.protection_thickness = 80.0;
        data.protection_material = "wood";
        data.rock_mass = 30.0 + i * 0.5;
        data.rock_velocity = 10.0 + i * 0.1;
        data.impact_location_x = 3.0;
        data.impact_location_y = 1.4;
        data.ambient_temp = 20.0;
        
        simulator_->run_simulation(data);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    EXPECT_LT(duration_ms, 1000) 
        << "100次仿真应在1秒内完成，实际耗时: " << duration_ms << "ms";
}

TEST_F(IntegrationTestFixture, DISABLED_PerformanceSmoke_FiftySessionCreations_Fast) {
    // 测试：性能冒烟 - 创建50个会话耗时 < 100ms (DISABLED 可选运行)
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::string> session_ids;
    session_ids.reserve(50);
    
    for (int i = 0; i < 50; ++i) {
        auto session = session_manager_->create_session(
            "user_" + std::to_string(i), 
            i % 2 == 0 ? "fenyun_basic" : "modern_m113"
        );
        session_ids.push_back(session.session_id);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    EXPECT_LT(duration_ms, 100) 
        << "创建50个会话应在100ms内完成，实际耗时: " << duration_ms << "ms";
    
    for (const auto& id : session_ids) {
        session_manager_->destroy_session(id);
    }
}

}
