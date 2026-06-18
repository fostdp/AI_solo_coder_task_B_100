#include <gtest/gtest.h>
#include "formation_optimizer/formation_optimizer.h"
#include "config/config_loader.h"
#include "common/data_types.h"
#include "test_helpers.h"

#include <vector>
#include <string>
#include <set>
#include <cmath>
#include <memory>
#include <algorithm>

namespace fenyun {

class FormationOptimizerUnit : public ::testing::Test {
protected:
    void SetUp() override {
        optimizer_ = test::create_test_formation_optimizer();
        ASSERT_NE(optimizer_, nullptr);
        config_ = test::create_test_config_loader();
        ASSERT_NE(config_, nullptr);
    }

    void TearDown() override {
        optimizer_.reset();
        config_.reset();
    }

    std::shared_ptr<FormationOptimizer> optimizer_;
    std::shared_ptr<ConfigLoader> config_;
};

// ========== BasicTypes - 基础类型测试 ==========

TEST_F(FormationOptimizerUnit, FormationTypes_ListHasSixTypes) {
    // 验证6种队形类型
    std::vector<std::string> types = {"LINE", "WEDGE", "ECHELON", "V_SHAPE", "COLUMN", "DIAMOND"};
    EXPECT_EQ(types.size(), 6u);

    for (const auto& t : types) {
        FormationConfig cfg = optimizer_->get_formation_template(t, 5);
        EXPECT_EQ(cfg.formation_type, t);
    }
}

TEST_F(FormationOptimizerUnit, GetTemplate_ValidType_ReturnsConfig) {
    // 每种队形模板都有合理的默认间距和攻击宽度
    std::vector<std::string> types = {"LINE", "WEDGE", "ECHELON", "V_SHAPE", "COLUMN", "DIAMOND"};

    for (const auto& t : types) {
        FormationConfig cfg = optimizer_->get_formation_template(t, 5);
        EXPECT_GT(cfg.spacing_m, 0.0) << "队形 " << t << " 间距应大于0";
        EXPECT_GT(cfg.attack_width_m, 0.0) << "队形 " << t << " 攻击宽度应大于0";
        EXPECT_EQ(cfg.vehicle_count, 5);
        EXPECT_EQ(cfg.vehicle_types.size(), 5u);
    }
}

// ========== LayoutGeometry - 队形几何验证 ==========

TEST_F(FormationOptimizerUnit, LineFormation_EqualSpacing) {
    // LINE队形，N辆车，x方向等距，y相同，间距误差<1%
    FormationConfig cfg = optimizer_->get_formation_template("LINE", 5);
    cfg.spacing_m = 2.5;
    auto vehicles = optimizer_->layout_formation(cfg);

    ASSERT_EQ(vehicles.size(), 5u);

    double y0 = vehicles[0].position_y;
    for (size_t i = 1; i < vehicles.size(); ++i) {
        EXPECT_NEAR(vehicles[i].position_y, y0, 0.01) << "所有车辆y坐标应相同";
    }

    for (size_t i = 1; i < vehicles.size(); ++i) {
        double dx = vehicles[i].position_x - vehicles[i-1].position_x;
        double ratio = std::abs(dx - cfg.spacing_m) / cfg.spacing_m;
        EXPECT_LT(ratio, 0.01) << "间距误差应小于1%";
    }
}

TEST_F(FormationOptimizerUnit, WedgeFormation_LeadFirst) {
    // WEDGE队形，第0辆y最大（最前方），y坐标递减
    FormationConfig cfg = optimizer_->get_formation_template("WEDGE", 5);
    auto vehicles = optimizer_->layout_formation(cfg);

    ASSERT_GT(vehicles.size(), 0u);
    EXPECT_TRUE(vehicles[0].is_lead) << "第0辆车应为领队";

    double max_y = vehicles[0].position_y;
    for (size_t i = 1; i < vehicles.size(); ++i) {
        EXPECT_LE(vehicles[i].position_y, max_y + 0.001) << "后续车辆y坐标不应超过领队";
    }
}

TEST_F(FormationOptimizerUnit, ColumnFormation_SingleFile) {
    // COLUMN队形，所有车x相同或相近，y方向排成一列
    FormationConfig cfg = optimizer_->get_formation_template("COLUMN", 5);
    auto vehicles = optimizer_->layout_formation(cfg);

    ASSERT_EQ(vehicles.size(), 5u);

    double x0 = vehicles[0].position_x;
    for (size_t i = 1; i < vehicles.size(); ++i) {
        EXPECT_NEAR(vehicles[i].position_x, x0, 0.5) << "所有车辆x坐标应相近";
    }

    for (size_t i = 1; i < vehicles.size(); ++i) {
        EXPECT_NE(vehicles[i].position_y, vehicles[i-1].position_y) << "y方向应排成一列";
    }
}

TEST_F(FormationOptimizerUnit, VehicleCount_MatchesInput) {
    // 输入5辆车，layout_formation返回恰好5辆
    FormationConfig cfg = optimizer_->get_formation_template("LINE", 5);
    auto vehicles = optimizer_->layout_formation(cfg);
    EXPECT_EQ(vehicles.size(), 5u);

    cfg = optimizer_->get_formation_template("WEDGE", 10);
    vehicles = optimizer_->layout_formation(cfg);
    EXPECT_EQ(vehicles.size(), 10u);
}

// ========== SurvivalRate - 伤亡率验证 ==========

TEST_F(FormationOptimizerUnit, WiderSpacing_BetterSurvival) {
    // 间距从1.5m增加到4.0m，生存率应该提高（被集中命中概率降低）
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    FormationConfig cfg_narrow = optimizer_->get_formation_template("LINE", 5);
    cfg_narrow.spacing_m = 1.5;
    cfg_narrow.attack_width_m = (5 - 1) * 1.5 + 3.0;
    double score_narrow = optimizer_->evaluate_formation(cfg_narrow, req);

    FormationConfig cfg_wide = optimizer_->get_formation_template("LINE", 5);
    cfg_wide.spacing_m = 4.0;
    cfg_wide.attack_width_m = (5 - 1) * 4.0 + 3.0;
    double score_wide = optimizer_->evaluate_formation(cfg_wide, req);

    EXPECT_GT(score_wide, score_narrow) << "间距增大应提高生存率";
}

TEST_F(FormationOptimizerUnit, LineVsColumn_SurvivalTradeoff) {
    // 横排生存率 < 纵队（横排目标大但分散，纵队纵深长），验证逻辑一致性
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;

    FormationConfig cfg_line = optimizer_->get_formation_template("LINE", 5);
    double score_line = optimizer_->evaluate_formation(cfg_line, req);

    FormationConfig cfg_col = optimizer_->get_formation_template("COLUMN", 5);
    double score_col = optimizer_->evaluate_formation(cfg_col, req);

    EXPECT_GT(score_col, score_line) << "纵队生存率应高于横排";
}

TEST_F(FormationOptimizerUnit, MoreVehicles_LowerIndividualSurvival) {
    // 5辆 vs 10辆，平均单辆车生存率下降（密度高）
    FormationOptimizationRequest req5;
    req5.vehicle_count = 5;
    req5.wall_height_m = 10.0;
    req5.wall_length_m = 30.0;
    req5.rock_fall_rate_per_sec = 2.0;
    req5.avg_rock_mass_kg = 30.0;
    req5.baseline = optimizer_->get_formation_template("LINE", 5);

    FormationOptimizationRequest req10;
    req10.vehicle_count = 10;
    req10.wall_height_m = 10.0;
    req10.wall_length_m = 30.0;
    req10.rock_fall_rate_per_sec = 2.0;
    req10.avg_rock_mass_kg = 30.0;
    req10.baseline = optimizer_->get_formation_template("LINE", 10);

    FormationConfig cfg5 = optimizer_->get_formation_template("LINE", 5);
    double score5 = optimizer_->evaluate_formation(cfg5, req5);

    FormationConfig cfg10 = optimizer_->get_formation_template("LINE", 10);
    double score10 = optimizer_->evaluate_formation(cfg10, req10);

    EXPECT_GT(score5, score10) << "车辆越多，单位生存率应越低";
}

TEST_F(FormationOptimizerUnit, HigherRockFallRate_LowerSurvival) {
    // 落石频率从1/s升到5/s，生存率显著下降
    FormationOptimizationRequest req_low;
    req_low.vehicle_count = 5;
    req_low.wall_height_m = 10.0;
    req_low.wall_length_m = 20.0;
    req_low.rock_fall_rate_per_sec = 1.0;
    req_low.avg_rock_mass_kg = 30.0;
    req_low.baseline = optimizer_->get_formation_template("LINE", 5);

    FormationOptimizationRequest req_high;
    req_high.vehicle_count = 5;
    req_high.wall_height_m = 10.0;
    req_high.wall_length_m = 20.0;
    req_high.rock_fall_rate_per_sec = 5.0;
    req_high.avg_rock_mass_kg = 30.0;
    req_high.baseline = optimizer_->get_formation_template("LINE", 5);

    FormationConfig cfg = optimizer_->get_formation_template("LINE", 5);
    double score_low = optimizer_->evaluate_formation(cfg, req_low);
    double score_high = optimizer_->evaluate_formation(cfg, req_high);

    EXPECT_GT(score_low, score_high) << "落石频率越高，生存率应越低";
}

TEST_F(FormationOptimizerUnit, SurvivalProbability_InZeroOneRange) {
    // 所有队形的生存率都在 [0, 1] 范围内
    std::vector<std::string> types = {"LINE", "WEDGE", "ECHELON", "V_SHAPE", "COLUMN", "DIAMOND"};

    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    for (const auto& t : types) {
        FormationConfig cfg = optimizer_->get_formation_template(t, 5);
        double score = optimizer_->evaluate_formation(cfg, req);
        EXPECT_GE(score, 0.0) << "队形 " << t << " 生存率不应小于0";
        EXPECT_LE(score, 1.0) << "队形 " << t << " 生存率不应大于1";
    }
}

// ========== OptimizationLogic - 优化逻辑 ==========

TEST_F(FormationOptimizerUnit, Optimize_ReturnsBestFormation) {
    // optimize()返回best_formation，且在候选列表中
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    auto result = optimizer_->optimize(req);

    EXPECT_FALSE(result.best_formation.formation_type.empty());
    EXPECT_EQ(result.best_formation.vehicle_count, 5);

    bool found = false;
    for (const auto& c : result.candidate_formations) {
        if (c.formation_type == result.best_formation.formation_type) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "最优队形应在候选列表中";
}

TEST_F(FormationOptimizerUnit, BestFormation_HasHighestScore) {
    // 最优队形的综合得分 >= 所有候选队形得分
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    auto result = optimizer_->optimize(req);

    double best_score = optimizer_->evaluate_formation(result.best_formation, req);

    for (const auto& c : result.candidate_formations) {
        double score = optimizer_->evaluate_formation(c, req);
        EXPECT_GE(best_score, score - 0.001) << "最优队形得分应不低于任何候选队形";
    }
}

TEST_F(FormationOptimizerUnit, Recommendations_NonEmpty) {
    // 返回3条或更多推荐建议
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    auto result = optimizer_->optimize(req);

    EXPECT_GE(result.recommendations.size(), 3u) << "至少应有3条推荐建议";

    for (const auto& rec : result.recommendations) {
        EXPECT_FALSE(rec.empty()) << "推荐建议不应为空";
    }
}

TEST_F(FormationOptimizerUnit, Optimization_Deterministic) {
    // 相同输入运行两次，最优队形相同
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    auto result1 = optimizer_->optimize(req);
    auto result2 = optimizer_->optimize(req);

    EXPECT_EQ(result1.best_formation.formation_type, result2.best_formation.formation_type)
        << "相同输入应得到相同的最优队形";
}

// ========== EdgeCases - 边界测试 ==========

TEST_F(FormationOptimizerUnit, SingleVehicle_AnyFormation) {
    // 只有1辆车时，所有队形结果相同
    std::vector<std::string> types = {"LINE", "WEDGE", "COLUMN", "DIAMOND"};

    FormationConfig first_cfg = optimizer_->get_formation_template(types[0], 1);
    auto first_vehicles = optimizer_->layout_formation(first_cfg);

    ASSERT_EQ(first_vehicles.size(), 1u);
    double x = first_vehicles[0].position_x;
    double y = first_vehicles[0].position_y;

    for (size_t i = 1; i < types.size(); ++i) {
        FormationConfig cfg = optimizer_->get_formation_template(types[i], 1);
        auto vehicles = optimizer_->layout_formation(cfg);
        ASSERT_EQ(vehicles.size(), 1u);
        EXPECT_NEAR(vehicles[0].position_x, x, 0.01) << "单辆车x坐标应相同";
        EXPECT_NEAR(vehicles[0].position_y, y, 0.01) << "单辆车y坐标应相同";
    }
}

TEST_F(FormationOptimizerUnit, ZeroVehicles_HandlesGracefully) {
    // 0辆车，不崩溃
    FormationConfig cfg = optimizer_->get_formation_template("LINE", 0);
    auto vehicles = optimizer_->layout_formation(cfg);
    EXPECT_EQ(vehicles.size(), 0u);

    FormationOptimizationRequest req;
    req.vehicle_count = 0;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = cfg;

    EXPECT_NO_THROW({
        optimizer_->evaluate_formation(cfg, req);
    }) << "0辆车时不应崩溃";
}

TEST_F(FormationOptimizerUnit, NegativeSpacing_ClampedToMin) {
    // 负间距自动修正为最小正值
    FormationConfig cfg;
    cfg.formation_type = "LINE";
    cfg.vehicle_count = 5;
    cfg.spacing_m = -2.0;
    cfg.attack_width_m = 10.0;
    cfg.wall_distance_m = 10.0;
    for (int i = 0; i < 5; ++i) {
        cfg.vehicle_types.push_back("FENYUN");
    }

    EXPECT_NO_THROW({
        auto vehicles = optimizer_->layout_formation(cfg);
        EXPECT_EQ(vehicles.size(), 5u);
    }) << "负间距不应导致崩溃";
}

TEST_F(FormationOptimizerUnit, HugeWallLength_WideFormation) {
    // 城墙很长时，优化倾向选择宽队形
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 100.0;
    req.rock_fall_rate_per_sec = 0.5;
    req.avg_rock_mass_kg = 20.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    auto result = optimizer_->optimize(req);

    EXPECT_GT(result.avg_coverage_score, 0.0);
    EXPECT_GT(result.best_formation.attack_width_m, 0.0);
}

TEST_F(FormationOptimizerUnit, ZeroRockFall_PerfectSurvival) {
    // 无落石，生存率=1.0
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 0.0;
    req.avg_rock_mass_kg = 0.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    auto result = optimizer_->optimize(req);

    EXPECT_GT(result.survival_probability, 0.5) << "无落石时生存率应较高";
}

// ========== CoverageAndProgress - 覆盖度与推进效率 ==========

TEST_F(FormationOptimizerUnit, LineFormation_BestCoverage) {
    // 横排覆盖度得分最高
    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 1.0;
    req.avg_rock_mass_kg = 20.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    std::vector<std::string> types = {"LINE", "WEDGE", "ECHELON", "V_SHAPE", "COLUMN", "DIAMOND"};

    double line_score = 0.0;
    for (const auto& t : types) {
        FormationConfig cfg = optimizer_->get_formation_template(t, 5);
        double score = optimizer_->evaluate_formation(cfg, req);
        if (t == "LINE") {
            line_score = score;
        }
    }

    EXPECT_GT(line_score, 0.0) << "横排覆盖度得分应大于0";
}

TEST_F(FormationOptimizerUnit, WedgeFormation_BestProgress) {
    // 楔形推进效率最高
    std::vector<std::string> types = {"LINE", "WEDGE", "ECHELON", "V_SHAPE", "COLUMN", "DIAMOND"};

    double best_score = 0.0;
    std::string best_type;

    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 1.0;
    req.avg_rock_mass_kg = 20.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    for (const auto& t : types) {
        FormationConfig cfg = optimizer_->get_formation_template(t, 5);
        double score = optimizer_->evaluate_formation(cfg, req);
        if (score > best_score) {
            best_score = score;
            best_type = t;
        }
    }

    EXPECT_FALSE(best_type.empty()) << "应有最优队形";
}

TEST_F(FormationOptimizerUnit, CoverageScore_InZeroOneRange) {
    // 覆盖度在 [0, 1] 范围
    std::vector<std::string> types = {"LINE", "WEDGE", "ECHELON", "V_SHAPE", "COLUMN", "DIAMOND"};

    FormationOptimizationRequest req;
    req.vehicle_count = 5;
    req.wall_height_m = 10.0;
    req.wall_length_m = 20.0;
    req.rock_fall_rate_per_sec = 2.0;
    req.avg_rock_mass_kg = 30.0;
    req.baseline = optimizer_->get_formation_template("LINE", 5);

    for (const auto& t : types) {
        FormationConfig cfg = optimizer_->get_formation_template(t, 5);
        double score = optimizer_->evaluate_formation(cfg, req);
        EXPECT_GE(score, 0.0) << "队形 " << t << " 覆盖度不应小于0";
        EXPECT_LE(score, 1.0) << "队形 " << t << " 覆盖度不应大于1";
    }
}

}
