#include "config/config_loader.h"
#include "common/data_types.h"
#include "common/json.h"

#include <gtest/gtest.h>
#include <string>

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

TEST(ConfigLoaderUnit, VehiclesConfig_LoadValidJson_ParsesCorrectly) {
    // 测试：从字符串加载vehicles配置，验证返回true，vehicles()大小>0
    ConfigLoader loader;
    bool result = loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson);
    EXPECT_TRUE(result);
    EXPECT_GT(loader.vehicles().size(), 0u);
}

TEST(ConfigLoaderUnit, VehiclesConfig_VehicleProfile_HasRequiredFields) {
    // 测试：每辆车都有必填字段
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    const auto& vehicles = loader.vehicles();
    ASSERT_GT(vehicles.size(), 0u);
    
    for (const auto& [id, vp] : vehicles) {
        EXPECT_FALSE(vp.id.empty()) << "车辆 " << id << " 缺少id";
        EXPECT_FALSE(vp.display_name.empty()) << "车辆 " << id << " 缺少display_name";
        EXPECT_GT(vp.length_m, 0.0) << "车辆 " << id << " length_m无效";
        EXPECT_GT(vp.width_m, 0.0) << "车辆 " << id << " width_m无效";
        EXPECT_GT(vp.weight_ton, 0.0) << "车辆 " << id << " weight_ton无效";
        EXPECT_GT(vp.roof_thickness_mm, 0.0) << "车辆 " << id << " roof_thickness_mm无效";
        EXPECT_FALSE(vp.primary_material.empty()) << "车辆 " << id << " 缺少primary_material";
    }
}

TEST(ConfigLoaderUnit, VehiclesConfig_AncientVehicles_CorrectEra) {
    // 测试：古代车 era == VehicleEra::ANCIENT
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto ancient = loader.get_vehicles_by_era(VehicleEra::ANCIENT);
    ASSERT_GT(ancient.size(), 0u);
    
    for (const auto& vp : ancient) {
        EXPECT_EQ(vp.era, VehicleEra::ANCIENT) << "车辆 " << vp.id << " era不正确";
    }
}

TEST(ConfigLoaderUnit, VehiclesConfig_ModernVehicles_CorrectEra) {
    // 测试：现代车 era == VehicleEra::MODERN
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto modern = loader.get_vehicles_by_era(VehicleEra::MODERN);
    ASSERT_GT(modern.size(), 0u);
    
    for (const auto& vp : modern) {
        EXPECT_EQ(vp.era, VehicleEra::MODERN) << "车辆 " << vp.id << " era不正确";
    }
}

TEST(ConfigLoaderUnit, VehiclesConfig_VehicleTypes_CorrectEnum) {
    // 测试：各车型枚举正确
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto fenyun = loader.get_vehicles_by_type(VehicleType::FENYUN);
    EXPECT_GT(fenyun.size(), 0u);
    for (const auto& vp : fenyun) {
        EXPECT_EQ(vp.type, VehicleType::FENYUN);
    }
    
    auto chongche = loader.get_vehicles_by_type(VehicleType::CHONGCHE);
    EXPECT_GT(chongche.size(), 0u);
    for (const auto& vp : chongche) {
        EXPECT_EQ(vp.type, VehicleType::CHONGCHE);
    }
    
    auto yunti = loader.get_vehicles_by_type(VehicleType::YUNTI);
    EXPECT_GT(yunti.size(), 0u);
    for (const auto& vp : yunti) {
        EXPECT_EQ(vp.type, VehicleType::YUNTI);
    }
    
    auto apc = loader.get_vehicles_by_type(VehicleType::MODERN_APC);
    EXPECT_GT(apc.size(), 0u);
    for (const auto& vp : apc) {
        EXPECT_EQ(vp.type, VehicleType::MODERN_APC);
    }
    
    auto tank = loader.get_vehicles_by_type(VehicleType::MODERN_TANK);
    EXPECT_GT(tank.size(), 0u);
    for (const auto& vp : tank) {
        EXPECT_EQ(vp.type, VehicleType::MODERN_TANK);
    }
    
    auto ifv = loader.get_vehicles_by_type(VehicleType::MODERN_IFV);
    EXPECT_GT(ifv.size(), 0u);
    for (const auto& vp : ifv) {
        EXPECT_EQ(vp.type, VehicleType::MODERN_IFV);
    }
}

