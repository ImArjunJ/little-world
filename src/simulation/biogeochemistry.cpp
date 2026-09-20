#include "ecosystem.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>
namespace terrarium {
double world_state::mineral_capacity() const {

    return design_.form == vessel_form::legacy ? 10 : 10 * design_.area() * design_.soil_depth / .024;
}
matter_amount world_state::body_matter(const creature_state& creature) {
    const double carbon = (creature.species == species_kind::aphid   ? .45
                           : creature.species == species_kind::snail ? 50
                                                                     : 5) *
                          creature.genes.size;
    return {carbon, carbon / 5};
}
matter_reading world_state::matter_sample() const {
    matter_reading result;
    result.day = day();
    const double capacity = mineral_capacity();
    for (const auto& soil : soil_) {
        result.nitrogen[0] += soil.nutrients * capacity;
        result.nitrogen[3] += soil.litter_nitrogen;
        result.nitrogen[4] += soil.microbial_nitrogen;
        result.carbon += soil.litter * plant_carbon + soil.microbial_nitrogen * microbial_cn;
    }
    for (const auto& plant : plants_) {
        result.nitrogen[1] += plant.biomass * plant_nitrogen;
        result.carbon += plant.biomass * plant_carbon + plant.nectar_carbon;
    }
    for (const auto& creature : creatures_) {
        result.nitrogen[2] += creature.matter.nitrogen;
        result.carbon += creature.matter.carbon;
    }
    return result;
}
matter_amount world_state::matter_balance() const {
    const auto sample = matter_sample();
    return {sample.carbon + matter_.respired_carbon - matter_.fixed_carbon - matter_.introduced.carbon -
                matter_.initial.carbon,
            std::accumulate(sample.nitrogen.begin(), sample.nitrogen.end(), 0.) -
                matter_.introduced.nitrogen - matter_.initial.nitrogen};
}
void world_state::initialize_matter() {
    for (auto& soil : soil_) {
        soil.litter_nitrogen = soil.litter * 15;
        soil.microbial_nitrogen = .0625;
    }
    for (auto& creature : creatures_)
        creature.matter = body_matter(creature);
    for (auto& plant : plants_)
        plant.nectar_carbon = 0;
    matter_ = {};
    const auto sample = matter_sample();
    matter_.initial = {sample.carbon, std::accumulate(sample.nitrogen.begin(), sample.nitrogen.end(), 0.)};
    matter_history_.clear();
    matter_history_.push_back(sample);
}
double world_state::root_nutrients(vec2 point) const {
    const int center = soil_index(point), cx = center % soil_width, cy = center / soil_width;
    double sum = 0, count = 0;
    for (int y = std::max(0, cy - 1); y <= std::min(soil_width - 1, cy + 1); ++y)
        for (int x = std::max(0, cx - 1); x <= std::min(soil_width - 1, cx + 1); ++x) {
            sum += soil_[y * soil_width + x].nutrients;
            ++count;
        }
    return std::clamp(sum / count, 0., 1.);
}
double world_state::root_nitrogen(vec2 point, double take) {
    const int center = soil_index(point), cx = center % soil_width, cy = center / soil_width;
    const double capacity = mineral_capacity();
    double available = 0;
    for (int y = std::max(0, cy - 1); y <= std::min(soil_width - 1, cy + 1); ++y)
        for (int x = std::max(0, cx - 1); x <= std::min(soil_width - 1, cx + 1); ++x)
            available += soil_[y * soil_width + x].nutrients * capacity;
    if (take <= 0 || available <= 0)
        return available;
    const double fraction = std::min(1., take / available);
    for (int y = std::max(0, cy - 1); y <= std::min(soil_width - 1, cy + 1); ++y)
        for (int x = std::max(0, cx - 1); x <= std::min(soil_width - 1, cx + 1); ++x)
            soil_[y * soil_width + x].nutrients *= 1 - fraction;
    return available * fraction;
}
void world_state::deposit(vec2 point, matter_amount matter) {
    auto& soil = soil_[soil_index(point)];
    soil.litter += matter.carbon / plant_carbon;
    soil.litter_nitrogen += matter.nitrogen;
}
void world_state::digest_food(creature_state& creature, matter_amount meal) {
    constexpr double assimilation = .35;
    deposit(creature.position, {meal.carbon * (1 - assimilation), meal.nitrogen * (1 - assimilation)});
    const auto body = body_matter(creature);
    const double carbon =
        std::min(meal.carbon * assimilation, std::max(0., body.carbon * 2 - creature.matter.carbon));
    const double nitrogen =
        std::min(meal.nitrogen * assimilation, std::max(0., body.nitrogen * 2 - creature.matter.nitrogen));
    creature.matter.carbon += carbon;
    creature.matter.nitrogen += nitrogen;
    matter_.respired_carbon += meal.carbon * assimilation - carbon;
    soil_[soil_index(creature.position)].nutrients +=
        (meal.nitrogen * assimilation - nitrogen) / mineral_capacity();
}
void world_state::decompose(double dt) {
    const double capacity = mineral_capacity();
    const double warmth = std::clamp(1. - std::abs(temperature() - 23) / 30, .05, 1.);
    for (auto& soil : soil_) {
        const double wetness = std::min(1., soil.water * 2);
        const double activity = warmth * wetness;

        const double turnover = soil.microbial_nitrogen * -std::expm1(-.08 * activity * dt);
        soil.microbial_nitrogen -= turnover;
        soil.nutrients += turnover / capacity;
        matter_.respired_carbon += turnover * microbial_cn;
        report_.mineralized_nitrogen += turnover;
        if (soil.litter <= 0)
            continue;
        const double population = std::clamp(soil.microbial_nitrogen * microbial_cn / .5, 0., 2.);
        const double fraction = -std::expm1(-.13 * activity * population * dt);
        const double decomposed = soil.litter * fraction;
        const double carbon = decomposed * plant_carbon, nitrogen = soil.litter_nitrogen * fraction;
        const double growth_n = std::min(carbon * .35 / microbial_cn, nitrogen + soil.nutrients * capacity);
        soil.litter -= decomposed;
        soil.litter_nitrogen -= nitrogen;
        soil.microbial_nitrogen += growth_n;
        soil.nutrients = std::max(0., soil.nutrients + (nitrogen - growth_n) / capacity);
        matter_.respired_carbon += carbon - growth_n * microbial_cn;
        report_.compost += decomposed;
        report_.mineralized_nitrogen += std::max(0., nitrogen - growth_n);
        report_.immobilized_nitrogen += std::max(0., growth_n - nitrogen);
    }
}
bool world_state::amend(vec2 point, amendment kind, double grams) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
        point.x * point.x + point.y * point.y > radius * radius || in_pond(point) || !std::isfinite(grams) ||
        grams <= 0 || grams > 5 || (kind != amendment::compost && kind != amendment::wood))
        return false;
    const matter_amount added{grams * plant_carbon, grams * (kind == amendment::compost ? 35 : 3)};
    deposit(point, added);
    matter_.introduced.carbon += added.carbon;
    matter_.introduced.nitrogen += added.nitrogen;
    record(kind == amendment::compost
               ? "A pinch of rich compost. The soil life will work it into food for roots."
               : "Woody mulch settles on the soil. Its decomposers need nitrogen as well as carbon.");
    return true;
}
}
