#include "ecosystem.hpp"

#include <algorithm>
#include <cmath>

#include "ecosystem_geometry.hpp"
namespace terrarium {
using namespace ecosystem_detail;
class creature_behavior {
  public:
    explicit creature_behavior(world_state& world) : world(world) { index_creatures(); }
    void update() {
        for (auto& creature : world.creatures_)
            choose(creature);
    }

  private:
    static constexpr float period = 15 * day_step;
    static constexpr int width = 12;
    struct neighbourhood {
        const creature_state* mate{};
        creature_state* prey{};
        const creature_state* danger{};
        float prey_distance{3.2f}, prey_shelter{};
        int neighbours{};
    };
    struct food_source {
        plant_state* plant{};
        float distance{4}, score{4};
    };
    world_state& world;
    std::array<int, width * width> heads;
    std::array<int, world_state::max_creatures> next{};
    std::array<float, world_state::max_creatures> shelter{};
    std::array<std::array<int, 3>, world_state::max_plants> food_crowding{};

  private:
    static int cell(vec2 p) {
        return std::clamp(static_cast<int>(p.y + 6), 0, 11) * 12 +
               std::clamp(static_cast<int>(p.x + 6), 0, 11);
    }
    void index_creatures() {

        heads.fill(-1);
        for (std::size_t i = 0; i < world.creatures_.size(); ++i) {
            int bin = cell(world.creatures_[i].position);
            next[i] = heads[bin];
            heads[bin] = static_cast<int>(i);
            if (world.creatures_[i].species == species_kind::aphid)
                shelter[i] = world.shelter_at(world.creatures_[i].position);
        }

        for (std::size_t p = 0; p < world.plants_.size(); ++p) {
            const int bin = cell(world.plants_[p].position), cx = bin % width, cy = bin / width;
            for (int y = std::max(0, cy - 1); y <= std::min(11, cy + 1); ++y)
                for (int x = std::max(0, cx - 1); x <= std::min(11, cx + 1); ++x)
                    for (int i = heads[y * width + x]; i >= 0; i = next[i])
                        if (world.creatures_[i].energy > 0 && world.creatures_[i].incubation <= 0 &&
                            distance_squared(world.creatures_[i].position, world.plants_[p].position) < .36f)
                            ++food_crowding[p][static_cast<int>(world.creatures_[i].species)];
        }
    }
    neighbourhood sense(const creature_state& c) {
        const int bin = cell(c.position), cx = bin % width, cy = bin / width;
        neighbourhood nearby;
        float mate_distance = 3.2f, danger_distance = .4f;
        for (int y = std::max(0, cy - 2); y <= std::min(11, cy + 2); ++y)
            for (int x = std::max(0, cx - 2); x <= std::min(11, cx + 2); ++x)
                for (int i = heads[y * width + x]; i >= 0; i = next[i]) {
                    auto& other = world.creatures_[i];
                    if (other.id == c.id || other.energy <= 0 || other.incubation > 0)
                        continue;
                    float d = distance_squared(c.position, other.position);
                    if (other.species == c.species && d < 1.44f)
                        ++nearby.neighbours;
                    if (c.species == species_kind::aphid && other.species == species_kind::ladybird &&
                        d < danger_distance) {
                        danger_distance = d;
                        nearby.danger = &other;
                    }
                    if (c.species == species_kind::ladybird && other.species == species_kind::aphid &&
                        d < nearby.prey_distance && d < 3.2f * (1 - shelter[i])) {
                        nearby.prey_shelter = shelter[i];
                        nearby.prey_distance = d;
                        nearby.prey = &other;
                    }
                    if (other.species == c.species && other.age >= maturity_age(c.species) &&
                        other.reproduction_cooldown <= 0 && other.energy > .65f && d < mate_distance) {
                        mate_distance = d;
                        nearby.mate = &other;
                    }
                }

        return nearby;
    }
    const creature_state* find_mate(const creature_state& c, const creature_state* mate) const {

        float range = 64;
        for (const auto& other : world.creatures_) {
            if (other.id == c.id || other.species != c.species || other.incubation > 0 ||
                other.age < maturity_age(c.species) || other.energy < .65f || other.reproduction_cooldown > 0)
                continue;
            float d = distance_squared(c.position, other.position);
            if (d < range) {
                range = d;
                mate = &other;
            }
        }
        return mate;
    }
    food_source find_food(const creature_state& c) {
        food_source source;
        for (std::size_t plant_index = 0; plant_index < world.plants_.size(); ++plant_index) {
            auto& p = world.plants_[plant_index];
            if (p.biomass < .16f)
                continue;
            if (c.species == species_kind::ladybird && (p.kind != plant_kind::flower || p.nectar_carbon <= 0))
                continue;
            if (c.species == species_kind::snail && p.kind == plant_kind::fern)
                continue;
            float d = distance_squared(c.position, p.position);
            int competitors = food_crowding[plant_index][static_cast<int>(c.species)];
            const float score =
                d + std::max(0, competitors - (c.species == species_kind::aphid ? 8 : 3)) * .35f;
            if (d < 4 && score < source.score) {
                source.score = score;
                source.distance = d;
                source.plant = &p;
            }
        }
        return source;
    }
    bool evade(creature_state& c, const creature_state* danger) {
        if (danger && c.energy > .2f) {
            float dx = c.position.x - danger->position.x, dy = c.position.y - danger->position.y,
                  d = std::max(.01f, std::hypot(dx, dy));
            c.target = bounded({c.position.x + dx / d * .4f, c.position.y + dy / d * .4f});
            c.attention = danger->id;
            c.activity = creature_activity::fleeing;
            return true;
        }
        return false;
    }
    bool rest(creature_state& c) {
        if (c.energy > 1.15f && c.activity_time <= 0) {
            c.activity = creature_activity::resting;
            c.activity_time = .1f + world.random(0, .12f);
        }
        if (c.activity == creature_activity::resting && c.activity_time > 0 && c.energy > .65f) {
            c.target = c.position;
            return true;
        }
        return false;
    }
    bool court(creature_state& c, bool seeking_mate, const creature_state* mate) {
        if (seeking_mate && mate) {
            c.attention = mate->id;
            c.target = mate->position;
            c.activity = creature_activity::courting;
            return true;
        }
        return false;
    }
    bool scavenge(creature_state& c) {
        if (c.species == species_kind::snail) {
            auto& soil = world.soil_[world.soil_index(c.position)];
            if (soil.water < .25) {
                float a = std::atan2(world_state::pond_center.y - c.position.y,
                                     world_state::pond_center.x - c.position.x);
                c.target = bounded({c.position.x + std::cos(a) * .6f, c.position.y + std::sin(a) * .6f});
                c.activity = creature_activity::drinking;
                return true;
            }
            if (soil.litter > .01 && c.energy < 1.25f) {
                double eaten = std::min(soil.litter, .13 * period);
                const double nitrogen = soil.litter_nitrogen * eaten / soil.litter;
                soil.litter -= eaten;
                soil.litter_nitrogen -= nitrogen;
                world.digest_food(c, {eaten * plant_carbon, nitrogen});
                c.meal = food_kind::litter;
                c.last_meal = world.day();
                c.energy = std::min(1.5f, c.energy + static_cast<float>(eaten * 4));
                c.activity = creature_activity::grazing;
                c.target = c.position;
                return true;
            }
        }
        return false;
    }
    bool hunt(creature_state& c, const neighbourhood& nearby, const food_source& source) {
        if (c.species == species_kind::ladybird && c.energy < .95f && nearby.prey && c.meal_cooldown <= 0 &&
            (!source.plant || nearby.prey_distance < source.score)) {
            c.attention = nearby.prey->id;
            c.target = nearby.prey->position;
            c.activity = creature_activity::hunting;
            if (nearby.prey_distance < .045f * (1 - .8f * nearby.prey_shelter)) {
                world.digest_food(c, nearby.prey->matter);
                nearby.prey->matter = {};
                nearby.prey->energy = -1;
                nearby.prey->death_cause = death_cause::predation;
                nearby.prey->predator = c.id;
                c.meal = food_kind::aphid;
                c.last_meal = world.day();
                c.protein = std::min(1.f, c.protein + .6f);
                c.energy = std::min(1.5f, c.energy + .65f);
                c.meal_cooldown = .7f;
                c.activity = creature_activity::resting;
                c.activity_time = .18f;
            }
            return true;
        }
        return false;
    }
    void forage(creature_state& c, const food_source& source) {
        if (c.activity == creature_activity::hunting) {
            c.activity = creature_activity::wandering;
            c.activity_time = 0;
        }
        if (source.plant && c.energy < 1.18f) {
            c.attention = source.plant->id;
            c.target = source.plant->position;
            c.activity = creature_activity::wandering;
            if (source.distance < .09f) {
                c.activity = creature_activity::grazing;
                c.last_meal = world.day();
                c.meal = c.species == species_kind::ladybird ? food_kind::nectar : food_kind::leaves;
                if (c.species == species_kind::ladybird) {
                    const double nectar = std::min(source.plant->nectar_carbon, double(period) * 2);
                    source.plant->nectar_carbon -= nectar;
                    world.digest_food(c, {nectar, 0});
                    c.energy = std::min(1.5f, c.energy + static_cast<float>(nectar * .15));
                } else {
                    constexpr float rate = .035f;
                    float eaten = std::min(source.plant->biomass - .12f, rate * period * c.genes.size);
                    const float before = source.plant->biomass;
                    source.plant->biomass -= eaten;
                    eaten = before - source.plant->biomass;
                    source.plant->grazing += eaten;
                    world.digest_food(c, {eaten * plant_carbon, eaten * plant_nitrogen});
                    c.energy = std::min(1.5f, c.energy + eaten * (c.species == species_kind::aphid ? 24 : 9));
                }
            }
        } else if (distance_squared(c.position, c.target) < .08f || world.random() < .07f) {
            float a = world.random(0, 2 * pi);
            c.target = bounded({c.position.x + std::cos(a) * .7f, c.position.y + std::sin(a) * .7f});
            c.activity = creature_activity::wandering;
        }
    }
    void choose(creature_state& c) {
        if (c.energy <= 0 || c.incubation > 0)
            return;
        c.attention = 0;
        c.activity_time = std::max(0.0f, c.activity_time - period);
        auto nearby = sense(c);
        c.energy -=
            period * std::max(0, nearby.neighbours - (c.species == species_kind::aphid ? 12 : 6)) * .014f;
        const float courtship_energy =
            reproduction_energy + (c.activity == creature_activity::courting ? .01f : .10f);
        const bool seeking_mate = c.age >= maturity_age(c.species) && c.energy >= courtship_energy &&
                                  c.reproduction_cooldown <= 0 &&
                                  (c.species != species_kind::ladybird || c.protein >= .4f);
        if (c.species == species_kind::ladybird && seeking_mate)
            nearby.mate = find_mate(c, nearby.mate);
        if (evade(c, nearby.danger) || rest(c) || court(c, seeking_mate, nearby.mate) || scavenge(c))
            return;
        const auto food = find_food(c);
        if (!hunt(c, nearby, food))
            forage(c, food);
    }
};
void world_state::behavior() {
    creature_behavior(*this).update();
}
}
