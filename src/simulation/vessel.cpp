#include "vessel.hpp"
#include <algorithm>
#include <stdexcept>
namespace terrarium::physics {
namespace {
constexpr double sigma = 5.670374419e-8, vapour_gas_constant = 461.5;
bool between(double x, double a, double b) {
    return std::isfinite(x) && x >= a && x <= b;
}
void heat(vessel_state& s, double dt, const vessel_forcing& f, const vessel_parameters& p) {
    const double solar = f.irradiance * p.ground_area * p.absorption * dt;
    s.body_c += solar / p.body_capacity;
    s.external_j += solar;
    const double capacity = 1 / (1 / p.body_capacity + 1 / p.glass_capacity);
    const double exchange =
        (s.body_c - s.glass_c) * capacity * -std::expm1(-p.internal_conductance * dt / capacity);
    s.body_c -= exchange / p.body_capacity;
    s.glass_c += exchange / p.glass_capacity;
    const double glass_k = s.glass_c + 273.15, ambient_k = f.ambient_c + 273.15;
    const double radiation = sigma * p.emissivity * p.glass_area * (glass_k + ambient_k) *
                             (glass_k * glass_k + ambient_k * ambient_k);
    const double glass_j = (f.ambient_c - s.glass_c) * p.glass_capacity *
                           -std::expm1(-(p.external_conductance + radiation) * dt / p.glass_capacity);
    s.glass_c += glass_j / p.glass_capacity;

    const double ventilation = f.opening * (4 + p.open_air_changes * p.air_volume * 1.2 * 1013 / 3600);
    const double air_j =
        (f.ambient_c - s.body_c) * p.body_capacity * -std::expm1(-ventilation * dt / p.body_capacity);
    s.body_c += air_j / p.body_capacity;
    s.external_j += glass_j + air_j;
}
}
double saturation_pressure(double celsius) {
    if (!between(celsius, -50, 100))
        throw std::invalid_argument("Vapour temperature outside model range");
    return 610.8 * std::exp(17.27 * celsius / (celsius + 237.3));
}
double saturation_density(double celsius) {
    return saturation_pressure(celsius) / (vapour_gas_constant * (celsius + 273.15));
}
double relative_humidity(const vessel_state& s, double vapour, const vessel_parameters& p) {
    return vapour / (p.air_volume * saturation_density(s.body_c));
}
double stored_energy(const vessel_state& s, double vapour, const vessel_parameters& p) {
    return p.body_capacity * s.body_c + p.glass_capacity * s.glass_c + latent_heat * vapour;
}
bool valid(const vessel_state& s) {
    if (!between(s.body_c, -50, 100) || !between(s.glass_c, -50, 100) || !between(s.film_kg, 0, 100) ||
        !between(s.external_j, -1e18, 1e18))
        return false;
    for (double mass : {s.evaporated_kg, s.condensed_kg, s.air_in_kg, s.air_out_kg})
        if (!between(mass, 0, 1e12))
            return false;
    return true;
}
water_flux advance(vessel_state& state, double& vapour, double liquid, double seconds,
                   const vessel_forcing& f, const vessel_parameters& p) {
    if (!valid(state) || !between(vapour, 0, .1) || !between(liquid, 0, 1000) ||
        !between(seconds, 0, 86400) || !between(f.ambient_c, -20, 50) || !between(f.ambient_humidity, 0, 1) ||
        !between(f.opening, 0, 1) || !between(f.irradiance, 0, 1400) || !between(f.wet_area, 0, 10) ||
        !between(p.air_volume, .001, 1) || !between(p.body_capacity, 1000, 1e7) ||
        !between(p.glass_capacity, 100, 1e6) || !between(p.ground_area, .001, 10) ||
        !between(p.glass_area, .001, 10) || !between(p.internal_conductance, 0, 100) ||
        !between(p.external_conductance, 0, 100) || !between(p.surface_transfer, 0, .01) ||
        !between(p.glass_transfer, 0, .01) || !between(p.absorption, 0, 1) || !between(p.emissivity, 0, 1) ||
        !between(p.open_air_changes, 0, 100) || !between(p.drainage_time, 1, 86400))
        throw std::invalid_argument("Invalid vessel physics input");
    water_flux total;

    auto s = state;
    double v = vapour;
    while (seconds > 0) {
        double dt = std::min(48., seconds);
        seconds -= dt;
        heat(s, dt * .5, f, p);
        const double body_density = saturation_density(s.body_c),
                     glass_density = saturation_density(s.glass_c);
        const double ambient_density = saturation_density(f.ambient_c) * f.ambient_humidity;
        const double initial = v / p.air_volume;
        const double soil = (liquid > 0 || initial > body_density) ? p.surface_transfer * f.wet_area : 0;
        const double glass = (s.film_kg > 0 || initial > glass_density) ? p.glass_transfer * p.glass_area : 0;
        const double air = f.opening * p.open_air_changes * p.air_volume / 3600;
        const double conductance = soil + glass + air;
        double surface_mass = 0, glass_mass = 0, air_mass = 0;
        if (conductance > 0) {
            const double equilibrium =
                (soil * body_density + glass * glass_density + air * ambient_density) / conductance;
            const double rate = conductance / p.air_volume;
            const double integral =
                equilibrium * dt + (initial - equilibrium) * -std::expm1(-rate * dt) / rate;
            surface_mass = std::min(liquid, soil * (body_density * dt - integral));
            glass_mass = std::min(s.film_kg, glass * (glass_density * dt - integral));
            air_mass = air * (ambient_density * dt - integral);
            const double incoming =
                std::max(0., surface_mass) + std::max(0., glass_mass) + std::max(0., air_mass);
            const double outgoing =
                -std::min(0., surface_mass) - std::min(0., glass_mass) - std::min(0., air_mass);
            if (outgoing > v + incoming) {
                const double fraction = (v + incoming) / outgoing;
                if (surface_mass < 0)
                    surface_mass *= fraction;
                if (glass_mass < 0)
                    glass_mass *= fraction;
                if (air_mass < 0)
                    air_mass *= fraction;
            }
        }
        v = std::max(0., v + surface_mass + glass_mass + air_mass);
        liquid -= surface_mass;
        s.film_kg -= glass_mass;
        s.body_c -= surface_mass * latent_heat / p.body_capacity;
        s.glass_c -= glass_mass * latent_heat / p.glass_capacity;
        s.external_j += air_mass * latent_heat;
        s.evaporated_kg += std::max(0., surface_mass) + std::max(0., glass_mass);
        s.condensed_kg -= std::min(0., surface_mass) + std::min(0., glass_mass);
        const double drip = s.film_kg * -std::expm1(-dt / p.drainage_time);
        s.film_kg -= drip;
        liquid += drip;
        const double air_in = std::max(0., air_mass), air_out = -std::min(0., air_mass);
        s.air_in_kg += air_in;
        s.air_out_kg += air_out;
        total.surface_kg += surface_mass;
        total.drip_kg += drip;
        total.air_in_kg += air_in;
        total.air_out_kg += air_out;
        heat(s, dt * .5, f, p);
        if (!valid(s))
            throw std::runtime_error("Vessel physics left its supported temperature domain");
    }
    state = s;
    vapour = v;
    return total;
}
}
