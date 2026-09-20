#include "construction.hpp"
#include "ecosystem.hpp"
#include <algorithm>
#include <numbers>
namespace terrarium {
bool garden_design::valid() const {
    return int(form) >= 0 && int(form) <= 3 && int(mix) >= 0 && int(mix) <= 2 && std::isfinite(soil_depth) &&
           soil_depth >= .04 && soil_depth <= .1 && std::isfinite(drainage_depth) && drainage_depth >= .01 &&
           drainage_depth <= .05 && soil_depth + drainage_depth <= height() - .07;
}
double garden_design::radius() const {
    return form == vessel_form::pocket ? .18 : form == vessel_form::tall ? .22 : .29;
}
double garden_design::height() const {
    return form == vessel_form::pocket ? .24 : form == vessel_form::tall ? .38 : .25;
}
double garden_design::area() const {
    return std::numbers::pi * radius() * radius();
}
double garden_design::porosity() const {
    return mix == soil_mix::forest ? .62 : mix == soil_mix::loam ? .48 : .42;
}
double garden_design::field_capacity() const {
    return mix == soil_mix::forest ? .68 : mix == soil_mix::loam ? .78 : .35;
}
double garden_design::drainage_rate() const {
    return mix == soil_mix::forest ? 2. : mix == soil_mix::loam ? .4 : 8.;
}
double garden_design::soil_water_capacity() const {
    return form == vessel_form::legacy ? 14.4 : area() * soil_depth * porosity() * 1000;
}
double garden_design::reservoir_capacity() const {
    return form == vessel_form::legacy ? 8. : area() * drainage_depth * .4 * 1000;
}
physics::vessel_parameters garden_design::parameters() const {
    if (form == vessel_form::legacy)
        return {};
    physics::vessel_parameters p;
    p.ground_area = area();
    p.air_volume = area() * (height() - soil_depth - drainage_depth);
    p.glass_area = 2 * std::numbers::pi * radius() * height() + area();

    const double soil_density = mix == soil_mix::forest ? 400 : mix == soil_mix::loam ? 1200 : 1600;
    const double soil_kg = area() * soil_depth * soil_density;
    const double stone_kg = area() * drainage_depth * .6 * 2650;
    const double initial_water_kg = soil_water_capacity() * .64 + reservoir_capacity() * .2;
    p.body_capacity = soil_kg * 1000 + stone_kg * 800 + initial_water_kg * 4180;
    p.glass_capacity = p.glass_area * .003 * 2500 * 840;
    p.internal_conductance = p.glass_area * (4 / .56);
    p.external_conductance = p.glass_area * (5 / .56);
    return p;
}
const char* vessel_name(vessel_form form) {
    switch (form) {
    case vessel_form::pocket:
        return "Pocket jar";
    case vessel_form::bowl:
        return "Broad bowl";
    case vessel_form::tall:
        return "Tall jar";
    default:
        return "Original garden";
    }
}
const char* soil_name(soil_mix mix) {
    return mix == soil_mix::forest ? "Forest blend" : mix == soil_mix::loam ? "Loam" : "Sandy soil";
}
void plant_starter(world_state& world, starter starter) {
    if (starter == starter::empty)
        return;
    for (int i = 0; i < 18; ++i) {
        float angle = i * 2.4f, radius = 1 + (i % 4) * .85f;
        vec2 point{std::cos(angle) * radius, std::sin(angle) * radius};
        plant_kind kind = starter == starter::woodland ? (i % 3 ? plant_kind::fern : plant_kind::clover)
                                                       : (i % 3 ? plant_kind::flower : plant_kind::clover);
        world.add_plant(kind, point);
    }
    world.add_creature(species_kind::snail, {-2, -2});
    world.add_creature(species_kind::snail, {-2.1f, -2});
    for (int i = 0; i < 10 && !world.plants().empty(); ++i)
        world.add_creature(species_kind::aphid, world.plants()[i % world.plants().size()].position);
}
}
