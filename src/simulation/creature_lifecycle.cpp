#include "ecosystem.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "ecosystem_geometry.hpp"
namespace terrarium {
using namespace ecosystem_detail;
void world_state::metabolize(creature_state& c) {
    c.age += day_step;
    if (c.energy > 0) {
        const double respiration = c.matter.carbon * -std::expm1(-.1 * day_step);
        const double excretion = c.matter.nitrogen * -std::expm1(-.025 * day_step);
        c.matter.carbon -= respiration;
        c.matter.nitrogen -= excretion;
        matter_.respired_carbon += respiration;
        soil_[soil_index(c.position)].nutrients += excretion / mineral_capacity();
    }
}
void world_state::incubate(creature_state& c) {

    c.incubation = std::max(0.f, c.incubation - day_step);
    if (c.incubation == 0) {
        c.age = 0;
        ++report_.hatched;
        const auto* a = ancestor(c.id);
        if (a && (a->favourite || (ancestor(a->mother) && ancestor(a->mother)->favourite)))
            record(name(c.id) + " hatched and began looking for food.");
    }
}
void world_state::age_creature(creature_state& c, float temp) {
    c.reproduction_cooldown -= day_step;
    c.meal_cooldown = std::max(0.0f, c.meal_cooldown - day_step);
    float stress = std::max(0.0f, std::abs(temp - 22) - 8 * c.genes.tolerance) * .026f;
    float hunger = c.species == species_kind::ladybird ? .18f
                   : c.species == species_kind::snail  ? .15f
                                                       : .26f;

    float moving_cost =
        distance_squared(c.position, c.target) > .001f ? .018f * c.genes.speed * c.genes.speed : 0;
    float resilience_cost = .025f * std::max(0.f, c.genes.tolerance - 1);
    float fertility_cost = .018f * std::max(0.f, c.genes.fertility - 1);
    const float body_fraction = .45f + .55f * std::clamp(c.age / maturity_age(c.species), 0.f, 1.f);
    c.energy -= (hunger * c.genes.size + stress + moving_cost + resilience_cost + fertility_cost) *
                body_fraction * day_step;
    if (c.energy <= 0 && c.death_cause == death_cause::unknown)
        c.death_cause = stress > hunger * c.genes.size ? (temp < 22 ? death_cause::cold : death_cause::heat)
                                                       : death_cause::starvation;
    float lifespan = c.species == species_kind::snail ? 65 : c.species == species_kind::ladybird ? 35 : 19;
    if (c.energy > 0 && c.age > lifespan * c.genes.tolerance) {
        c.energy = -1;
        c.death_cause = death_cause::old_age;
    }
}
void world_state::move_creature(creature_state& c) {
    const bool escaping_pond = in_pond(c.position);
    if (escaping_pond) {
        c.target = dry_ground(c.position);
        c.activity = creature_activity::wandering;
        c.attention = 0;
    }
    float dx = c.target.x - c.position.x, dy = c.target.y - c.position.y, d = std::hypot(dx, dy);
    if (d > .025f && c.activity != creature_activity::resting && c.activity != creature_activity::grazing) {
        float desired = std::atan2(dy, dx), difference = std::remainder(desired - c.heading, 2 * pi);
        c.heading = std::remainder(c.heading + std::clamp(difference, -.12f, .12f), 2 * pi);
        float speed = (c.species == species_kind::snail      ? .026f
                       : c.species == species_kind::ladybird ? .18f
                                                             : .11f) *
                      c.genes.speed;
        if (c.activity == creature_activity::fleeing)
            speed *= 1.6f;
        float move = std::min(d, speed * static_cast<float>(fixed_step));
        vec2 candidate = bounded({c.position.x + dx / d * move, c.position.y + dy / d * move});
        if (in_pond(candidate) && !escaping_pond) {
            float a = std::atan2(c.position.y - pond_center.y, c.position.x - pond_center.x) + .16f;
            c.target = bounded({pond_center.x + std::cos(a) * (pond_radii.x + .2f),
                                pond_center.y + std::sin(a) * (pond_radii.y + .2f)});
        } else
            c.position = candidate;
    }
}
void world_state::remove_dead_creatures() {
    for (const auto& c : creatures_)
        if (c.energy <= 0) {
            auto i = std::lower_bound(family_.begin(), family_.end(), c.id,
                                      [](const ancestor_record& a, entity_id id) { return a.id < id; });
            if (i != family_.end()) {
                i->died = day();
                i->death_cause = c.death_cause;
                i->predator = c.predator;
                if (i->favourite)
                    record(name(c.id) + ": " + death_reason(c.death_cause));
            }
            if (c.death_cause == death_cause::predation)
                ++report_.hunted;
            else if (c.death_cause == death_cause::starvation)
                ++report_.starved;
            else if (c.death_cause == death_cause::old_age)
                ++report_.old_age;
            else
                ++report_.weather_deaths;
            deposit(c.position, c.matter);
        }
    std::erase_if(creatures_, [](const creature_state& c) { return c.energy <= 0; });
}
void world_state::sample_history() {
    history_.push_back({day(), populations(), static_cast<int>(plants_.size())});
    if (history_.size() > 240)
        history_.pop_front();
    matter_history_.push_back(matter_sample());
    if (matter_history_.size() > 240)
        matter_history_.pop_front();
}
void world_state::report_day() {
    int thirsty = 0, growing = 0;
    for (const auto& p : plants_) {
        auto health = plant_health(p);
        thirsty += health.state == plant_condition::thirsty;
        growing += health.growth > health.loss;
    }
    record(std::format("{} plants gaining leaves; {} need water. {} seeds scattered. Fallen leaves "
                       "released {:.1f} mg of nitrogen for roots.",
                       growing, thirsty, report_.seedlings, report_.mineralized_nitrogen));
    if (report_.born || report_.hatched || report_.hunted || report_.starved || report_.weather_deaths ||
        report_.old_age)
        record(std::format("{} new lives, {} eggs hatched. {} aphids caught; {} deaths from hunger, {} "
                           "from temperature stress, {} from age.",
                           report_.born, report_.hatched, report_.hunted, report_.starved,
                           report_.weather_deaths, report_.old_age));
    report_ = {};
}
void world_state::step() {
    ++tick_;
    microclimate();
    if (tick_ % 15 == 0)
        behavior();
    const float temperature_now = temperature();
    for (auto& creature : creatures_) {
        metabolize(creature);
        if (creature.incubation > 0) {
            incubate(creature);
            continue;
        }
        age_creature(creature, temperature_now);
        if (creature.energy > 0)
            move_creature(creature);
    }
    if (tick_ % 30 == 0)
        ecology();
    remove_dead_creatures();
    if (tick_ % 300 == 0)
        sample_history();
    if (tick_ % ticks_per_day == 0)
        report_day();
    if (tick_ % (ticks_per_day * 12) == 0 && climate.seasons)
        record(std::string(season()) + " arrives in the garden.");
}
}
