#include "ecosystem.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
namespace terrarium {
float maturity_age(species_kind species) {
    return species == species_kind::snail ? 5.f : species == species_kind::ladybird ? 5.f : .8f;
}
const char* death_reason(death_cause cause) {
    return std::array{
        "The cause was not recorded.",         "Ran out of food energy.",
        "Was caught by a ladybird.",           "Cold stress exhausted their energy.",
        "Heat stress exhausted their energy.", "Reached the end of their lifespan."}[static_cast<int>(cause)];
}
const char* plant_state_name(plant_condition state) {
    return std::array{"Making new leaves",      "A seed taking root", "Thirsty roots",
                      "Not enough sunlight",    "Too exposed",        "Too cold to flourish",
                      "Struggling in the heat", "Hungry soil",        "Leaves being eaten",
                      "A full, healthy plant",  "Losing leaves",      "Resting after sunset"}
        [static_cast<int>(state)];
}
const char* plant_advice(plant_condition state) {
    return std::array{
        "Water, light and soil food are supporting new growth.",
        "Keep the soil damp. A small shoot will appear as this seed germinates.",
        "Water these roots or invite a passing shower.",
        "Move the sun to open a brighter gap in the canopy.",
        "This plant prefers gentler light. Try the shade of a taller neighbour.",
        "A warmer room will help. The soil and glass need time to warm up.",
        "Try shelf shade or open the vents. A cooler room also lets the stored heat escape.",
        "Fallen leaves need time and damp soil to release nutrients.",
        "Grazing is outpacing new leaves. More plants can spread the pressure; ladybirds hunt aphids.",
        "There is little room for more leaves. Healthy mature plants can scatter seeds.",
        "Leaves are fading faster than they grow. Check light, warmth and soil food to help this plant "
        "recover.",
        "Photosynthesis pauses in the dark. Watch new growth when the daylight returns."}[static_cast<int>(
        state)];
}
float world_state::growth_scale(const plant_state& plant) {
    return .12f + .95f * std::sqrt(std::max(0.f, plant.biomass));
}
float world_state::plant_height(const plant_state& plant) {
    return growth_scale(plant) * (plant.kind == plant_kind::fern     ? 1.65f
                                  : plant.kind == plant_kind::clover ? .43f
                                                                     : 1.25f);
}
const plant_state* world_state::plant(entity_id id) const {
    auto it = std::find_if(plants_.begin(), plants_.end(), [id](const plant_state& p) { return p.id == id; });
    return it == plants_.end() ? nullptr : &*it;
}
float world_state::light_at(vec2 point, entity_id exclude) const {

    const float dx = std::cos(climate.sun_angle) * .85f / 1.5f;
    const float dy = std::sin(climate.sun_angle) * .85f / 1.5f;
    float transmission = 1;
    for (const auto& p : plants_) {
        if (p.id == exclude || (p.parent && p.age < .35f))
            continue;
        float height = plant_height(p) * .65f;
        vec2 center{p.position.x - dx * height, p.position.y - dy * height};
        float radius = (.28f + (p.kind == plant_kind::fern ? .62f : .28f) * p.biomass);
        float d2 = distance_squared(center, point), r2 = radius * radius;
        if (d2 < r2)
            transmission *= 1 - .70f * (1 - d2 / r2);
    }
    return light() * (.16f + .84f * transmission);
}
float world_state::shelter_at(vec2 point) const {
    float cover = 0;
    for (const auto& plant : plants_) {
        if (plant.parent && plant.age < .35f)
            continue;
        const float radius = plant.kind == plant_kind::fern ? .65f : .35f;
        const float distance = distance_squared(point, plant.position);
        if (distance < radius * radius)
            cover += plant.biomass * (1 - distance / (radius * radius));
    }
    return std::min(.95f, cover);
}
plant_status world_state::plant_health(const plant_state& p) const {
    const auto& soil = soil_at(p.position);
    plant_status h{light_at(p.position, p.id), static_cast<float>(soil.water),
                   static_cast<float>(root_nutrients(p.position))};
    float preferred_light = p.kind == plant_kind::fern ? .43f : p.kind == plant_kind::clover ? .62f : .77f;
    float preferred_water = p.kind == plant_kind::fern ? .52f : p.kind == plant_kind::clover ? .38f : .32f;
    float wetness = std::clamp(h.moisture / preferred_water, 0.f, 1.f);
    float sunshine = std::clamp(h.light / preferred_light, 0.f, 1.15f);
    if (p.kind == plant_kind::fern)
        sunshine *= 1 - std::max(0.f, h.light - .70f) * 1.4f;
    float comfort = std::clamp(1 - std::abs(temperature() - 22) / 23, 0.f, 1.f);

    const float growth_rate = .34f * (climate.day_night ? std::numbers::pi_v<float> : 1.f);
    h.growth = growth_rate * sunshine * comfort * wetness * h.nutrients * (1 - p.biomass / 1.6f);
    h.loss = .018f + std::max(0.f, .22f - h.moisture) * .5f + (1 - comfort) * .045f;
    if (p.parent && p.age < .35f)
        h.state = plant_condition::seed;
    else if (h.moisture < .24f)
        h.state = plant_condition::thirsty;
    else if (temperature() < 10)
        h.state = plant_condition::cold;
    else if (temperature() > 33)
        h.state = plant_condition::hot;
    else if (h.nutrients < .18f)
        h.state = plant_condition::poor_soil;
    else if (climate.day_night && daylight() < .01f)
        h.state = plant_condition::night;
    else if (h.light < preferred_light * .48f)
        h.state = plant_condition::shade;
    else if (p.kind == plant_kind::fern && h.light > .8f)
        h.state = plant_condition::scorched;
    else if (p.grazing > .03f && p.grazing * 2 > std::max(.01f, h.growth - h.loss))
        h.state = plant_condition::grazed;
    else if (h.growth < h.loss)
        h.state = plant_condition::struggling;
    else if (p.biomass > 1.25f)
        h.state = plant_condition::mature;
    return h;
}
int world_state::eggs() const {
    return std::count_if(creatures_.begin(), creatures_.end(),
                         [](const creature_state& c) { return c.incubation > 0; });
}
std::string world_state::intention(const creature_state& c) const {
    if (c.incubation > 0)
        return std::format("An egg resting here. About {:.1f} garden days until hatching.", c.incubation);
    if (c.activity == creature_activity::fleeing)
        return "A nearby ladybird was spotted. Moving away from danger.";
    if (c.activity == creature_activity::hunting)
        return "Hungry, and following " + name(c.attention) + " the aphid.";
    if (c.activity == creature_activity::courting)
        return "Well fed, and seeking " + name(c.attention) + " to start a family.";
    if (c.activity == creature_activity::drinking)
        return "The ground here is dry. Heading toward the damp pond edge.";
    if (c.activity == creature_activity::resting)
        return "Resting after a meal, saving energy for later.";
    if (auto* p = plant(c.attention))
        return std::string(c.activity == creature_activity::grazing ? "Feeding on " : "Heading toward ") +
               plant_name(p->kind) +
               (c.species == species_kind::ladybird ? " nectar. Aphids are still needed to breed."
                                                    : " leaves to restore energy.");
    if (c.activity == creature_activity::grazing && c.meal == food_kind::litter)
        return "Eating fallen leaves. Some of that material returns to the soil.";
    if (c.energy < .6f)
        return "Low on energy. Searching for suitable food nearby.";
    return "Exploring between meals. Nearby food, damp soil and other creatures guide the next move.";
}
std::string world_state::last_meal(const creature_state& c) const {
    if (c.last_meal < 0)
        return "No meal recorded yet.";
    constexpr std::array foods{"food", "leaves", "flower nectar", "fallen leaves", "an aphid"};
    return std::format("Last meal: {}, {:.1f} days ago.", foods[static_cast<int>(c.meal)],
                       std::max(0., day() - c.last_meal));
}
}
