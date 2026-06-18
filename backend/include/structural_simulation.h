#pragma once

#include "common.h"
#include <cmath>
#include <map>

namespace fenyun {

class StructuralSimulation {
public:
    StructuralSimulation();

    SimulationResult run_simulation(const SensorData& sensor_data);

    void set_roof_dimensions(double length_m, double width_m, double thickness_m);

    void set_deformation_threshold(double mm);
    void set_penetration_threshold(double ratio);

private:
    double calc_impact_energy(double mass_kg, double velocity_m_s) const;

    double calc_contact_force(double impact_energy_j, double contact_area_m2,
                              double impactor_radius_m) const;

    double calc_bending_deformation(double force_n, double length_m, double width_m,
                                     double thickness_m, double E_pa, double nu) const;

    double calc_plastic_strain(double von_mises_pa, double yield_pa, double E_pa) const;

    double calc_von_mises_stress(double force_n, double contact_area_m2,
                                  double bending_stress_pa) const;

    double calc_penetration_depth(double impact_energy_j, const MaterialProperties& mat,
                                   double thickness_m, double contact_area_m2) const;

    double calc_absorbed_energy(double impact_energy_j, const MaterialProperties& mat,
                                 double thickness_m, double area_m2) const;

    uint8_t determine_damage_level(double plastic_strain, double penetration_ratio,
                                    double von_mises_ratio) const;

    std::string determine_failure_mode(double bending_stress_pa, double shear_stress_pa,
                                        double contact_stress_pa, const MaterialProperties& mat) const;

    void generate_deformation_field(std::vector<double>& field, double max_deformation_mm,
                                     double impact_x, double impact_y) const;

    void generate_stress_field(std::vector<double>& field, double max_stress_mpa,
                                double impact_x, double impact_y) const;

    MaterialProperties get_material_properties(const std::string& material_name) const;

    std::map<std::string, MaterialProperties> material_db_;

    double roof_length_m_ = 6.0;
    double roof_width_m_ = 2.5;
    double roof_thickness_m_ = 0.08;
    double deformation_threshold_mm_ = 15.0;
    double penetration_threshold_ratio_ = 0.9;

    static constexpr int GRID_SIZE = 10;
};

}
