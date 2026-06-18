#pragma once

#include "common/data_types.h"
#include "vehicle_comparator/vehicle_comparator.h"
#include "config/config_loader.h"

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace fenyun {

struct EraGapMetrics {
    double protection_factor;
    double weight_efficiency_factor;
    double cost_efficiency_factor;
    double speed_factor;
    double technology_gap_years;
    int stanag_level_gap;
    double rha_thickness_ratio;
    std::string summary_text;
};

struct EraComparisonResult {
    uint64_t comparison_id;
    int64_t timestamp_ms;
    VehicleComparisonRequest request;
    VehicleComparisonResult ancient_summary;
    VehicleComparisonResult modern_summary;
    VehicleComparisonResult full_comparison;
    EraGapMetrics gap_metrics;
    std::vector<std::string> historical_insights;
    std::vector<std::string> technology_insights;
    std::vector<std::string> fun_facts;
    std::string best_overall_vehicle_id;
};

struct EraTimelineEntry {
    int year;
    std::string vehicle_id;
    std::string display_name;
    std::string era;
    double protection_score;
    std::string milestone;
};

class EraComparator {
public:
    explicit EraComparator(std::shared_ptr<ConfigLoader> config,
                           std::shared_ptr<VehicleComparator> base_comparator);
    ~EraComparator();

    EraComparisonResult compare_cross_era(double rock_mass_kg,
                                          double rock_velocity_ms);

    EraComparisonResult compare_full(double rock_mass_kg,
                                      double rock_velocity_ms,
                                      const std::vector<std::string>& extra_ids = {});

    EraGapMetrics calculate_era_gap(const VehicleComparisonResult& ancient,
                                     const VehicleComparisonResult& modern) const;

    std::vector<EraTimelineEntry> build_protection_timeline(
        double rock_mass_kg, double rock_velocity_ms);

    std::vector<std::string> generate_historical_insights(
        const EraGapMetrics& gap,
        const VehicleComparisonResult& ancient,
        const VehicleComparisonResult& modern) const;

    std::vector<std::string> generate_technology_insights(
        const EraGapMetrics& gap) const;

    std::vector<std::string> generate_fun_facts(
        const EraGapMetrics& gap) const;

    std::vector<VehicleProfile> list_ancient() const;
    std::vector<VehicleProfile> list_modern() const;

    uint64_t comparisons_run() const { return era_comparisons_run_.load(); }

private:
    double average_score(const VehicleComparisonResult& result) const;
    double max_score(const VehicleComparisonResult& result) const;
    std::string best_vehicle_id(const VehicleComparisonResult& result) const;

    std::shared_ptr<ConfigLoader> config_;
    std::shared_ptr<VehicleComparator> base_comparator_;

    std::atomic<uint64_t> era_comparisons_run_ {0};
    std::atomic<uint64_t> era_comparison_id_counter_ {0};
    mutable std::mutex mutex_;
};

}