TEST(ConfigLoaderUnit, VehiclesConfig_AvailableMaterials_NonEmpty) {
    // 测试：每辆车available_materials至少有一种材料
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    const auto& vehicles = loader.vehicles();
    for (const auto& [id, vp] : vehicles) {
        EXPECT_GT(vp.available_materials.size(), 0u) << "车辆 " << id << " 没有可用材料";
    }
}

TEST(ConfigLoaderUnit, VehiclesConfig_RoofThickness_Positive) {
    // 测试：所有车辆车顶厚度 > 0
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    const auto& vehicles = loader.vehicles();
    for (const auto& [id, vp] : vehicles) {
        EXPECT_GT(vp.roof_thickness_mm, 0.0) << "车辆 " << id << " 车顶厚度无效";
    }
}

TEST(ConfigLoaderUnit, MaterialsExtension_ModernMaterials_Loaded) {
    // 测试：现代材料都存在
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    EXPECT_TRUE(loader.has_material("modern_aluminum"));
    EXPECT_TRUE(loader.has_material("modern_steel"));
    EXPECT_TRUE(loader.has_material("modern_composite"));
    EXPECT_TRUE(loader.has_material("modern_du"));
    EXPECT_TRUE(loader.has_material("modern_era"));
}

TEST(ConfigLoaderUnit, MaterialsExtension_ModernSteel_HigherYieldThanIron) {
    // 测试：RHA钢的屈服强度 > 铸铁(iron)
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto iron = loader.get_material("iron");
    auto steel = loader.get_material("modern_steel");
    
    EXPECT_GT(steel.yield_strength_mpa, iron.yield_strength_mpa);
}

TEST(ConfigLoaderUnit, MaterialsExtension_DUArmor_HighestDensity) {
    // 测试：贫铀密度 > 其他所有材料
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto du = loader.get_material("modern_du");
    
    for (const auto& [name, mat] : loader.materials()) {
        if (name == "modern_du") continue;
        EXPECT_GT(du.density, mat.density) << "贫铀密度应大于 " << name;
    }
}

TEST(ConfigLoaderUnit, MaterialsExtension_ERA_HighToughness) {
    // 测试：爆炸反应装甲的韧性 > 传统材料
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto era = loader.get_material("modern_era");
    auto iron = loader.get_material("iron");
    auto wood = loader.get_material("wood");
    
    EXPECT_GT(era.toughness_mj_m3, iron.toughness_mj_m3);
    EXPECT_GT(era.toughness_mj_m3, wood.toughness_mj_m3);
}

TEST(ConfigLoaderUnit, MaterialsExtension_ModernMaterials_JohnsonCookParams) {
    // 测试：每种现代材料都有JC参数
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    std::vector<std::string> modern_materials = {
        "modern_aluminum", "modern_steel", "modern_composite",
        "modern_du", "modern_era"
    };
    
    for (const auto& name : modern_materials) {
        auto jc = loader.get_jc_params(name);
        EXPECT_GT(jc.A, 0.0) << name << " JC参数A无效";
        EXPECT_GT(jc.B, 0.0) << name << " JC参数B无效";
        EXPECT_GT(jc.n, 0.0) << name << " JC参数n无效";
    }
}

TEST(ConfigLoaderUnit, EdgeCases_InvalidVehiclesJson_ReturnsFalse) {
    // 测试：损坏的JSON返回false或优雅降级
    ConfigLoader loader;
    bool result = loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, "{ invalid json }");
    EXPECT_FALSE(result);
}

