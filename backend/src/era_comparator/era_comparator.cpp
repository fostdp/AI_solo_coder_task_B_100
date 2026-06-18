#include "era_comparator/era_comparator.h"

#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace fenyun {

EraComparator::EraComparator(std::shared_ptr<ConfigLoader> config,
                             std::shared_ptr<VehicleComparator> base_comparator)
    : config_(std::move(config)), base_comparator_(std::move(base_comparator)) {
}

EraComparator::~EraComparator() = default;

double EraComparator::average_score(const VehicleComparisonResult& result) const {
    if (result.items.empty()) return 0.0;

    double sum = 0.0;
    for (const auto& item : result.items) {
        sum += item.overall_score;
    }
    return sum / static_cast<double>(result.items.size());
}

double EraComparator::max_score(const VehicleComparisonResult& result) const {
    if (result.items.empty()) return 0.0;

    double max_val = 0.0;
    for (const auto& item : result.items) {
        if (item.overall_score > max_val) {
            max_val = item.overall_score;
        }
    }
    return max_val;
}

std::string EraComparator::best_vehicle_id(const VehicleComparisonResult& result) const {
    if (result.items.empty()) return "";
    return result.items.front().vehicle_id;
}

EraGapMetrics EraComparator::calculate_era_gap(const VehicleComparisonResult& ancient,
                                                const VehicleComparisonResult& modern) const {
    EraGapMetrics metrics{};

    double ancient_avg = average_score(ancient);
    double modern_avg = average_score(modern);

    if (ancient_avg > 1e-6) {
        metrics.protection_factor = modern_avg / ancient_avg;
    } else {
        metrics.protection_factor = 0.0;
    }

    double ancient_weight_sum = 0.0;
    double ancient_score_sum = 0.0;
    for (const auto& item : ancient.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            ancient_weight_sum += vp.weight_ton;
            ancient_score_sum += item.protection_efficiency_score;
        }
    }

    double modern_weight_sum = 0.0;
    double modern_score_sum = 0.0;
    for (const auto& item : modern.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            modern_weight_sum += vp.weight_ton;
            modern_score_sum += item.protection_efficiency_score;
        }
    }

    double ancient_weight_eff = (ancient_weight_sum > 1e-6) ? (ancient_score_sum / ancient_weight_sum) : 0.0;
    double modern_weight_eff = (modern_weight_sum > 1e-6) ? (modern_score_sum / modern_weight_sum) : 0.0;

    if (ancient_weight_eff > 1e-6) {
        metrics.weight_efficiency_factor = modern_weight_eff / ancient_weight_eff;
    } else {
        metrics.weight_efficiency_factor = 0.0;
    }

    double ancient_cost_sum = 0.0;
    for (const auto& item : ancient.items) {
        ancient_cost_sum += item.cost_normalized_score;
    }

    double modern_cost_sum = 0.0;
    for (const auto& item : modern.items) {
        modern_cost_sum += item.cost_normalized_score;
    }

    if (modern_cost_sum > 1e-6 && !ancient.items.empty() && !modern.items.empty()) {
        double ancient_avg_cost = ancient_cost_sum / static_cast<double>(ancient.items.size());
        double modern_avg_cost = modern_cost_sum / static_cast<double>(modern.items.size());
        if (modern_avg_cost > 1e-6) {
            metrics.cost_efficiency_factor = ancient_avg_cost / modern_avg_cost;
        } else {
            metrics.cost_efficiency_factor = 0.0;
        }
    } else {
        metrics.cost_efficiency_factor = 0.0;
    }

    double ancient_max_speed = 0.0;
    for (const auto& item : ancient.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            if (vp.max_speed_kmh > ancient_max_speed) {
                ancient_max_speed = vp.max_speed_kmh;
            }
        }
    }

    double modern_max_speed = 0.0;
    for (const auto& item : modern.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            if (vp.max_speed_kmh > modern_max_speed) {
                modern_max_speed = vp.max_speed_kmh;
            }
        }
    }

    if (ancient_max_speed > 1e-6) {
        metrics.speed_factor = modern_max_speed / ancient_max_speed;
    } else {
        metrics.speed_factor = 0.0;
    }

    int ancient_latest_year = -9999;
    for (const auto& item : ancient.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            if (vp.historical_year > ancient_latest_year) {
                ancient_latest_year = vp.historical_year;
            }
        }
    }

    int modern_earliest_year = 9999;
    for (const auto& item : modern.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            if (vp.historical_year < modern_earliest_year) {
                modern_earliest_year = vp.historical_year;
            }
        }
    }

    if (ancient_latest_year != -9999 && modern_earliest_year != 9999) {
        metrics.technology_gap_years = static_cast<double>(modern_earliest_year - ancient_latest_year);
    } else {
        metrics.technology_gap_years = 2500.0;
    }

    int modern_max_stanag = 0;
    for (const auto& item : modern.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            if (vp.stanag_equivalent_level > modern_max_stanag) {
                modern_max_stanag = vp.stanag_equivalent_level;
            }
        }
    }
    metrics.stanag_level_gap = modern_max_stanag - 0;

    double ancient_max_rha = 0.0;
    for (const auto& item : ancient.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            if (vp.rha_equivalent_mm > ancient_max_rha) {
                ancient_max_rha = vp.rha_equivalent_mm;
            }
            if (vp.roof_thickness_mm > ancient_max_rha) {
                ancient_max_rha = vp.roof_thickness_mm;
            }
        }
    }

    double modern_max_rha = 0.0;
    for (const auto& item : modern.items) {
        if (config_ && config_->has_vehicle(item.vehicle_id)) {
            VehicleProfile vp = config_->get_vehicle(item.vehicle_id);
            if (vp.rha_equivalent_mm > modern_max_rha) {
                modern_max_rha = vp.rha_equivalent_mm;
            }
        }
    }

    if (ancient_max_rha > 1e-6) {
        metrics.rha_thickness_ratio = modern_max_rha / ancient_max_rha;
    } else {
        metrics.rha_thickness_ratio = 0.0;
    }

    std::ostringstream oss;
    oss << "跨越约" << std::fixed << std::setprecision(0) << metrics.technology_gap_years
        << "年的技术进步，现代M1A2坦克防护效率是古代轒辒车的 "
        << std::fixed << std::setprecision(1) << metrics.protection_factor << " 倍";
    metrics.summary_text = oss.str();

    return metrics;
}

