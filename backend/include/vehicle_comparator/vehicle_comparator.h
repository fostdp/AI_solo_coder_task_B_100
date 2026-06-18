#pragma once

#include "common/data_types.h"
#include "impact_simulator/impact_simulator.h"
#include "config/config_loader.h"

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace fenyun {

class VehicleComparator {
public:
    explicit VehicleComparator(std::shared_ptr<ConfigLoader> config,
                               std::shared_ptr<ImpactSimulator> simulator);
    ~VehicleComparator();

    VehicleComparisonResult compare_vehicles(const VehicleComparisonRequest& req);

    VehicleComparisonResult compare_ancient_vehicles(double rock_mass_kg,
                                                      double rock_velocity_ms);

    VehicleComparisonResult compare_cross_era(double rock_mass_kg,
                                               double rock_velocity_ms);

    std::vector<VehicleProfile> list_all_vehicles() const;
    std::vector<VehicleProfile> list_ancient_vehicles() const;
    std::vector<VehicleProfile> list_modern_vehicles() const;

    uint64_t comparisons_run() const { return comparisons_run_.load(); }

private:
    double compute_protection_efficiency(const SimulationResult& sim,
                                          const VehicleProfile& vp) const;

    double compute_weight_normalized(const SimulationResult& sim,
                                      const VehicleProfile& vp) const;

    double compute_cost_normalized(const VehicleProfile& vp) const;

    std::vector<std::string> generate_insights(
        const std::vector<VehicleComparisonItem>& items) const;

    std::shared_ptr<ConfigLoader> config_;
    std::shared_ptr<ImpactSimulator> simulator_;

    std::atomic<uint64_t> comparisons_run_ {0};
    std::atomic<uint64_t> comparison_id_counter_ {0};
    mutable std::mutex mutex_;
};

}
