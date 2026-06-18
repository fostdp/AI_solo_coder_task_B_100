#include "vehicle_comparator/vehicle_comparator.h"

#include <cmath>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace fenyun {

VehicleComparator::VehicleComparator(std::shared_ptr<ConfigLoader> config,
                                     std::shared_ptr<ImpactSimulator> simulator)
    : config_(std::move(config)), simulator_(std::move(simulator)) {
}

VehicleComparator::~VehicleComparator() = default;

double VehicleComparator::compute_protection_efficiency(const SimulationResult& sim,
                                                         const VehicleProfile& vp) const {
    double mass_kg = vp.weight_ton * 1000.0;
    double thickness_m = vp.roof_thickness_mm / 1000.0;
    double denominator = mass_kg * thickness_m;
    if (denominator < 1e-9) return 0.0;

    double raw = sim.absorbed_energy_j / denominator;

    double normalized = raw / 5000.0;
    normalized = std::max(0.0, std::min(1.0, normalized));
    return normalized * 100.0;
}

double VehicleComparator::compute_weight_normalized(const SimulationResult& sim,
                                                     const VehicleProfile& vp) const {
    double eff = compute_protection_efficiency(sim, vp);
    double weight_ton = std::max(vp.weight_ton, 0.1);

    double raw = eff / weight_ton;

    double normalized = raw / 50.0;
    normalized = std::max(0.0, std::min(1.0, normalized));
    return normalized * 100.0;
}

double VehicleComparator::compute_cost_normalized(const VehicleProfile& vp) const {
    if (!config_) return 50.0;

    MaterialProperties mat = config_->get_material(vp.primary_material);
    double cost = std::max(mat.cost_per_unit, 0.1);

    double raw = 1.0 / cost;

    double normalized = raw * 5.0;
    normalized = std::max(0.0, std::min(1.0, normalized));
    return normalized * 100.0;
}

std::vector<std::string> VehicleComparator::generate_insights(
    const std::vector<VehicleComparisonItem>& items) const {
    std::vector<std::string> insights;

    if (items.size() < 2) return insights;

    const auto& best = items.front();
    const auto& worst = items.back();

    std::ostringstream oss;
    oss << "综合排名第一: " << best.display_name
        << " (总体评分: " << std::fixed << std::setprecision(1) << best.overall_score << ")";
    insights.push_back(oss.str());

    oss.str("");
    oss << "综合排名末位: " << worst.display_name
        << " (总体评分: " << std::fixed << std::setprecision(1) << worst.overall_score << ")";
    insights.push_back(oss.str());

    if (worst.protection_efficiency_score > 1e-6) {
        double ratio = best.protection_efficiency_score / worst.protection_efficiency_score;
        oss.str("");
        oss << best.display_name << "防护效率是" << worst.display_name
            << "的" << std::fixed << std::setprecision(1) << ratio << "倍";
        insights.push_back(oss.str());
    }

    bool has_ancient = false, has_modern = false;
    const VehicleComparisonItem* best_ancient = nullptr;
    const VehicleComparisonItem* best_modern = nullptr;

    for (const auto& item : items) {
        if (item.era == VehicleEra::ANCIENT) {
            has_ancient = true;
            if (!best_ancient || item.overall_score > best_ancient->overall_score) {
                best_ancient = &item;
            }
        } else {
            has_modern = true;
            if (!best_modern || item.overall_score > best_modern->overall_score) {
                best_modern = &item;
            }
        }
    }

    if (has_ancient && has_modern && best_ancient && best_modern) {
        if (best_ancient->protection_efficiency_score > 1e-6) {
            double ratio = best_modern->protection_efficiency_score / best_ancient->protection_efficiency_score;
            oss.str("");
            oss << "跨时代对比: " << best_modern->display_name << "防护效率是"
                << best_ancient->display_name << "的"
                << std::fixed << std::setprecision(1) << ratio << "倍";
            insights.push_back(oss.str());
        }
    }

    if (items.size() >= 3) {
        double avg_eff = 0.0;
        for (const auto& item : items) avg_eff += item.protection_efficiency_score;
        avg_eff /= items.size();

        for (const auto& item : items) {
            if (item.protection_efficiency_score > avg_eff * 1.5) {
                oss.str("");
                oss << item.display_name << "防护效率远超平均水平("
                    << std::fixed << std::setprecision(1) << item.protection_efficiency_score
                    << " vs 平均 " << std::fixed << std::setprecision(1) << avg_eff << ")";
                insights.push_back(oss.str());
                break;
            }
        }
    }

    return insights;
}