TEST(ConfigLoaderUnit, EdgeCases_EmptyVehiclesJson_ValidButEmpty) {
    // 测试：空vehicles对象，加载成功但大小为0
    ConfigLoader loader;
    const char* empty_vehicles = R"({"version": "1.0.0", "vehicles": {}})";
    bool result = loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, empty_vehicles);
    EXPECT_TRUE(result);
    EXPECT_EQ(loader.vehicles().size(), 0u);
}

TEST(ConfigLoaderUnit, EdgeCases_MissingFields_DefaultValues) {
    // 测试：缺少可选字段的车辆仍能加载，使用默认值
    ConfigLoader loader;
    const char* minimal_vehicle = R"({
      "version": "1.0.0",
      "vehicles": {
        "minimal_car": {
          "display_name": "极简车",
          "era": "ancient",
          "type": "FENYUN",
          "length_m": 5.0,
          "width_m": 2.0,
          "weight_ton": 1.0,
          "roof_thickness_mm": 10.0,
          "primary_material": "wood",
          "available_materials": ["wood"]
        }
      }
    })";
    
    bool result = loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, minimal_vehicle);
    EXPECT_TRUE(result);
    EXPECT_EQ(loader.vehicles().size(), 1u);
    
    auto vp = loader.get_vehicle("minimal_car");
    EXPECT_EQ(vp.display_name, "极简车");
    EXPECT_EQ(vp.era, VehicleEra::ANCIENT);
    EXPECT_EQ(vp.type, VehicleType::FENYUN);
}

TEST(ConfigLoaderUnit, EdgeCases_DuplicateIds_LastWins) {
    // 测试：重复id时最后一个覆盖前一个
    ConfigLoader loader;
    const char* dup_vehicles = R"({
      "version": "1.0.0",
      "vehicles": {
        "test_car": {
          "display_name": "第一版",
          "era": "ancient",
          "type": "FENYUN",
          "length_m": 5.0,
          "width_m": 2.0,
          "weight_ton": 1.0,
          "roof_thickness_mm": 10.0,
          "primary_material": "wood",
          "available_materials": ["wood"]
        },
        "test_car": {
          "display_name": "第二版",
          "era": "modern",
          "type": "MODERN_TANK",
          "length_m": 10.0,
          "width_m": 4.0,
          "weight_ton": 50.0,
          "roof_thickness_mm": 200.0,
          "primary_material": "modern_steel",
          "available_materials": ["modern_steel"]
        }
      }
    })";
    
    bool result = loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, dup_vehicles);
    EXPECT_TRUE(result);
    
    auto vp = loader.get_vehicle("test_car");
    EXPECT_EQ(vp.display_name, "第二版");
    EXPECT_EQ(vp.era, VehicleEra::MODERN);
    EXPECT_EQ(vp.type, VehicleType::MODERN_TANK);
}

TEST(ConfigLoaderUnit, QueryFunctions_GetVehicle_Existing_ReturnsCorrect) {
    // 测试：已知id的车辆能正确获取
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto vp = loader.get_vehicle("fenyun_basic");
    EXPECT_EQ(vp.id, "fenyun_basic");
    EXPECT_EQ(vp.display_name, "轒辒车(基础型)");
    EXPECT_EQ(vp.era, VehicleEra::ANCIENT);
    EXPECT_EQ(vp.type, VehicleType::FENYUN);
    EXPECT_DOUBLE_EQ(vp.length_m, 6.5);
}

TEST(ConfigLoaderUnit, QueryFunctions_GetVehicle_Nonexistent_ReturnsDefault) {
    // 测试：不存在的id返回默认值
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto vp = loader.get_vehicle("nonexistent_vehicle");
    EXPECT_FALSE(vp.id.empty());
}

