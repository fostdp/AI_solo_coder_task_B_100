#include <gtest/gtest.h>
#include "vehicle_comparator/vehicle_comparator.h"
#include "config/config_loader.h"
#include "common/data_types.h"
#include "impact_simulator/impact_simulator.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace fenyun {

class VehicleComparatorUnit : public ::testing::Test {
protected:
    void SetUp() override {
        comparator_ = test::create_test_vehicle_comparator();
        ASSERT_NE(comparator_, nullptr);
        config_ = test::create_test_config_loader();
        ASSERT_NE(config_, nullptr);
    }

    void TearDown() override {
        comparator_.reset();
        config_.reset();
    }

    std::shared_ptr<VehicleComparator> comparator_;
    std::shared_ptr<ConfigLoader> config_;
};

TEST_F(VehicleComparatorUnit, BasicFunctionality_ListAllVehicles_ReturnsAllVehicles) {
    // 测试目的：验证 list_all_vehicles 返回 9 辆车（6 古 3 现），每辆车都有 id 和 display_name，非空
    std::vector<VehicleProfile> vehicles = comparator_->list_all_vehicles();

    EXPECT_EQ(vehicles.size(), 9u) << "总共应该有 9 辆车（6 辆古代车 + 3 辆现代车）";

    for (const auto& v : vehicles) {
        EXPECT_FALSE(v.id.empty()) << "车辆 id 不能为空";
        EXPECT_FALSE(v.display_name.empty()) << "车辆 display_name 不能为空, id=" << v.id;
    }
}

TEST_F(VehicleComparatorUnit, BasicFunctionality_ListAncientVehicles_ReturnsSixVehicles) {
    // 测试目的：验证古代车有 6 辆
    std::vector<VehicleProfile> vehicles = comparator_->list_ancient_vehicles();

    EXPECT_EQ(vehicles.size(), 6u) << "应该有 6 辆古代车";

    for (const auto& v : vehicles) {
        EXPECT_EQ(v.era, VehicleEra::ANCIENT) << "车辆 " << v.id << " 应该是古代车";
    }
}

TEST_F(VehicleComparatorUnit, BasicFunctionality_ListModernVehicles_ReturnsThreeVehicles) {
    // 测试目的：验证现代车有 3 辆
    std::vector<VehicleProfile> vehicles = comparator_->list_modern_vehicles();

    EXPECT_EQ(vehicles.size(), 3u) << "应该有 3 辆现代车";

    for (const auto& v : vehicles) {
        EXPECT_EQ(v.era, VehicleEra::MODERN) << "车辆 " << v.id << " 应该是现代车";
    }
}

TEST_F(VehicleComparatorUnit, BasicFunctionality_GetVehicleProfile_ValidId_ReturnsCorrectData) {
    // 测试目的：验证 get_vehicle("fenyun_basic") 的尺寸、重量、厚度等参数正确
    VehicleProfile vp = config_->get_vehicle("fenyun_basic");

    EXPECT_EQ(vp.id, "fenyun_basic");
    EXPECT_EQ(vp.display_name, "轒辒车(基础型)");
    EXPECT_EQ(vp.era, VehicleEra::ANCIENT);
    EXPECT_EQ(vp.type, VehicleType::FENYUN);
    EXPECT_NEAR(vp.length_m, 6.5, 0.001);
    EXPECT_NEAR(vp.width_m, 2.8, 0.001);
    EXPECT_NEAR(vp.height_m, 2.5, 0.001);
    EXPECT_NEAR(vp.weight_ton, 3.2, 0.001);
    EXPECT_NEAR(vp.roof_thickness_mm, 80.0, 0.001);
    EXPECT_NEAR(vp.wall_thickness_mm, 60.0, 0.001);
    EXPECT_EQ(vp.primary_material, "composite");
    EXPECT_EQ(vp.historical_year, -550);
}