VehicleComparisonResult VehicleComparator::compare_vehicles(const VehicleComparisonRequest& req) {
    std::lock_guard<std::mutex> lock(mutex_);

    VehicleComparisonResult result{};
    result.comparison_id = comparison_id_counter_.fetch_add(1) + 1;
    result.timestamp_ms = current_timestamp_ms();
    result.request = req;

    if (!config_ || !simulator_) {
        return result;
    }

    std::vector<VehicleComparisonItem> items;

    for (const auto& vid : req.vehicle_ids) {
        if (!config_->has_vehicle(vid)) continue;

        VehicleProfile vp = config_->get_vehicle(vid);

        SensorData data{};
        data.vehicle_id = 0;
        data.timestamp_ms = result.timestamp_ms;
        data.protection_thickness = vp.roof_thickness_mm;
        data.protection_material = vp.primary_material;
        data.ambient_temp = (req.temperature_K > 0) ? (req.temperature_K - 273.15) : 20.0;
        data.impact_location_x = req.impact_location_x;
        data.impact_location_y = req.impact_location_y;
        data.rock_mass = std::max(req.rock_mass_kg, 1.0);
        data.rock_velocity = std::max(req.rock_velocity_ms, 1.0);

        SimulationResult sim = simulator_->run_simulation(data);

        VehicleComparisonItem item{};
        item.vehicle_id = vp.id;
        item.display_name = vp.display_name;
        item.era = vp.era;
        item.simulation = sim;
        item.protection_efficiency_score = compute_protection_efficiency(sim, vp);
        item.weight_normalized_score = compute_weight_normalized(sim, vp);
        item.cost_normalized_score = compute_cost_normalized(vp);
        item.overall_score = 0.5 * item.protection_efficiency_score
                            + 0.3 * item.weight_normalized_score
                            + 0.2 * item.cost_normalized_score;

        items.push_back(std::move(item));
    }

    std::sort(items.begin(), items.end(),
              [](const VehicleComparisonItem& a, const VehicleComparisonItem& b) {
                  return a.overall_score > b.overall_score;
              });

    for (size_t i = 0; i < items.size(); ++i) {
        items[i].rank = static_cast<int>(i + 1);
    }

    result.items = std::move(items);

    if (!result.items.empty()) {
        result.best_vehicle_id = result.items.front().vehicle_id;
    }

    result.insights = generate_insights(result.items);

    comparisons_run_.fetch_add(1);

    return result;
}

VehicleComparisonResult VehicleComparator::compare_ancient_vehicles(double rock_mass_kg,
                                                                     double rock_velocity_ms) {
    VehicleComparisonRequest req{};
    req.rock_mass_kg = rock_mass_kg;
    req.rock_velocity_ms = rock_velocity_ms;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;
    req.use_johnson_cook = true;

    if (config_) {
        auto ancient = config_->get_vehicles_by_era(VehicleEra::ANCIENT);
        for (const auto& vp : ancient) {
            req.vehicle_ids.push_back(vp.id);
        }
    }

    return compare_vehicles(req);
}

VehicleComparisonResult VehicleComparator::compare_cross_era(double rock_mass_kg,
                                                              double rock_velocity_ms) {
    VehicleComparisonRequest req{};
    req.rock_mass_kg = rock_mass_kg;
    req.rock_velocity_ms = rock_velocity_ms;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;
    req.use_johnson_cook = true;

    if (config_) {
        auto all_vehicles = list_all_vehicles();
        for (const auto& vp : all_vehicles) {
            req.vehicle_ids.push_back(vp.id);
        }
    }

    return compare_vehicles(req);
}

std::vector<VehicleProfile> VehicleComparator::list_all_vehicles() const {
    std::vector<VehicleProfile> result;
    if (!config_) return result;

    const auto& veh_map = config_->vehicles();
    result.reserve(veh_map.size());
    for (const auto& [id, vp] : veh_map) {
        result.push_back(vp);
    }

    std::sort(result.begin(), result.end(),
              [](const VehicleProfile& a, const VehicleProfile& b) {
                  return a.historical_year < b.historical_year;
              });

    return result;
}

std::vector<VehicleProfile> VehicleComparator::list_ancient_vehicles() const {
    if (!config_) return {};
    return config_->get_vehicles_by_era(VehicleEra::ANCIENT);
}

std::vector<VehicleProfile> VehicleComparator::list_modern_vehicles() const {
    if (!config_) return {};
    return config_->get_vehicles_by_era(VehicleEra::MODERN);
}

}