std::vector<std::string> EraComparator::generate_historical_insights(
    const EraGapMetrics& gap,
    const VehicleComparisonResult& ancient,
    const VehicleComparisonResult& modern) const {
    std::vector<std::string> insights;

    std::ostringstream oss;

    if (!ancient.items.empty() && !modern.items.empty()) {
        oss << "从春秋轒辒车到M1A2坦克，防护能力跃升了 "
            << std::fixed << std::setprecision(1) << gap.protection_factor
            << " 倍，耗时约 " << std::fixed << std::setprecision(0)
            << gap.technology_gap_years << " 年";
        insights.push_back(oss.str());
        oss.str("");
    }

    bool has_m113 = false;
    double m113_score = 0.0;
    double best_ancient_score = 0.0;
    std::string best_ancient_name;

    for (const auto& item : modern.items) {
        if (item.vehicle_id == "modern_m113") {
            has_m113 = true;
            m113_score = item.overall_score;
        }
    }

    if (!ancient.items.empty()) {
        best_ancient_score = ancient.items.front().overall_score;
        best_ancient_name = ancient.items.front().display_name;
    }

    if (has_m113 && best_ancient_score > 1e-6) {
        oss << "即使是基础型M113装甲车，防护也超过所有古代攻城车";
        insights.push_back(oss.str());
        oss.str("");
    }

    if (gap.technology_gap_years > 0) {
        oss << "两千五百年的军事工程演进，从木牛流马到钢铁巨兽";
        insights.push_back(oss.str());
        oss.str("");
    }

    if (!ancient.items.empty() && !modern.items.empty()) {
        oss << "古代攻城武器依靠人力推进，现代装甲车辆具备自主机动能力";
        insights.push_back(oss.str());
        oss.str("");
    }

    if (ancient.items.size() >= 2) {
        oss << "古代攻城车从春秋到唐宋，防护水平提升有限，主要依赖材料工艺改进";
        insights.push_back(oss.str());
        oss.str("");
    }

    return insights;
}