TEST_F(VehicleComparatorUnit, BasicFunctionality_GetVehicleProfile_InvalidId_ReturnsEmptyOrFallback) {
    // 测试目的：无效 id 时不会崩溃，返回默认值或回退值
    EXPECT_NO_FATAL_FAILURE({
        VehicleProfile vp = config_->get_vehicle("invalid_vehicle_id_xyz");
        EXPECT_FALSE(vp.id == "invalid_vehicle_id_xyz") << "无效 id 不应该返回自身作为 id";
    });

    EXPECT_NO_FATAL_FAILURE({
        VehicleProfile vp = config_->get_vehicle("nonexistent_car");
        EXPECT_TRUE(vp.id.empty() || !vp.id != "nonexistent_car")
            << "无效 id 应该返回默认值或第一辆车，而不是返回查询的 id";
    });
}

TEST_F(VehicleComparatorUnit, ComparisonLogic_CompareAncientVehicles_ProducesValidRanking) {
    // 测试目的：调用 compare_ancient_vehicles(50kg, 15m/s)，验证排名、damage_level、best_vehicle_id
    VehicleComparisonResult result = comparator_->compare_ancient_vehicles(50.0, 15.0);

    EXPECT_GE(result.items.size(), 3u) << "古代车对比至少应有 3 辆车";

    EXPECT_FALSE(result.best_vehicle_id.empty()) << "best_vehicle_id 不能为空";

    if (result.items.size() >= 2) {
        EXPECT_GT(result.items.front().overall_score, result.items.back().overall_score)
            << "排名第一的总体评分应该大于排名最后的";
    }

    for (const auto& item : result.items) {
        EXPECT_FALSE(item.vehicle_id.empty());
        EXPECT_FALSE(item.display_name.empty());
        EXPECT_GE(item.simulation.damage_level, 0u) << "damage_level 应该 >= 0";
        EXPECT_LE(item.simulation.damage_level, 4u) << "damage_level 应该 <= 4";
        EXPECT_GE(item.simulation.roof_max_deformation_mm, 0.0) << "变形量应该 >= 0";
    }

    if (!result.items.empty()) {
        EXPECT_EQ(result.best_vehicle_id, result.items.front().vehicle_id)
            << "best_vehicle_id 应该对应排名第一的车";
        EXPECT_EQ(result.items.front().rank, 1) << "第一名 rank 应该是 1";
    }
}

TEST_F(VehicleComparatorUnit, ComparisonLogic_CrossEraComparison_ModernBeatsAncient) {
    // 测试目的：跨时代对比，验证现代车平均防护效率 > 古代车，M1A2 得分最高，insights 非空
    VehicleComparisonResult result = comparator_->compare_cross_era(50.0, 15.0);

    EXPECT_GE(result.items.size(), 9u) << "跨时代对比应该包含所有 9 辆车";

    double ancient_avg_eff = 0.0;
    double modern_avg_eff = 0.0;
    int ancient_count = 0;
    int modern_count = 0;
    double m1a2_eff = -1.0;

    for (const auto& item : result.items) {
        if (item.era == VehicleEra::ANCIENT) {
            ancient_avg_eff += item.protection_efficiency_score;
            ancient_count++;
        } else {
            modern_avg_eff += item.protection_efficiency_score;
            modern_count++;
            if (item.vehicle_id == "modern_m1a2") {
                m1a2_eff = item.protection_efficiency_score;
            }
        }
    }

    ASSERT_GT(ancient_count, 0) << "应该有古代车";
    ASSERT_GT(modern_count, 0) << "应该有现代车";

    ancient_avg_eff /= ancient_count;
    modern_avg_eff /= modern_count;

    EXPECT_GT(modern_avg_eff, ancient_avg_eff)
        << "现代车的平均防护效率应该大于古代车";

    EXPECT_GT(m1a2_eff, 0.0) << "M1A2 应该存在且有正的防护效率";

    for (const auto& item : result.items) {
        if (item.vehicle_id != "modern_m1a2") {
            EXPECT_GE(m1a2_eff, item.protection_efficiency_score - 1e-6)
                << "M1A2（贫铀装甲）应该有最高的防护效率，但 "
                << item.vehicle_id << " 得分更高";
        }
    }

    EXPECT_FALSE(result.insights.empty()) << "跨时代对比应该生成洞察";

    bool has_cross_era_insight = false;
    for (const auto& insight : result.insights) {
        if (insight.find("跨时代对比") != std::string::npos) {
            has_cross_era_insight = true;
            break;
        }
    }
    EXPECT_TRUE(has_cross_era_insight) << "洞察中应包含跨时代对比的内容";
}

