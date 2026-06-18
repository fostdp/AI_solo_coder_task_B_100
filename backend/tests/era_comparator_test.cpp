#include <gtest/gtest.h>
#include "era_comparator/era_comparator.h"
#include "config/config_loader.h"
#include "common/data_types.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace fenyun {

class EraComparatorUnit : public ::testing::Test {
protected:
    void SetUp() override {
        comparator_ = test::create_test_era_comparator();
        ASSERT_NE(comparator_, nullptr);
        config_ = test::create_test_config_loader();
        ASSERT_NE(config_, nullptr);
    }

    void TearDown() override {
        comparator_.reset();
        config_.reset();
    }

    std::shared_ptr<EraComparator> comparator_;
    std::shared_ptr<ConfigLoader> config_;
};

TEST_F(EraComparatorUnit, BasicFunctionality_ListAncientVehicles_ReturnsSixVehicles) {
    std::vector<VehicleProfile> vehicles = comparator_->list_ancient();

    EXPECT_GE(vehicles.size(), 1u) << "古代车辆列表不应为空";

    for (const auto& v : vehicles) {
        EXPECT_EQ(v.era, VehicleEra::ANCIENT) << "车辆 " << v.id << " 应该是古代车";
        EXPECT_FALSE(v.id.empty()) << "车辆 id 不能为空";
    }
}

TEST_F(EraComparatorUnit, BasicFunctionality_ListModernVehicles_ReturnsNonEmpty) {
    std::vector<VehicleProfile> vehicles = comparator_->list_modern();

    EXPECT_GE(vehicles.size(), 1u) << "现代车辆列表不应为空";

    for (const auto& v : vehicles) {
        EXPECT_EQ(v.era, VehicleEra::MODERN) << "车辆 " << v.id << " 应该是现代车";
        EXPECT_FALSE(v.id.empty()) << "车辆 id 不能为空";
    }
}

TEST_F(EraComparatorUnit, ComparisonLogic_CompareCrossEra_ProducesValidResult) {
    EraComparisonResult result = comparator_->compare_cross_era(5.0, 30.0);

    EXPECT_GT(result.comparison_id, 0u) << "comparison_id 应该大于 0";
    EXPECT_GT(result.timestamp_ms, 0) << "timestamp_ms 应该大于 0";

    EXPECT_FALSE(result.best_overall_vehicle_id.empty()) << "应该有最佳车辆";

    EXPECT_GT(result.ancient_summary.items.size(), 0u) << "古代对比结果不应为空";
    EXPECT_GT(result.modern_summary.items.size(), 0u) << "现代对比结果不应为空";
    EXPECT_GT(result.full_comparison.items.size(), 0u) << "完整对比结果不应为空";

    EXPECT_GT(result.gap_metrics.protection_factor, 0.0) << "防护系数应该大于 0";
    EXPECT_GT(result.gap_metrics.technology_gap_years, 0.0) << "技术差距年数应该大于 0";
}

TEST_F(EraComparatorUnit, ComparisonLogic_CompareCrossEra_ModernBetterThanAncient) {
    EraComparisonResult result = comparator_->compare_cross_era(5.0, 30.0);

    double ancient_avg = 0.0;
    for (const auto& item : result.ancient_summary.items) {
        ancient_avg += item.overall_score;
    }
    if (!result.ancient_summary.items.empty()) {
        ancient_avg /= result.ancient_summary.items.size();
    }

    double modern_avg = 0.0;
    for (const auto& item : result.modern_summary.items) {
        modern_avg += item.overall_score;
    }
    if (!result.modern_summary.items.empty()) {
        modern_avg /= result.modern_summary.items.size();
    }

    EXPECT_GT(modern_avg, ancient_avg) << "现代车辆平均分数应该高于古代车辆";
    EXPECT_GT(result.gap_metrics.protection_factor, 1.0) << "防护系数应该大于 1";
}

TEST_F(EraComparatorUnit, ComparisonLogic_CalculateEraGap_ValidMetrics) {
    VehicleComparisonResult ancient;
    VehicleComparisonResult modern;

    VehicleComparisonItem ancient_item;
    ancient_item.vehicle_id = "fenyun_basic";
    ancient_item.overall_score = 50.0;
    ancient_item.protection_efficiency_score = 40.0;
    ancient_item.cost_normalized_score = 60.0;
    ancient.items.push_back(ancient_item);

    VehicleComparisonItem modern_item;
    modern_item.vehicle_id = "m1126_stryker";
    modern_item.overall_score = 80.0;
    modern_item.protection_efficiency_score = 70.0;
    modern_item.cost_normalized_score = 50.0;
    modern.items.push_back(modern_item);

    EraGapMetrics gap = comparator_->calculate_era_gap(ancient, modern);

    EXPECT_GT(gap.protection_factor, 0.0) << "防护系数应该大于 0";
    EXPECT_GT(gap.speed_factor, 0.0) << "速度系数应该大于 0";
    EXPECT_GT(gap.technology_gap_years, 0.0) << "技术差距应该大于 0";
    EXPECT_FALSE(gap.summary_text.empty()) << "总结文本不应为空";
}

