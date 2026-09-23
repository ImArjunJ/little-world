#include "ecosystem.hpp"

#include <algorithm>
#include <cmath>

#include "ecosystem_geometry.hpp"
namespace terrarium {
using namespace ecosystem_detail;
float world_state::inherit_trait(float maternal, float paternal) {
    return std::clamp((maternal + paternal) * .5f + random(-.055f, .055f), .5f, 1.8f);
}
void world_state::grow_plant(plant_state& p, const plant_status& condition, float dt) {
    p.age += dt;
    p.seed_cooldown -= dt;
    p.grazing *= std::exp(-2 * dt);
    const double loss = std::min(double(p.biomass), double(condition.loss) * dt);
    const float before_loss = p.biomass;
    p.biomass = std::max(0.f, p.biomass - static_cast<float>(loss));
    const double shed = double(before_loss) - p.biomass;
    deposit(p.position, {shed * plant_carbon, shed * plant_nitrogen});
    const double available = root_nitrogen(p.position);
    const double growth =
        std::min({double(condition.growth) * dt, available / plant_nitrogen, double(1.6f) - p.biomass});
    float grown = static_cast<float>(p.biomass + std::max(0., growth));

    if (double(grown) - p.biomass > growth)
        grown = std::nextafter(grown, p.biomass);
    const double gained = double(grown) - p.biomass;
    root_nitrogen(p.position, gained * plant_nitrogen);
    p.biomass = grown;
    matter_.fixed_carbon += gained * plant_carbon;
    if (p.kind == plant_kind::flower) {
        const double nectar =
            std::min(std::max(0., 4. - p.nectar_carbon),
                     double(condition.light) * std::min(1.f, condition.moisture * 2) * dt * 8);
        p.nectar_carbon += nectar;
        matter_.fixed_carbon += nectar;
    }
}
void world_state::scatter_seed(plant_state& p) {
    if (p.biomass > 1.f && p.seed_cooldown <= 0 && plants_.size() < max_plants) {
        float a = random(0, 2 * pi);
        vec2 seed = bounded({p.position.x + std::cos(a) * .8f, p.position.y + std::sin(a) * .8f});
        bool room = !in_pond(seed);
        for (const auto& n : plants_)
            if (distance_squared(seed, n.position) < .27f) {
                room = false;
                break;
            }
        p.seed_cooldown = random(3, 6);
        if (room) {
            auto kind = p.kind;
            entity_id parent = p.id;
            const float before = p.biomass;
            p.biomass -= .09f;
            const float seed_mass = before - p.biomass;
            plants_.push_back({next_id_++, kind, seed, seed_mass, 0, random(2, 5),
                               static_cast<std::uint32_t>(random_()), 0, parent});
            ++report_.seedlings;
        }
    }
}
void world_state::remove_dead_plants() {
    for (const auto& p : plants_)
        if (p.biomass <= 0)
            deposit(p.position, {p.nectar_carbon, 0});
    std::erase_if(plants_, [](const plant_state& p) { return p.biomass <= 0; });
}
void world_state::reproduce(std::size_t i, std::size_t adults) {
    auto& c = creatures_[i];
    if (c.incubation > 0 || (c.species == species_kind::ladybird && c.protein < .4f))
        return;
    float maturity = maturity_age(c.species);
    if (c.energy < reproduction_energy || c.age < maturity || c.reproduction_cooldown > 0 ||
        creatures_.size() >= max_creatures)
        return;
    creature_state* mate = nullptr;
    float best = .45f;
    for (std::size_t j = 0; j < adults; ++j) {
        auto& o = creatures_[j];
        if (i == j || o.incubation > 0 || o.reproduction_cooldown > 0 || o.species != c.species ||
            o.age < maturity || o.energy < .65f)
            continue;
        float d = distance_squared(c.position, o.position);
        if (d < best) {
            best = d;
            mate = &o;
        }
    }
    if (!mate)
        return;
    const bool clutch = c.species == species_kind::ladybird;
    const int offspring = std::min<std::size_t>(clutch ? 2 : 1, max_creatures - creatures_.size());
    const float mother_cost = clutch ? .50f : .44f, father_cost = clutch ? .24f : .15f;
    c.energy -= mother_cost;
    mate->energy -= father_cost;
    if (clutch)
        c.protein -= .4f;
    c.reproduction_cooldown = (c.species == species_kind::snail ? 7.5f
                               : clutch                         ? 12.0f
                                                                : 1.7f) /
                              c.genes.fertility;
    mate->reproduction_cooldown = c.reproduction_cooldown;
    entity_id mother = c.id, father = mate->id;
    species_kind species = c.species;

    const inherited_traits maternal = c.genes, paternal = mate->genes;
    const vec2 birthplace = c.position;
    const matter_amount investment{c.matter.carbon * .28 + mate->matter.carbon * .15,
                                   c.matter.nitrogen * .28 + mate->matter.nitrogen * .15};
    c.matter.carbon *= .72;
    c.matter.nitrogen *= .72;
    mate->matter.carbon *= .85;
    mate->matter.nitrogen *= .85;
    for (int egg = 0; egg < offspring; ++egg) {
        inherited_traits g{inherit_trait(maternal.speed, paternal.speed),
                           inherit_trait(maternal.size, paternal.size),
                           inherit_trait(maternal.fertility, paternal.fertility),
                           inherit_trait(maternal.tolerance, paternal.tolerance)};
        vec2 p = bounded({birthplace.x + random(-.08f, .08f), birthplace.y + random(-.08f, .08f)});
        entity_id child = spawn(species, p, mother, father, g);
        if (child) {

            creatures_.back().energy = clutch ? (mother_cost + father_cost) * .9f / offspring : .50f;
            creatures_.back().matter = {investment.carbon / offspring, investment.nitrogen / offspring};
            creatures_.back().protein = 0;
            creatures_.back().incubation = species == species_kind::snail ? 1.2f : clutch ? .65f : 0;
            ++report_.born;
            if ((ancestor(mother) && ancestor(mother)->favourite) ||
                (ancestor(father) && ancestor(father)->favourite) || tick_ % 300 < 30)
                record(name(mother) + " and " + name(father) +
                       (species == species_kind::aphid ? " welcomed " : " laid an egg for ") + name(child) +
                       ".");
        }
    }
}
void world_state::ecology() {
    constexpr float dt = 30 * day_step;
    hydrology(dt);
    decompose(dt);
    const auto count = plants_.size();
    std::vector<plant_status> conditions;
    conditions.reserve(count);
    for (const auto& plant : plants_)
        conditions.push_back(plant_health(plant));
    for (std::size_t i = 0; i < count; ++i) {
        grow_plant(plants_[i], conditions[i], dt);
        scatter_seed(plants_[i]);
    }
    remove_dead_plants();
    const auto adults = creatures_.size();
    for (std::size_t i = 0; i < adults; ++i)
        reproduce(i, adults);
}
}