std::vector<std::string> EraComparator::generate_technology_insights(
    const EraGapMetrics& gap) const {
    std::vector<std::string> insights;

    std::ostringstream oss;

    oss << "材料代差：从木/牛皮→铸铁→钢→铝合金→复合装甲→贫铀";
    insights.push_back(oss.str());
    oss.str("");

    oss << "STANAG等级从0级跃升至" << gap.stanag_level_gap << "级";
    insights.push_back(oss.str());
    oss.str("");

    if (gap.rha_thickness_ratio > 1e-6) {
        oss << "等效装甲厚度提升了 " << std::fixed << std::setprecision(1)
            << gap.rha_thickness_ratio << " 倍";
        insights.push_back(oss.str());
        oss.str("");
    }

    if (gap.weight_efficiency_factor > 1e-6) {
        oss << "重量效率提升了 " << std::fixed << std::setprecision(1)
            << gap.weight_efficiency_factor << " 倍";
        insights.push_back(oss.str());
        oss.str("");
    }

    if (gap.speed_factor > 1e-6) {
        oss << "机动速度提升了 " << std::fixed << std::setprecision(1)
            << gap.speed_factor << " 倍";
        insights.push_back(oss.str());
        oss.str("");
    }

    return insights;
}

std::vector<std::string> EraComparator::generate_fun_facts(const EraGapMetrics& gap) const {
    std::vector<std::string> facts;

    std::ostringstream oss;

    if (config_) {
        if (config_->has_vehicle("modern_m1a2") && config_->has_vehicle("fenyun_basic")) {
            VehicleProfile m1a2 = config_->get_vehicle("modern_m1a2");
            VehicleProfile fenyun = config_->get_vehicle("fenyun_basic");
            if (fenyun.weight_ton > 1e-6) {
                double ratio = m1a2.weight_ton / fenyun.weight_ton;
                oss << "M1A2坦克的重量相当于约 " << std::fixed << std::setprecision(0)
                    << ratio << " 辆春秋轒辒车";
                facts.push_back(oss.str());
                oss.str("");
            }
        }

        if (config_->has_vehicle("modern_m113") && config_->has_vehicle("yunti_basic")) {
            VehicleProfile m113 = config_->get_vehicle("modern_m113");
            VehicleProfile yunti = config_->get_vehicle("yunti_basic");
            if (yunti.max_speed_kmh > 1e-6) {
                double ratio = m113.max_speed_kmh / yunti.max_speed_kmh;
                oss << "M113装甲车的速度是古代云梯车的 "
                    << std::fixed << std::setprecision(1) << ratio << " 倍";
                facts.push_back(oss.str());
                oss.str("");
            }
        }

        if (config_->has_vehicle("modern_m1a2") && config_->has_vehicle("fenyun_basic")) {
            VehicleProfile m1a2 = config_->get_vehicle("modern_m1a2");
            VehicleProfile fenyun = config_->get_vehicle("fenyun_basic");
            if (fenyun.roof_thickness_mm > 1e-6) {
                double ratio = m1a2.wall_thickness_mm / fenyun.roof_thickness_mm;
                oss << "最厚坦克装甲" << std::fixed << std::setprecision(0)
                    << m1a2.wall_thickness_mm << "mm，是轒辒车车顶"
                    << std::fixed << std::setprecision(0) << fenyun.roof_thickness_mm
                    << "mm的 " << std::fixed << std::setprecision(1) << ratio << " 倍";
                facts.push_back(oss.str());
                oss.str("");
            }
        }
    }

    if (gap.technology_gap_years > 0) {
        oss << "人类用了两千五百年才把攻城车变成坦克";
        facts.push_back(oss.str());
        oss.str("");
    }

    return facts;
}