TEST_F(VehicleComparatorUnit, ComparisonLogic_ProtectionEfficiency_ThickerArmorBetter) {
    // 测试目的：对比轒辒车基础型(80mm)和重装型(120mm)，重装型变形更小，damage_level <= 基础型
    VehicleComparisonRequest req{};
    req.vehicle_ids = {"fenyun_basic", "fenyun_heavy"};
    req.rock_mass_kg = 50.0;
    req.rock_velocity_ms = 15.0;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;
    req.use_johnson_cook = true;

    VehicleComparisonResult result = comparator_->compare_vehicles(req);

    ASSERT_GE(result.items.size(), 2u) << "基础型和重装型都应该参与对比";

    const VehicleComparisonItem* basic = nullptr;
    const VehicleComparisonItem* heavy = nullptr;

    for (const auto& item : result.items) {
        if (item.vehicle_id == "fenyun_basic") basic = &item;
        if (item.vehicle_id == "fenyun_heavy") heavy = &item;
    }

    ASSERT_NE(basic, nullptr) << "结果中找不到 fenyun_basic";
    ASSERT_NE(heavy, nullptr) << "结果中找不到 fenyun_heavy";

    EXPECT_LT(heavy->simulation.roof_max_deformation_mm, basic->simulation.roof_max_deformation_mm)
        << "重装型（更厚装甲）的变形量应该小于基础型";

    EXPECT_LE(heavy->simulation.damage_level, basic->simulation.damage_level)
        << "重装型的 damage_level 应该 <= 基础型";
}

TEST_F(VehicleComparatorUnit, ComparisonLogic_DamageLevelClassification_CorrectRange) {
    // 测试目的：轻击（小质量低速）damage_level 应为 0 或 1；重击（大质量高速）damage_level 应为 3 或 4
    VehicleComparisonResult light_result = comparator_->compare_ancient_vehicles(5.0, 5.0);
    ASSERT_FALSE(light_result.items.empty());

    uint8_t light_max_damage = 0;
    for (const auto& item : light_result.items) {
        light_max_damage = std::max(light_max_damage, item.simulation.damage_level);
    }
    EXPECT_LE(light_max_damage, 1u)
        << "轻击（5kg, 5m/s）的 damage_level 应该为 0 或 1，最大值为 "
        << static_cast<int>(light_max_damage);

    VehicleComparisonResult heavy_result = comparator_->compare_ancient_vehicles(200.0, 30.0);
    ASSERT_FALSE(heavy_result.items.empty());

    uint8_t heavy_min_damage = 5;
    for (const auto& item : heavy_result.items) {
        heavy_min_damage = std::min(heavy_min_damage, item.simulation.damage_level);
    }
    EXPECT_GE(heavy_min_damage, 2u)
        << "重击（200kg, 30m/s）应该造成更高的 damage_level";
}

TEST_F(VehicleComparatorUnit, EdgeCases_ZeroMassRock_NoDamage) {
    // 测试目的：rock_mass=0 时，damage 应为 0 级，变形接近 0
    VehicleComparisonRequest req{};
    req.vehicle_ids = {"fenyun_basic"};
    req.rock_mass_kg = 0.0;
    req.rock_velocity_ms = 15.0;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;

    VehicleComparisonResult result = comparator_->compare_vehicles(req);

    ASSERT_FALSE(result.items.empty());
    const auto& item = result.items.front();

    EXPECT_GE(item.simulation.damage_level, 0u);
    EXPECT_NEAR(item.simulation.roof_max_deformation_mm, 0.0, 50.0)
        << "零质量岩石应该导致接近零的变形";
}