TEST_F(EraComparatorUnit, ComparisonLogic_CompareFull_ExtraVehiclesIncluded) {
    std::vector<std::string> extra_ids = {"fenyun_basic"};
    EraComparisonResult result = comparator_->compare_full(5.0, 30.0, extra_ids);

    EXPECT_GT(result.full_comparison.items.size(), 0u) << "完整对比结果不应为空";
    EXPECT_GT(result.comparison_id, 0u) << "comparison_id 应该有效";
}

TEST_F(EraComparatorUnit, Timeline_BuildProtectionTimeline_ReturnsValidEntries) {
    std::vector<EraTimelineEntry> timeline = comparator_->build_protection_timeline(5.0, 30.0);

    EXPECT_GT(timeline.size(), 0u) << "时间线不应为空";

    for (const auto& entry : timeline) {
        EXPECT_FALSE(entry.vehicle_id.empty()) << "车辆 id 不能为空";
        EXPECT_FALSE(entry.display_name.empty()) << "显示名称不能为空";
        EXPECT_GE(entry.protection_score, 0.0) << "防护分数不应为负";
    }

    bool has_ancient = false;
    bool has_modern = false;
    for (const auto& entry : timeline) {
        if (entry.era == "ancient") has_ancient = true;
        if (entry.era == "modern") has_modern = true;
    }
    EXPECT_TRUE(has_ancient) << "时间线应该包含古代车辆";
    EXPECT_TRUE(has_modern) << "时间线应该包含现代车辆";
}

TEST_F(EraComparatorUnit, Insights_GenerateHistoricalInsights_NonEmpty) {
    EraComparisonResult result = comparator_->compare_cross_era(5.0, 30.0);

    EXPECT_GT(result.historical_insights.size(), 0u) << "历史洞察不应为空";

    for (const auto& insight : result.historical_insights) {
        EXPECT_FALSE(insight.empty()) << "每条历史洞察都不应为空字符串";
    }
}

TEST_F(EraComparatorUnit, Insights_GenerateTechnologyInsights_NonEmpty) {
    EraComparisonResult result = comparator_->compare_cross_era(5.0, 30.0);

    EXPECT_GT(result.technology_insights.size(), 0u) << "技术洞察不应为空";

    for (const auto& insight : result.technology_insights) {
        EXPECT_FALSE(insight.empty()) << "每条技术洞察都不应为空字符串";
    }
}

TEST_F(EraComparatorUnit, Insights_GenerateFunFacts_NonEmpty) {
    EraComparisonResult result = comparator_->compare_cross_era(5.0, 30.0);

    EXPECT_GT(result.fun_facts.size(), 0u) << "趣味事实不应为空";

    for (const auto& fact : result.fun_facts) {
        EXPECT_FALSE(fact.empty()) << "每条趣味事实都不应为空字符串";
    }
}

TEST_F(EraComparatorUnit, Stats_ComparisonsRun_IncrementsCorrectly) {
    uint64_t initial = comparator_->comparisons_run();

    comparator_->compare_cross_era(5.0, 30.0);
    uint64_t after_one = comparator_->comparisons_run();
    EXPECT_EQ(after_one, initial + 1) << "执行一次对比后计数应该加 1";

    comparator_->compare_cross_era(10.0, 50.0);
    uint64_t after_two = comparator_->comparisons_run();
    EXPECT_EQ(after_two, initial + 2) << "执行两次对比后计数应该加 2";
}

TEST_F(EraComparatorUnit, EdgeCases_ZeroRockMass_DoesNotCrash) {
    EXPECT_NO_FATAL_FAILURE({
        EraComparisonResult result = comparator_->compare_cross_era(0.0, 30.0);
        EXPECT_GT(result.comparison_id, 0u);
    });
}

TEST_F(EraComparatorUnit, EdgeCases_VeryHighVelocity_DoesNotCrash) {
    EXPECT_NO_FATAL_FAILURE({
        EraComparisonResult result = comparator_->compare_cross_era(100.0, 1000.0);
        EXPECT_GT(result.comparison_id, 0u);
    });
}

TEST_F(EraComparatorUnit, EdgeCases_VeryLowVelocity_DoesNotCrash) {
    EXPECT_NO_FATAL_FAILURE({
        EraComparisonResult result = comparator_->compare_cross_era(1.0, 1.0);
        EXPECT_GT(result.comparison_id, 0u);
    });
}

TEST_F(EraComparatorUnit, GapMetrics_AllFieldsPopulated) {
    EraComparisonResult result = comparator_->compare_cross_era(5.0, 30.0);

    const auto& gap = result.gap_metrics;

    EXPECT_GE(gap.protection_factor, 0.0) << "protection_factor 应该有值";
    EXPECT_GE(gap.weight_efficiency_factor, 0.0) << "weight_efficiency_factor 应该有值";
    EXPECT_GE(gap.cost_efficiency_factor, 0.0) << "cost_efficiency_factor 应该有值";
    EXPECT_GE(gap.speed_factor, 0.0) << "speed_factor 应该有值";
    EXPECT_GT(gap.technology_gap_years, 0.0) << "technology_gap_years 应该有值";
    EXPECT_GE(gap.rha_thickness_ratio, 0.0) << "rha_thickness_ratio 应该有值";
    EXPECT_FALSE(gap.summary_text.empty()) << "summary_text 应该有值";
}

}