std::vector<VehicleProfile> EraComparator::list_ancient() const {
    if (!base_comparator_) return {};
    return base_comparator_->list_ancient_vehicles();
}

std::vector<VehicleProfile> EraComparator::list_modern() const {
    if (!base_comparator_) return {};
    return base_comparator_->list_modern_vehicles();
}

EraComparisonResult EraComparator::compare_cross_era(double rock_mass_kg,
                                                      double rock_velocity_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    EraComparisonResult result{};
    result.comparison_id = era_comparison_id_counter_.fetch_add(1) + 1;
    result.timestamp_ms = current_timestamp_ms();

    if (!base_comparator_ || !config_) {
        return result;
    }

    result.full_comparison = base_comparator_->compare_cross_era(rock_mass_kg, rock_velocity_ms);

    VehicleComparisonResult ancient_result = base_comparator_->compare_ancient_vehicles(
        rock_mass_kg, rock_velocity_ms);

    VehicleComparisonRequest modern_req{};
    modern_req.rock_mass_kg = rock_mass_kg;
    modern_req.rock_velocity_ms = rock_velocity_ms;
    modern_req.impact_location_x = 3.25;
    modern_req.impact_location_y = 1.4;
    modern_req.temperature_K = 293.15;
    modern_req.use_johnson_cook = true;

    auto modern_vehicles = base_comparator_->list_modern_vehicles();
    for (const auto& vp : modern_vehicles) {
        modern_req.vehicle_ids.push_back(vp.id);
    }

    VehicleComparisonResult modern_result = base_comparator_->compare_vehicles(modern_req);

    result.ancient_summary = ancient_result;
    result.modern_summary = modern_result;

    result.gap_metrics = calculate_era_gap(ancient_result, modern_result);

    result.historical_insights = generate_historical_insights(
        result.gap_metrics, ancient_result, modern_result);

    result.technology_insights = generate_technology_insights(result.gap_metrics);

    result.fun_facts = generate_fun_facts(result.gap_metrics);

    if (!result.full_comparison.items.empty()) {
        result.best_overall_vehicle_id = result.full_comparison.items.front().vehicle_id;
    }

    result.request = result.full_comparison.request;

    era_comparisons_run_.fetch_add(1);

    return result;
}