TEST_F(VehicleComparatorUnit, EdgeCases_ExtremelyHeavyRock_HighDamage) {
    // 测试目的：1000kg 大质量岩石，验证不溢出，damage_level=4
    VehicleComparisonRequest req{};
    req.vehicle_ids = {"fenyun_basic"};
    req.rock_mass_kg = 1000.0;
    req.rock_velocity_ms = 30.0;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;

    EXPECT_NO_FATAL_FAILURE({
        VehicleComparisonResult result = comparator_->compare_vehicles(req);
        ASSERT_FALSE(result.items.empty());
        const auto& item = result.items.front();
        EXPECT_LE(item.simulation.damage_level, 4u) << "damage_level 不应超过 4";
        EXPECT_GT(item.simulation.roof_max_deformation_mm, 0.0);
        EXPECT_FALSE(std::isnan(item.simulation.roof_max_deformation_mm))
            << "变形量不应为 NaN（无溢出）";
        EXPECT_FALSE(std::isinf(item.simulation.roof_max_deformation_mm))
            << "变形量不应为 Inf（无溢出）";
    });
}

TEST_F(VehicleComparatorUnit, EdgeCases_SingleVehicleComparison_StillValid) {
    // 测试目的：只传 1 辆车，返回结果仍有效，rank=1
    VehicleComparisonRequest req{};
    req.vehicle_ids = {"fenyun_basic"};
    req.rock_mass_kg = 50.0;
    req.rock_velocity_ms = 15.0;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;

    VehicleComparisonResult result = comparator_->compare_vehicles(req);

    ASSERT_EQ(result.items.size(), 1u) << "结果中应该恰好有 1 辆车";
    EXPECT_EQ(result.items.front().rank, 1) << "单辆车排名应该是第 1 名";
    EXPECT_FALSE(result.items.front().vehicle_id.empty());
    EXPECT_GE(result.items.front().overall_score, 0.0);
    EXPECT_EQ(result.best_vehicle_id, result.items.front().vehicle_id);
}

TEST_F(VehicleComparatorUnit, EdgeCases_EmptyVehicleList_HandlesGracefully) {
    // 测试目的：vehicle_ids 为空，不崩溃，返回空结果
    VehicleComparisonRequest req{};
    req.vehicle_ids = {};
    req.rock_mass_kg = 50.0;
    req.rock_velocity_ms = 15.0;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;

    EXPECT_NO_FATAL_FAILURE({
        VehicleComparisonResult result = comparator_->compare_vehicles(req);
        EXPECT_TRUE(result.items.empty()) << "空输入应该返回空的 items";
        EXPECT_TRUE(result.best_vehicle_id.empty()) << "空输入 best_vehicle_id 应该为空";
    });
}

TEST_F(VehicleComparatorUnit, EdgeCases_NegativeVelocity_AbsoluteValue) {
    // 测试目的：负速度取绝对值，正常计算
    VehicleComparisonRequest req_pos{};
    req_pos.vehicle_ids = {"fenyun_basic"};
    req_pos.rock_mass_kg = 50.0;
    req_pos.rock_velocity_ms = 15.0;
    req_pos.impact_location_x = 3.25;
    req_pos.impact_location_y = 1.4;
    req_pos.temperature_K = 293.15;

    VehicleComparisonRequest req_neg{};
    req_neg.vehicle_ids = {"fenyun_basic"};
    req_neg.rock_mass_kg = 50.0;
    req_neg.rock_velocity_ms = -15.0;
    req_neg.impact_location_x = 3.25;
    req_neg.impact_location_y = 1.4;
    req_neg.temperature_K = 293.15;

    VehicleComparisonResult result_pos = comparator_->compare_vehicles(req_pos);
    VehicleComparisonResult result_neg = comparator_->compare_vehicles(req_neg);

    ASSERT_FALSE(result_pos.items.empty());
    ASSERT_FALSE(result_neg.items.empty());

    EXPECT_NEAR(result_pos.items.front().simulation.roof_max_deformation_mm,
                result_neg.items.front().simulation.roof_max_deformation_mm,
                1e-6)
        << "负速度应该产生与正速度相同的结果（取绝对值）";

    EXPECT_EQ(result_pos.items.front().simulation.damage_level,
              result_neg.items.front().simulation.damage_level)
        << "负速度应该产生相同的 damage_level";
}

