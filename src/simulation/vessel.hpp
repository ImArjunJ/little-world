#pragma once
#include <cmath>
namespace terrarium::physics {

struct vessel_parameters {
    double air_volume{.064};
    double ground_area{.264}, glass_area{.56};
    double body_capacity{50000}, glass_capacity{1800};
    double internal_conductance{4}, external_conductance{5};
    double surface_transfer{.002}, glass_transfer{.001};
    double absorption{.65}, emissivity{.9};
    double open_air_changes{12};
    double drainage_time{600};
};
struct vessel_state {
    double body_c{22}, glass_c{22}, film_kg{}, external_j{};
    double evaporated_kg{}, condensed_kg{}, air_in_kg{}, air_out_kg{};
};
struct vessel_forcing {
    double ambient_c{22}, ambient_humidity{.5}, irradiance{};
    double opening{.15}, wet_area{.264};
};
struct water_flux {
    double surface_kg{}, drip_kg{}, air_in_kg{}, air_out_kg{};
};
inline constexpr double latent_heat = 2450000;
double saturation_pressure(double celsius);
double saturation_density(double celsius);
double relative_humidity(const vessel_state&, double vapour_kg, const vessel_parameters& = {});
double stored_energy(const vessel_state&, double vapour_kg, const vessel_parameters& = {});
bool valid(const vessel_state&);

water_flux advance(vessel_state&, double& vapour_kg, double liquid_kg, double seconds, const vessel_forcing&,
                   const vessel_parameters& = {});
}