EraComparisonResult EraComparator::compare_full(double rock_mass_kg,
                                                 double rock_velocity_ms,
                                                 const std::vector<std::string>& extra_ids) {
    std::lock_guard<std::mutex> lock(mutex_);

    EraComparisonResult result{};
    result.comparison_id = era_comparison_id_counter_.fetch_add(1) + 1;
    result.timestamp_ms = current_timestamp_ms();

    if (!base_comparator_ || !config_) {
        return result;
    }

    VehicleComparisonRequest req{};
    req.rock_mass_kg = rock_mass_kg;
    req.rock_velocity_ms = rock_velocity_ms;
    req.impact_location_x = 3.25;
    req.impact_location_y = 1.4;
    req.temperature_K = 293.15;
    req.use_johnson_cook = true;

    auto all_vehicles = base_comparator_->list_all_vehicles();
    for (const auto& vp : all_vehicles) {
        req.vehicle_ids.push_back(vp.id);
    }

    for (const auto& id : extra_ids) {
        if (config_->has_vehicle(id)) {
            bool already_exists = false;
            for (const auto& existing_id : req.vehicle_ids) {
                if (existing_id == id) {
                    already_exists = true;
                    break;
                }
            }
            if (!already_exists) {
                req.vehicle_ids.push_back(id);
            }
        }
    }

    result.full_comparison = base_comparator_->compare_vehicles(req);

    std::vector<VehicleComparisonItem> ancient_items;
    std::vector<VehicleComparisonItem> modern_items;

    for (const auto& item : result.full_comparison.items) {
        if (item.era == VehicleEra::ANCIENT) {
            ancient_items.push_back(item);
        } else {
            modern_items.push_back(item);
        }
    }

    VehicleComparisonResult ancient_result{};
    ancient_result.comparison_id = result.comparison_id;
    ancient_result.timestamp_ms = result.timestamp_ms;
    ancient_result.request = req;
    ancient_result.items = ancient_items;
    if (!ancient_items.empty()) {
        ancient_result.best_vehicle_id = ancient_items.front().vehicle_id;
    }

    VehicleComparisonResult modern_result{};
    modern_result.comparison_id = result.comparison_id;
    modern_result.timestamp_ms = result.timestamp_ms;
    modern_result.request = req;
    modern_result.items = modern_items;
    if (!modern_items.empty()) {
        modern_result.best_vehicle_id = modern_items.front().vehicle_id;
    }

    result.ancient_summary = ancient_result;
    result.modern_summary = modern_result;

    result.gap_metrics = calculate_era_gap(ancient_result, modern_result);

    result.historical_insights = generate_historical_insights(
        result.gap_metrics, ancient_result, modern_result);

    result.technology_insights = generate_technology_insights(result.gap_metrics);

    result.fun_facts = generate_fun_facts(result.gap_metrics);

    if (!result.full_comparison.items.empty()) {
        result.best_overall_vehicle_id = result.full_comparison.items.front().vehicle_id;
    }

    result.request = req;

    era_comparisons_run_.fetch_add(1);

    return result;
}

std::vector<EraTimelineEntry> EraComparator::build_protection_timeline(
    double rock_mass_kg, double rock_velocity_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<EraTimelineEntry> timeline;

    if (!config_ || !base_comparator_) {
        return timeline;
    }

    auto all_vehicles = base_comparator_->list_all_vehicles();

    std::sort(all_vehicles.begin(), all_vehicles.end(),
              [](const VehicleProfile& a, const VehicleProfile& b) {
                  return a.historical_year < b.historical_year;
              });

    std::map<int, std::string> milestones = {
        {-440, "春秋时期-轒辒车诞生(牛皮蒙顶)"},
        {-280, "战国-铁叶护甲出现"},
        {1044, "唐宋-云梯制式化"},
        {1960, "1960s-铝合金APC问世"},
        {1980, "1980s-复合装甲革命"},
        {1992, "1990s-贫铀装甲列装"}
    };

    for (const auto& vp : all_vehicles) {
        VehicleComparisonRequest req{};
        req.vehicle_ids.push_back(vp.id);
        req.rock_mass_kg = rock_mass_kg;
        req.rock_velocity_ms = rock_velocity_ms;
        req.impact_location_x = 3.25;
        req.impact_location_y = 1.4;
        req.temperature_K = 293.15;
        req.use_johnson_cook = true;

        VehicleComparisonResult comp_result = base_comparator_->compare_vehicles(req);

        EraTimelineEntry entry{};
        entry.year = vp.historical_year;
        entry.vehicle_id = vp.id;
        entry.display_name = vp.display_name;
        entry.era = (vp.era == VehicleEra::ANCIENT) ? "ancient" : "modern";

        if (!comp_result.items.empty()) {
            entry.protection_score = comp_result.items.front().protection_efficiency_score;
        } else {
            entry.protection_score = 0.0;
        }

        int closest_year = -9999;
        for (const auto& [year, text] : milestones) {
            if (year <= vp.historical_year && year > closest_year) {
                closest_year = year;
            }
        }

        if (closest_year != -9999) {
            auto it = milestones.find(closest_year);
            if (it != milestones.end()) {
                entry.milestone = it->second;
            }
        }

        timeline.push_back(std::move(entry));
    }

    return timeline;
}

}