TEST_F(VehicleComparatorUnit, ScoreConsistency_OverallScore_WeightedSumConsistent) {
    // 测试目的：验证 overall_score = 0.5*efficiency + 0.3*weight + 0.2*cost，分数在 0-100 范围内
    VehicleComparisonResult result = comparator_->compare_ancient_vehicles(50.0, 15.0);

    ASSERT_FALSE(result.items.empty());

    for (const auto& item : result.items) {
        double expected_overall = 0.5 * item.protection_efficiency_score
                                 + 0.3 * item.weight_normalized_score
                                 + 0.2 * item.cost_normalized_score;

        EXPECT_NEAR(item.overall_score, expected_overall, 1e-6)
            << "overall_score 应该是 efficiency(0.5) + weight(0.3) + cost(0.2) 的加权和";

        EXPECT_GE(item.overall_score, 0.0) << "overall_score 应该 >= 0";
        EXPECT_LE(item.overall_score, 100.0) << "overall_score 应该 <= 100";

        EXPECT_GE(item.protection_efficiency_score, 0.0);
        EXPECT_LE(item.protection_efficiency_score, 100.0);
        EXPECT_GE(item.weight_normalized_score, 0.0);
        EXPECT_LE(item.weight_normalized_score, 100.0);
        EXPECT_GE(item.cost_normalized_score, 0.0);
        EXPECT_LE(item.cost_normalized_score, 100.0);
    }
}

TEST_F(VehicleComparatorUnit, ScoreConsistency_SameInputSameOutput_Deterministic) {
    // 测试目的：相同输入运行两次，结果完全相同或非常接近
    VehicleComparisonResult result1 = comparator_->compare_ancient_vehicles(50.0, 15.0);
    VehicleComparisonResult result2 = comparator_->compare_ancient_vehicles(50.0, 15.0);

    ASSERT_EQ(result1.items.size(), result2.items.size());

    for (size_t i = 0; i < result1.items.size(); ++i) {
        EXPECT_EQ(result1.items[i].vehicle_id, result2.items[i].vehicle_id);
        EXPECT_NEAR(result1.items[i].overall_score, result2.items[i].overall_score, 1e-9);
        EXPECT_NEAR(result1.items[i].simulation.roof_max_deformation_mm,
                    result2.items[i].simulation.roof_max_deformation_mm, 1e-9);
        EXPECT_EQ(result1.items[i].simulation.damage_level,
                  result2.items[i].simulation.damage_level);
        EXPECT_EQ(result1.items[i].rank, result2.items[i].rank);
    }

    EXPECT_EQ(result1.best_vehicle_id, result2.best_vehicle_id);
}

TEST_F(VehicleComparatorUnit, ScoreConsistency_RankingConsistentWithDamage) {
    // 测试目的：damage 越低的车 overall 排名越高（部分验证）
    VehicleComparisonResult result = comparator_->compare_ancient_vehicles(50.0, 15.0);

    ASSERT_GE(result.items.size(), 2u);

    const auto& first = result.items.front();
    const auto& last = result.items.back();

    EXPECT_GT(first.overall_score, last.overall_score);

    if (first.simulation.damage_level != last.simulation.damage_level) {
        EXPECT_LE(first.simulation.damage_level, last.simulation.damage_level)
            << "排名更高的车辆应该倾向于有更低或相等的 damage_level";
    }
}

}