TEST(ConfigLoaderUnit, QueryFunctions_GetVehiclesByEra_CorrectCount) {
    // 测试：按时代筛选数量正确
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto ancient = loader.get_vehicles_by_era(VehicleEra::ANCIENT);
    EXPECT_EQ(ancient.size(), 3u);
    
    auto modern = loader.get_vehicles_by_era(VehicleEra::MODERN);
    EXPECT_EQ(modern.size(), 3u);
}

TEST(ConfigLoaderUnit, QueryFunctions_GetVehiclesByType_CorrectCount) {
    // 测试：按类型筛选数量正确
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    EXPECT_EQ(loader.get_vehicles_by_type(VehicleType::FENYUN).size(), 1u);
    EXPECT_EQ(loader.get_vehicles_by_type(VehicleType::CHONGCHE).size(), 1u);
    EXPECT_EQ(loader.get_vehicles_by_type(VehicleType::YUNTI).size(), 1u);
    EXPECT_EQ(loader.get_vehicles_by_type(VehicleType::MODERN_APC).size(), 1u);
    EXPECT_EQ(loader.get_vehicles_by_type(VehicleType::MODERN_TANK).size(), 1u);
    EXPECT_EQ(loader.get_vehicles_by_type(VehicleType::MODERN_IFV).size(), 1u);
}

TEST(ConfigLoaderUnit, QueryFunctions_HasVehicle_Existing_True) {
    // 测试：存在返回true
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    EXPECT_TRUE(loader.has_vehicle("fenyun_basic"));
    EXPECT_TRUE(loader.has_vehicle("modern_m1a2"));
}

TEST(ConfigLoaderUnit, QueryFunctions_HasVehicle_Nonexistent_False) {
    // 测试：不存在返回false
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    EXPECT_FALSE(loader.has_vehicle("nonexistent"));
    EXPECT_FALSE(loader.has_vehicle(""));
}

TEST(ConfigLoaderUnit, Consistency_VehiclePrimaryMaterial_ExistsInMaterials) {
    // 测试：每辆车的primary_material在materials库中都存在
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    const auto& vehicles = loader.vehicles();
    for (const auto& [id, vp] : vehicles) {
        EXPECT_TRUE(loader.has_material(vp.primary_material))
            << "车辆 " << id << " 的primary_material " << vp.primary_material << " 不存在于材料库中";
    }
}

TEST(ConfigLoaderUnit, Consistency_AvailableMaterials_AllExist) {
    // 测试：每辆车的available_materials列表中材料都在materials库中
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    const auto& vehicles = loader.vehicles();
    for (const auto& [id, vp] : vehicles) {
        for (const auto& mat_name : vp.available_materials) {
            EXPECT_TRUE(loader.has_material(mat_name))
                << "车辆 " << id << " 的available_material " << mat_name << " 不存在于材料库中";
        }
    }
}

TEST(ConfigLoaderUnit, Consistency_EraTypeConsistency) {
    // 测试：古代车不会出现MODERN类型，现代车不会出现ANCIENT类型
    ConfigLoader loader;
    ASSERT_TRUE(loader.load_from_string(kSystemJson, kMaterialsJson, kAhpJson, kVehiclesJson));
    
    auto ancient = loader.get_vehicles_by_era(VehicleEra::ANCIENT);
    for (const auto& vp : ancient) {
        EXPECT_NE(vp.type, VehicleType::MODERN_APC);
        EXPECT_NE(vp.type, VehicleType::MODERN_TANK);
        EXPECT_NE(vp.type, VehicleType::MODERN_IFV);
    }
    
    auto modern = loader.get_vehicles_by_era(VehicleEra::MODERN);
    for (const auto& vp : modern) {
        EXPECT_NE(vp.type, VehicleType::FENYUN);
        EXPECT_NE(vp.type, VehicleType::CHONGCHE);
        EXPECT_NE(vp.type, VehicleType::YUNTI);
    }
}

}
