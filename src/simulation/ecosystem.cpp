#include "ecosystem.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <numbers>
#include <sstream>
#include <unordered_set>

namespace terrarium {
namespace {
constexpr float day_step = 1.0f / world_state::ticks_per_day;
constexpr float pi = std::numbers::pi_v<float>;
constexpr float reproduction_energy = .98f;
vec2 bounded(vec2 p) {
    const float length = std::hypot(p.x, p.y);
    if (length > world_state::radius) {
        p.x *= world_state::radius / length;
        p.y *= world_state::radius / length;
    }
    return p;
}
vec2 dry_ground(vec2 p) {
    if (!world_state::in_pond(p))
        return p;
    const float radius = std::sqrt(world_state::pond_radius_squared(p));
    if (radius < .0001f)
        return {world_state::pond_center.x, world_state::pond_center.y - world_state::pond_radii.y * 1.03f};
    return {world_state::pond_center.x + (p.x - world_state::pond_center.x) * 1.03f / radius,
            world_state::pond_center.y + (p.y - world_state::pond_center.y) * 1.03f / radius};
}
bool valid_position(vec2 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::hypot(p.x, p.y) <= world_state::radius + 0.01f;
}
bool range(float f, float lo, float hi) {
    return std::isfinite(f) && f >= lo && f <= hi;
}
bool valid_genes(inherited_traits g) {
    return range(g.speed, .5f, 1.8f) && range(g.size, .5f, 1.8f) && range(g.fertility, .5f, 1.8f) &&
           range(g.tolerance, .5f, 1.8f);
}
std::ostream& operator<<(std::ostream& o, inherited_traits g) {
    return o << g.speed << ' ' << g.size << ' ' << g.fertility << ' ' << g.tolerance;
}
std::istream& operator>>(std::istream& i, inherited_traits& g) {
    return i >> g.speed >> g.size >> g.fertility >> g.tolerance;
}
}
float distance_squared(vec2 a, vec2 b) {
    return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}
const char* species_name(species_kind s) {
    return std::array{"Aphid", "Snail", "Ladybird"}[static_cast<int>(s)];
}
const char* plant_name(plant_kind p) {
    return std::array{"Fern", "Clover", "Wildflower"}[static_cast<int>(p)];
}
std::string creature_name(entity_id id) {
    constexpr std::array names{"Pip",   "Clover",  "Miso",  "Fern",  "Basil",  "Pebble", "Coco",
                               "Maple", "Bramble", "Olive", "Poppy", "Dew",    "Fig",    "Mochi",
                               "Cedar", "Bean",    "Wren",  "Honey", "Sprout", "Moss"};
    return std::string(names[(id * 7) % names.size()]) + " " + std::to_string(id);
}
const char* activity_name(creature_activity activity) {
    return std::array{"Exploring",
                      "Resting",
                      "Having a little meal",
                      "Looking for aphids",
                      "Hiding in the leaves",
                      "Looking for company",
                      "Seeking a damp spot"}[static_cast<int>(activity)];
}
float world_state::pond_radius_squared(vec2 p) {
    return (p.x - pond_center.x) * (p.x - pond_center.x) / 1.44f +
           (p.y - pond_center.y) * (p.y - pond_center.y) / .81f;
}
bool world_state::in_pond(vec2 p) {
    return pond_radius_squared(p) < 1;
}
std::string world_state::name(entity_id id) const {
    const auto* a = ancestor(id);
    return a && !a->nickname.empty() ? a->nickname : creature_name(id);
}
bool world_state::rename(entity_id id, const std::string& nickname) {
    if (nickname.size() > 64 ||
        std::any_of(nickname.begin(), nickname.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        return false;
    auto i = std::lower_bound(family_.begin(), family_.end(), id,
                              [](const ancestor_record& a, entity_id id) { return a.id < id; });
    if (i == family_.end() || i->id != id)
        return false;
    i->nickname = nickname;
    return true;
}
bool world_state::toggle_favourite(entity_id id) {
    auto i = std::lower_bound(family_.begin(), family_.end(), id,
                              [](const ancestor_record& a, entity_id id) { return a.id < id; });
    if (i == family_.end() || i->id != id)
        return false;
    i->favourite = !i->favourite;
    return true;
}
double world_state::water_balance() const {
    const double capacity = soil_cell_capacity();
    double total =
        water_.pond + water_.vapour + vessel_.film_kg + water_.escaped + water_.overflow - water_.added;
    for (const auto& soil : soil_)
        total += soil.water * capacity;
    return total;
}
void world_state::water(vec2 p, double amount) {
    if (!valid_position(p) || !std::isfinite(amount) || amount <= 0)
        return;
    const double capacity = soil_cell_capacity();
    amount = std::min(amount, 2.0);
    water_.added += amount;
    if (in_pond(p))
        water_.pond += amount;
    else {
        std::array<double, soil_width * soil_width> weights{};
        double total = 0;
        for (int y = 0; y < soil_width; ++y)
            for (int x = 0; x < soil_width; ++x) {
                vec2 point{(x + .5f) * 10 / soil_width - 5, (y + .5f) * 10 / soil_width - 5};
                double d = distance_squared(p, point);
                if (d < 1.2) {
                    weights[y * soil_width + x] = std::exp(-d * 4);
                    total += weights[y * soil_width + x];
                }
            }
        for (std::size_t i = 0; i < soil_.size(); ++i) {
            double incoming = amount * weights[i] / total;
            double absorbed = std::min(incoming, (1 - soil_[i].water) * capacity);
            soil_[i].water += absorbed / capacity;
            water_.pond += incoming - absorbed;
        }
    }
    if (water_.pond > reservoir_capacity()) {
        water_.overflow += water_.pond - reservoir_capacity();
        water_.pond = reservoir_capacity();
    }
}
void world_state::hydrology(double dt) {
    const double capacity = soil_cell_capacity();
    const auto old = soil_;

    for (int y = 0; y < soil_width; ++y)
        for (int x = 0; x < soil_width; ++x) {
            int i = y * soil_width + x;
            for (auto [dx, dy] : std::array<std::pair<int, int>, 2>{{{1, 0}, {0, 1}}}) {
                if (x + dx >= soil_width || y + dy >= soil_width)
                    continue;
                int j = (y + dy) * soil_width + x + dx;
                double flux = (old[i].water - old[j].water) * .18 * dt;
                soil_[i].water -= flux;
                soil_[j].water += flux;
            }
        }
    const double rainfall =
        day() < climate.rain_until
            ? 9.5 * dt * (design_.form == vessel_form::legacy ? 1 : parameters_.ground_area / .264)
            : 0;
    water_.added += rainfall;
    water_.pond += rainfall * .15;
    receive_water(rainfall * .85);

    for (int y = 0; y < soil_width; ++y)
        for (int x = 0; x < soil_width; ++x) {
            vec2 p{(x + .5f) * 10 / soil_width - 5, (y + .5f) * 10 / soil_width - 5};
            double d = std::sqrt(distance_squared(p, {1.35f, .75f}));
            auto& soil = soil_[y * soil_width + x];
            if (d < 2 && soil.water < .85) {
                double flow =
                    std::min({water_.pond, (.85 - soil.water) * capacity, .008 * std::exp(-d) * dt});
                soil.water += flow / capacity;
                water_.pond -= flow;
            }
            const double field_capacity = design_.form == vessel_form::legacy ? .9 : design_.field_capacity();
            if (soil.water > field_capacity) {
                double drainage =
                    (soil.water - field_capacity) * capacity *
                    (design_.form == vessel_form::legacy ? .15 * dt
                                                         : -std::expm1(-design_.drainage_rate() * dt));
                soil.water -= drainage / capacity;
                water_.pond += drainage;
            }
        }
    if (water_.pond > reservoir_capacity()) {
        water_.overflow += water_.pond - reservoir_capacity();
        water_.pond = reservoir_capacity();
    }
}
void world_state::receive_water(double kg) {
    const double capacity = soil_cell_capacity();
    const double received = kg / soil_.size();
    for (auto& soil : soil_) {
        const double absorbed = std::min(received, (1 - soil.water) * capacity);
        soil.water += absorbed / capacity;
        water_.pond += received - absorbed;
    }
    if (water_.pond > reservoir_capacity()) {
        water_.overflow += water_.pond - reservoir_capacity();
        water_.pond = reservoir_capacity();
    }
}
void world_state::microclimate() {
    const double capacity = soil_cell_capacity();
    double soil_water = 0, foliage = 0;
    for (const auto& soil : soil_)
        soil_water += soil.water * capacity;
    for (const auto& plant : plants_)
        foliage += plant.biomass * .002;
    const double supply = soil_water + water_.pond;
    const double wetness = std::min(1., soil_water / (soil_.size() * capacity * .4));
    const double area =
        design_.form == vessel_form::legacy
            ? (.251 + foliage * (.1 + .9 * daylight())) * wetness + (water_.pond > 0 ? .013 : 0)
            : (parameters_.ground_area * .95 + foliage * (.1 + .9 * daylight())) * wetness +
                  (pond_level() > 0 ? parameters_.ground_area * .05 : 0);
    const auto flux = physics::advance(
        vessel_, water_.vapour, supply, 86400. / ticks_per_day,
        {ambient_temperature(), climate.ambient_humidity, irradiance(), climate.opening, area}, parameters_);
    water_.added += flux.air_in_kg;
    water_.escaped += flux.air_out_kg;
    if (flux.surface_kg > 0 && supply > 0) {

        const double fraction = std::min(1., flux.surface_kg / supply);
        water_.pond *= 1 - fraction;
        for (auto& soil : soil_)
            soil.water *= 1 - fraction;
    }
    receive_water(flux.drip_kg + std::max(0., -flux.surface_kg));
}
float world_state::pond_level() const {
    const double fraction = water_.pond / reservoir_capacity();
    return static_cast<float>(design_.form == vessel_form::legacy ? fraction
                                                                  : std::clamp((fraction - .8) / .2, 0., 1.));
}
bool world_state::construct(const garden_design& design) {
    if (!design.valid() || design.form == vessel_form::legacy || tick_ != 0 || !plants_.empty() ||
        !creatures_.empty() || !family_.empty())
        return false;
    design_ = design;
    parameters_ = design_.parameters();
    soil_.fill(soil_cell{});
    water_ = {};
    water_.pond = reservoir_capacity() * .2;
    vessel_ = {};
    vessel_.body_c = vessel_.glass_c = ambient_temperature();
    water_.vapour = physics::saturation_density(vessel_.body_c) * parameters_.air_volume * .75;
    initialize_matter();
    return true;
}
float world_state::random(float low, float high) {
    return low + (high - low) * static_cast<float>(random_() >> 8) / 16777216.0f;
}
vec2 world_state::random_position() {
    const float a = random(0, 2 * pi), r = std::sqrt(random()) * (radius - .2f);
    return {std::cos(a) * r, std::sin(a) * r};
}
world_state::world_state(std::uint32_t seed, bool populate) : random_(seed) {
    water_.vapour =
        physics::saturation_density(vessel_.body_c) * physics::vessel_parameters{}.air_volume * .75;
    creatures_.reserve(max_creatures);
    plants_.reserve(max_plants);
    family_.reserve(4096);
    if (!populate) {
        initialize_matter();
        return;
    }
    for (int i = 0; i < 62; ++i) {
        vec2 p = random_position();

        if (distance_squared(p, pond_center) < 2.0f)
            continue;
        add_plant(static_cast<plant_kind>(i % 3), p);
        plants_.back().biomass = random(.5f, 1.2f);
        plants_.back().age = random(0, 4);
    }
    for (int s = 0; s < 3; ++s)
        for (int i = 0; i < std::array{36, 5, 3}[s]; ++i) {
            vec2 p = random_position();
            if (s == 0 && !plants_.empty())
                p = bounded({plants_[i % plants_.size()].position.x + .08f,
                             plants_[i % plants_.size()].position.y + .08f});
            add_creature(static_cast<species_kind>(s), dry_ground(p));
            creatures_.back().age = s == 1 ? random(4, 9) : random(.6f, 2.8f);
            creatures_.back().energy = random(.65f, 1.1f);
        }
    journal_.clear();
    record("A little world begins. Make yourself at home.");
    history_.push_back({day(), populations(), static_cast<int>(plants_.size())});
    initialize_matter();
}
int world_state::soil_index(vec2 p) const {
    const int x = std::clamp(static_cast<int>((p.x + 5) * soil_width / 10), 0, soil_width - 1);
    const int y = std::clamp(static_cast<int>((p.y + 5) * soil_width / 10), 0, soil_width - 1);
    return y * soil_width + x;
}
const soil_cell& world_state::soil_at(vec2 p) const {
    return soil_[soil_index(p)];
}
float world_state::temperature() const {
    return static_cast<float>(vessel_.body_c);
}
float world_state::ambient_temperature() const {
    return climate.temperature +
           (climate.seasons ? 5 * std::sin(static_cast<float>(day() / 48) * 2 * pi) : 0);
}
float world_state::daylight() const {
    return climate.day_night ? std::max(0.f, std::cos(static_cast<float>(std::fmod(day(), 1.)) * 2 * pi)) : 1;
}
float world_state::irradiance() const {
    const float season = climate.seasons ? .8f + .2f * std::sin(static_cast<float>(day() / 48) * 2 * pi) : 1;
    return 350 * climate.sunlight * season * climate.room_exposure * daylight();
}
float world_state::light() const {
    return std::clamp(irradiance() / (350 * .55f), 0.f, 1.f);
}
float world_state::moisture() const {
    float total = 0;
    for (auto s : soil_)
        total += s.water;
    return total / soil_.size();
}
const char* world_state::season() const {
    return climate.seasons ? std::array{"Spring", "Summer", "Autumn",
                                        "Winter"}[static_cast<int>(std::fmod(day() / 12, 4))]
                           : "Endless spring";
}
std::array<int, 3> world_state::populations() const {
    std::array<int, 3> p{};
    for (const auto& c : creatures_)
        ++p[static_cast<int>(c.species)];
    return p;
}
const ancestor_record* world_state::ancestor(entity_id id) const {
    auto i = std::lower_bound(family_.begin(), family_.end(), id,
                              [](const ancestor_record& a, entity_id b) { return a.id < b; });
    return i != family_.end() && i->id == id ? &*i : nullptr;
}
const creature_state* world_state::creature(entity_id id) const {
    for (const auto& c : creatures_)
        if (c.id == id)
            return &c;
    return nullptr;
}
void world_state::record(std::string message) {
    journal_.push_front({day(), std::move(message)});
    if (journal_.size() > 32)
        journal_.pop_back();
}
bool world_state::can_add_plant(plant_kind kind, vec2 p) const {
    return plants_.size() < max_plants && valid_position(p) && !in_pond(p) && static_cast<int>(kind) >= 0 &&
           static_cast<int>(kind) <= 2;
}
bool world_state::can_add_creature(species_kind species, vec2 p) const {
    return creatures_.size() < max_creatures && valid_position(p) && !in_pond(p) &&
           static_cast<int>(species) >= 0 && static_cast<int>(species) <= 2;
}
bool world_state::add_plant(plant_kind kind, vec2 p) {
    if (!can_add_plant(kind, p))
        return false;
    plants_.push_back({next_id_++, kind, p, .45f, 0, random(2, 5), static_cast<std::uint32_t>(random_())});
    plants_.back().nectar_carbon = kind == plant_kind::flower ? .2 : 0;
    matter_.introduced.carbon += plants_.back().biomass * plant_carbon + plants_.back().nectar_carbon;
    matter_.introduced.nitrogen += plants_.back().biomass * plant_nitrogen;
    return true;
}
entity_id world_state::spawn(species_kind s, vec2 p, entity_id mother, entity_id father,
                             inherited_traits genes) {
    if (creatures_.size() >= max_creatures)
        return 0;
    const entity_id id = next_id_++;
    const auto* m = ancestor(mother);
    const auto* f = ancestor(father);
    const int generation = mother ? 1 + std::max(m ? m->generation : 0, f ? f->generation : 0) : 0;
    p = dry_ground(bounded(p));
    creatures_.push_back({id, s, p, p, random(0, 2 * pi), 0, .8f, random(.7f, 1.4f), genes});
    family_.push_back({id, mother, father, s, generation, day(), -1, genes, {}, false});
    return id;
}
entity_id world_state::add_creature(species_kind s, vec2 p) {
    if (!can_add_creature(s, p))
        return 0;
    const entity_id id = spawn(
        s, p, 0, 0, {random(.85f, 1.15f), random(.85f, 1.15f), random(.85f, 1.15f), random(.85f, 1.15f)});
    if (id) {
        creatures_.back().age = maturity_age(s);
        creatures_.back().matter = body_matter(creatures_.back());
        matter_.introduced.carbon += creatures_.back().matter.carbon;
        matter_.introduced.nitrogen += creatures_.back().matter.nitrogen;
        record(name(id) + " the " + species_name(s) + " moved in.");
    }
    return id;
}
void world_state::rain(double days) {
    if (!std::isfinite(days) || days <= 0)
        return;
    climate.rain_until = std::max(climate.rain_until, day()) + std::min(days, 5.0);
    record("A passing shower. The garden drinks it in.");
}

void world_state::behavior() {
    constexpr float period = 15 * day_step;

    constexpr int width = 12;
    std::array<int, width * width> heads;
    heads.fill(-1);
    std::array<int, max_creatures> next{};
    std::array<float, max_creatures> shelter{};
    auto cell = [](vec2 p) {
        return std::clamp(static_cast<int>(p.y + 6), 0, 11) * 12 +
               std::clamp(static_cast<int>(p.x + 6), 0, 11);
    };
    for (std::size_t i = 0; i < creatures_.size(); ++i) {
        int bin = cell(creatures_[i].position);
        next[i] = heads[bin];
        heads[bin] = static_cast<int>(i);
        if (creatures_[i].species == species_kind::aphid)
            shelter[i] = shelter_at(creatures_[i].position);
    }

    std::array<std::array<int, 3>, max_plants> food_crowding{};
    for (std::size_t p = 0; p < plants_.size(); ++p) {
        const int bin = cell(plants_[p].position), cx = bin % width, cy = bin / width;
        for (int y = std::max(0, cy - 1); y <= std::min(11, cy + 1); ++y)
            for (int x = std::max(0, cx - 1); x <= std::min(11, cx + 1); ++x)
                for (int i = heads[y * width + x]; i >= 0; i = next[i])
                    if (creatures_[i].energy > 0 && creatures_[i].incubation <= 0 &&
                        distance_squared(creatures_[i].position, plants_[p].position) < .36f)
                        ++food_crowding[p][static_cast<int>(creatures_[i].species)];
    }
    for (auto& c : creatures_) {
        if (c.energy <= 0 || c.incubation > 0)
            continue;
        c.attention = 0;
        c.activity_time = std::max(0.0f, c.activity_time - period);
        const int bin = cell(c.position), cx = bin % width, cy = bin / width;
        const creature_state* mate = nullptr;
        creature_state* prey = nullptr;
        const creature_state* danger = nullptr;
        float mate_distance = 3.2f, prey_distance = 3.2f, danger_distance = .4f, prey_shelter = 0;
        int neighbours = 0;
        for (int y = std::max(0, cy - 2); y <= std::min(11, cy + 2); ++y)
            for (int x = std::max(0, cx - 2); x <= std::min(11, cx + 2); ++x)
                for (int i = heads[y * width + x]; i >= 0; i = next[i]) {
                    auto& other = creatures_[i];
                    if (other.id == c.id || other.energy <= 0 || other.incubation > 0)
                        continue;
                    float d = distance_squared(c.position, other.position);
                    if (other.species == c.species && d < 1.44f)
                        ++neighbours;
                    if (c.species == species_kind::aphid && other.species == species_kind::ladybird &&
                        d < danger_distance) {
                        danger_distance = d;
                        danger = &other;
                    }
                    if (c.species == species_kind::ladybird && other.species == species_kind::aphid &&
                        d < prey_distance && d < 3.2f * (1 - shelter[i])) {
                        prey_shelter = shelter[i];
                        prey_distance = d;
                        prey = &other;
                    }
                    if (other.species == c.species && other.age >= maturity_age(c.species) &&
                        other.reproduction_cooldown <= 0 && other.energy > .65f && d < mate_distance) {
                        mate_distance = d;
                        mate = &other;
                    }
                }

        c.energy -= period * std::max(0, neighbours - (c.species == species_kind::aphid ? 12 : 6)) * .014f;
        const float courtship_energy =
            reproduction_energy + (c.activity == creature_activity::courting ? .01f : .10f);
        const bool seeking_mate = c.age >= maturity_age(c.species) && c.energy >= courtship_energy &&
                                  c.reproduction_cooldown <= 0 &&
                                  (c.species != species_kind::ladybird || c.protein >= .4f);
        if (c.species == species_kind::ladybird && seeking_mate) {
            float range = 64;
            for (const auto& other : creatures_) {
                if (other.id == c.id || other.species != c.species || other.incubation > 0 ||
                    other.age < maturity_age(c.species) || other.energy < .65f ||
                    other.reproduction_cooldown > 0)
                    continue;
                float d = distance_squared(c.position, other.position);
                if (d < range) {
                    range = d;
                    mate = &other;
                }
            }
        }
        if (danger && c.energy > .2f) {
            float dx = c.position.x - danger->position.x, dy = c.position.y - danger->position.y,
                  d = std::max(.01f, std::hypot(dx, dy));
            c.target = bounded({c.position.x + dx / d * .4f, c.position.y + dy / d * .4f});
            c.attention = danger->id;
            c.activity = creature_activity::fleeing;
            continue;
        }
        if (c.energy > 1.15f && c.activity_time <= 0) {
            c.activity = creature_activity::resting;
            c.activity_time = .1f + random(0, .12f);
        }
        if (c.activity == creature_activity::resting && c.activity_time > 0 && c.energy > .65f) {
            c.target = c.position;
            continue;
        }
        if (seeking_mate && mate) {
            c.attention = mate->id;
            c.target = mate->position;
            c.activity = creature_activity::courting;
            continue;
        }
        if (c.species == species_kind::snail) {
            auto& soil = soil_[soil_index(c.position)];
            if (soil.water < .25) {
                float a = std::atan2(pond_center.y - c.position.y, pond_center.x - c.position.x);
                c.target = bounded({c.position.x + std::cos(a) * .6f, c.position.y + std::sin(a) * .6f});
                c.activity = creature_activity::drinking;
                continue;
            }
            if (soil.litter > .01 && c.energy < 1.25f) {
                double eaten = std::min(soil.litter, .13 * period);
                const double nitrogen = soil.litter_nitrogen * eaten / soil.litter;
                soil.litter -= eaten;
                soil.litter_nitrogen -= nitrogen;
                digest_food(c, {eaten * plant_carbon, nitrogen});
                c.meal = food_kind::litter;
                c.last_meal = day();
                c.energy = std::min(1.5f, c.energy + static_cast<float>(eaten * 4));
                c.activity = creature_activity::grazing;
                c.target = c.position;
                continue;
            }
        }
        plant_state* food = nullptr;
        float distance = 4, best_food = 4;
        for (std::size_t plant_index = 0; plant_index < plants_.size(); ++plant_index) {
            auto& p = plants_[plant_index];
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
            if (d < 4 && score < best_food) {
                best_food = score;
                distance = d;
                food = &p;
            }
        }
        if (c.species == species_kind::ladybird && c.energy < .95f && prey && c.meal_cooldown <= 0 &&
            (!food || prey_distance < best_food)) {
            c.attention = prey->id;
            c.target = prey->position;
            c.activity = creature_activity::hunting;
            if (prey_distance < .045f * (1 - .8f * prey_shelter)) {
                digest_food(c, prey->matter);
                prey->matter = {};
                prey->energy = -1;
                prey->death_cause = death_cause::predation;
                prey->predator = c.id;
                c.meal = food_kind::aphid;
                c.last_meal = day();
                c.protein = std::min(1.f, c.protein + .6f);
                c.energy = std::min(1.5f, c.energy + .65f);
                c.meal_cooldown = .7f;
                c.activity = creature_activity::resting;
                c.activity_time = .18f;
            }
            continue;
        }
        if (c.activity == creature_activity::hunting) {
            c.activity = creature_activity::wandering;
            c.activity_time = 0;
        }
        if (food && c.energy < 1.18f) {
            c.attention = food->id;
            c.target = food->position;
            c.activity = creature_activity::wandering;
            if (distance < .09f) {
                c.activity = creature_activity::grazing;
                c.last_meal = day();
                c.meal = c.species == species_kind::ladybird ? food_kind::nectar : food_kind::leaves;
                if (c.species == species_kind::ladybird) {
                    const double nectar = std::min(food->nectar_carbon, double(period) * 2);
                    food->nectar_carbon -= nectar;
                    digest_food(c, {nectar, 0});
                    c.energy = std::min(1.5f, c.energy + static_cast<float>(nectar * .15));
                } else {
                    constexpr float rate = .035f;
                    float eaten = std::min(food->biomass - .12f, rate * period * c.genes.size);
                    const float before = food->biomass;
                    food->biomass -= eaten;
                    eaten = before - food->biomass;
                    food->grazing += eaten;
                    digest_food(c, {eaten * plant_carbon, eaten * plant_nitrogen});
                    c.energy = std::min(1.5f, c.energy + eaten * (c.species == species_kind::aphid ? 24 : 9));
                }
            }
        } else if (distance_squared(c.position, c.target) < .08f || random() < .07f) {
            float a = random(0, 2 * pi);
            c.target = bounded({c.position.x + std::cos(a) * .7f, c.position.y + std::sin(a) * .7f});
            c.activity = creature_activity::wandering;
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
    for (const auto& p : plants_)
        conditions.push_back(plant_health(p));
    for (std::size_t i = 0; i < count; ++i) {
        auto& p = plants_[i];
        const auto& condition = conditions[i];
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
    for (const auto& p : plants_)
        if (p.biomass <= 0)
            deposit(p.position, {p.nectar_carbon, 0});
    std::erase_if(plants_, [](const plant_state& p) { return p.biomass <= 0; });
    const auto adults = creatures_.size();
    for (std::size_t i = 0; i < adults; ++i) {
        auto& c = creatures_[i];
        if (c.incubation > 0 || (c.species == species_kind::ladybird && c.protein < .4f))
            continue;
        float maturity = maturity_age(c.species);
        if (c.energy < reproduction_energy || c.age < maturity || c.reproduction_cooldown > 0 ||
            creatures_.size() >= max_creatures)
            continue;
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
            continue;
        auto inherit = [this](float a, float b) {
            return std::clamp((a + b) * .5f + random(-.055f, .055f), .5f, 1.8f);
        };
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
            inherited_traits g{inherit(maternal.speed, paternal.speed), inherit(maternal.size, paternal.size),
                               inherit(maternal.fertility, paternal.fertility),
                               inherit(maternal.tolerance, paternal.tolerance)};
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
                           (species == species_kind::aphid ? " welcomed " : " laid an egg for ") +
                           name(child) + ".");
            }
        }
    }
}
void world_state::step() {
    ++tick_;
    microclimate();
    if (tick_ % 15 == 0)
        behavior();
    const float temp = temperature();
    for (auto& c : creatures_) {
        c.age += day_step;
        if (c.energy > 0) {
            const double respiration = c.matter.carbon * -std::expm1(-.1 * day_step);
            const double excretion = c.matter.nitrogen * -std::expm1(-.025 * day_step);
            c.matter.carbon -= respiration;
            c.matter.nitrogen -= excretion;
            matter_.respired_carbon += respiration;
            soil_[soil_index(c.position)].nutrients += excretion / mineral_capacity();
        }
        if (c.incubation > 0) {
            c.incubation = std::max(0.f, c.incubation - day_step);
            if (c.incubation == 0) {
                c.age = 0;
                ++report_.hatched;
                const auto* a = ancestor(c.id);
                if (a && (a->favourite || (ancestor(a->mother) && ancestor(a->mother)->favourite)))
                    record(name(c.id) + " hatched and began looking for food.");
            }
            continue;
        }
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
            c.death_cause = stress > hunger * c.genes.size
                                ? (temp < 22 ? death_cause::cold : death_cause::heat)
                                : death_cause::starvation;
        float lifespan = c.species == species_kind::snail      ? 65
                         : c.species == species_kind::ladybird ? 35
                                                               : 19;
        if (c.energy > 0 && c.age > lifespan * c.genes.tolerance) {
            c.energy = -1;
            c.death_cause = death_cause::old_age;
        }
        if (c.energy <= 0)
            continue;
        const bool escaping_pond = in_pond(c.position);
        if (escaping_pond) {
            c.target = dry_ground(c.position);
            c.activity = creature_activity::wandering;
            c.attention = 0;
        }
        float dx = c.target.x - c.position.x, dy = c.target.y - c.position.y, d = std::hypot(dx, dy);
        if (d > .025f && c.activity != creature_activity::resting &&
            c.activity != creature_activity::grazing) {
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
    if (tick_ % 30 == 0)
        ecology();
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
    if (tick_ % 300 == 0) {
        history_.push_back({day(), populations(), static_cast<int>(plants_.size())});
        if (history_.size() > 240)
            history_.pop_front();
        matter_history_.push_back(matter_sample());
        if (matter_history_.size() > 240)
            matter_history_.pop_front();
    }
    if (tick_ % ticks_per_day == 0) {
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
    if (tick_ % (ticks_per_day * 12) == 0 && climate.seasons)
        record(std::string(season()) + " arrives in the garden.");
}
void world_state::advance(std::uint64_t ticks) {
    for (std::uint64_t i = 0; i < ticks; ++i)
        step();
}
void world_state::write(std::ostream& o) const {
    o.imbue(std::locale::classic());
    o << std::setprecision(std::numeric_limits<double>::max_digits10);
    o << "LITTLE_WORLD 6\n" << tick_ << ' ' << next_id_ << '\n' << random_ << '\n';
    o << climate.temperature << ' ' << climate.sunlight << ' ' << climate.sun_angle << ' ' << climate.seasons
      << ' ' << climate.rain_until << '\n';
    for (auto s : soil_)
        o << s.water << ' ' << s.nutrients << ' ' << s.litter << '\n';
    o << plants_.size() << '\n';
    for (auto p : plants_)
        o << p.id << ' ' << static_cast<int>(p.kind) << ' ' << p.position.x << ' ' << p.position.y << ' '
          << p.biomass << ' ' << p.age << ' ' << p.seed_cooldown << ' ' << p.shape << ' ' << p.grazing << ' '
          << p.parent << '\n';
    o << creatures_.size() << '\n';
    for (auto c : creatures_)
        o << c.id << ' ' << static_cast<int>(c.species) << ' ' << c.position.x << ' ' << c.position.y << ' '
          << c.target.x << ' ' << c.target.y << ' ' << c.heading << ' ' << c.age << ' ' << c.energy << ' '
          << c.reproduction_cooldown << ' ' << c.genes << ' ' << static_cast<int>(c.activity) << ' '
          << c.activity_time << ' ' << c.meal_cooldown << ' ' << c.incubation << ' ' << c.protein << ' '
          << c.attention << ' ' << static_cast<int>(c.meal) << ' ' << c.last_meal << ' '
          << static_cast<int>(c.death_cause) << ' ' << c.predator << '\n';
    o << family_.size() << '\n';
    for (auto a : family_)
        o << a.id << ' ' << a.mother << ' ' << a.father << ' ' << static_cast<int>(a.species) << ' '
          << a.generation << ' ' << a.born << ' ' << a.died << ' ' << a.genes << ' '
          << std::quoted(a.nickname) << ' ' << a.favourite << ' ' << static_cast<int>(a.death_cause) << ' '
          << a.predator << '\n';
    o << history_.size() << '\n';
    for (auto s : history_)
        o << s.day << ' ' << s.population[0] << ' ' << s.population[1] << ' ' << s.population[2] << ' '
          << s.plants << '\n';
    o << journal_.size() << '\n';
    for (const auto& e : journal_)
        o << e.day << ' ' << std::quoted(e.message) << '\n';
    o << water_.pond << ' ' << water_.vapour << ' ' << water_.added << ' ' << water_.escaped << ' '
      << water_.overflow << '\n';
    o << report_.born << ' ' << report_.hatched << ' ' << report_.hunted << ' ' << report_.starved << ' '
      << report_.weather_deaths << ' ' << report_.old_age << ' ' << report_.seedlings << ' '
      << report_.compost << '\n';
    o << climate.opening << ' ' << climate.room_exposure << ' ' << climate.ambient_humidity << ' '
      << climate.day_night << '\n';
    o << vessel_.body_c << ' ' << vessel_.glass_c << ' ' << vessel_.film_kg << ' ' << vessel_.external_j
      << ' ' << vessel_.evaporated_kg << ' ' << vessel_.condensed_kg << ' ' << vessel_.air_in_kg << ' '
      << vessel_.air_out_kg << '\n';
    o << int(design_.form) << ' ' << int(design_.mix) << ' ' << design_.soil_depth << ' '
      << design_.drainage_depth << '\n';
    o << "MATTER 1\n";
    o << matter_.initial.carbon << ' ' << matter_.initial.nitrogen << ' ' << matter_.introduced.carbon << ' '
      << matter_.introduced.nitrogen << ' ' << matter_.fixed_carbon << ' ' << matter_.respired_carbon << ' '
      << report_.mineralized_nitrogen << ' ' << report_.immobilized_nitrogen << '\n';
    for (const auto& soil : soil_)
        o << soil.litter_nitrogen << ' ' << soil.microbial_nitrogen << '\n';
    o << creatures_.size() << '\n';
    for (const auto& c : creatures_)
        o << c.id << ' ' << c.matter.carbon << ' ' << c.matter.nitrogen << '\n';
    o << plants_.size() << '\n';
    for (const auto& p : plants_)
        o << p.id << ' ' << p.nectar_carbon << '\n';
    o << matter_history_.size() << '\n';
    for (const auto& sample : matter_history_) {
        o << sample.day;
        for (double value : sample.nitrogen)
            o << ' ' << value;
        o << ' ' << sample.carbon << '\n';
    }
}
bool world_state::read(std::istream& in) {
    in.imbue(std::locale::classic());
    std::string magic;
    int version{};
    if (!(in >> magic >> version) || magic != "LITTLE_WORLD" || (version < 1 || version > 6))
        return false;
    if (!(in >> tick_ >> next_id_ >> random_))
        return false;
    if (tick_ > 1000000000000ULL || next_id_ == 0)
        return false;
    in >> climate.temperature >> climate.sunlight >> climate.sun_angle >> climate.seasons >>
        climate.rain_until;
    if (!range(climate.temperature, 0, 45) || !range(climate.sunlight, 0, 1) ||
        !range(climate.sun_angle, -10, 10) || !std::isfinite(climate.rain_until) || climate.rain_until < 0)
        return false;
    for (auto& s : soil_) {
        in >> s.water >> s.nutrients;
        if (version >= 2)
            in >> s.litter;
        if (!range(s.water, 0, 1) || !range(s.nutrients, 0, version >= 6 ? 1e6 : 1) ||
            !range(s.litter, 0, version >= 6 ? 1e6 : 1))
            return false;
    }
    std::size_t n{};
    if (!(in >> n) || n > max_plants)
        return false;
    plants_.resize(n);
    std::unordered_set<entity_id> live_ids;
    for (auto& p : plants_) {
        int kind{};
        in >> p.id >> kind >> p.position.x >> p.position.y >> p.biomass >> p.age >> p.seed_cooldown >>
            p.shape;
        if (version >= 3)
            in >> p.grazing >> p.parent;
        if (!range(p.grazing, 0, 16) || p.parent >= p.id)
            return false;
        if (kind < 0 || kind > 2 || p.id == 0 || p.id >= next_id_ || !live_ids.insert(p.id).second ||
            !valid_position(p.position) || !range(p.biomass, 0, 1.6f) || !range(p.age, 0, 1e9f) ||
            !range(p.seed_cooldown, -1e9f, 10))
            return false;
        p.kind = static_cast<plant_kind>(kind);
    }
    if (!(in >> n) || n > max_creatures)
        return false;
    creatures_.resize(n);
    for (auto& c : creatures_) {
        int species{};
        in >> c.id >> species >> c.position.x >> c.position.y >> c.target.x >> c.target.y >> c.heading >>
            c.age >> c.energy >> c.reproduction_cooldown >> c.genes;
        int activity = 0;
        if (version >= 2)
            in >> activity >> c.activity_time >> c.meal_cooldown;
        if (activity < 0 || activity > 6 || !range(c.activity_time, 0, 10) || !range(c.meal_cooldown, 0, 10))
            return false;
        c.activity = static_cast<creature_activity>(activity);
        int meal = 0, cause = 0;
        if (version >= 3)
            in >> c.incubation >> c.protein >> c.attention >> meal >> c.last_meal >> cause >> c.predator;
        if (!range(c.incubation, 0, 2) || !range(c.protein, 0, 1) || c.attention >= next_id_ || meal < 0 ||
            meal > 4 || !std::isfinite(c.last_meal) || c.last_meal < -1 || c.last_meal > day() || cause < 0 ||
            cause > 5 || c.predator >= next_id_)
            return false;
        c.meal = static_cast<food_kind>(meal);
        c.death_cause = static_cast<death_cause>(cause);
        if (species < 0 || species > 2 || c.id == 0 || c.id >= next_id_ || !live_ids.insert(c.id).second ||
            !valid_position(c.position) || !valid_position(c.target) || !range(c.heading, -7, 7) ||
            !range(c.age, 0, 1e9f) || !range(c.energy, 0, 1.6f) ||
            !range(c.reproduction_cooldown, -1e9f, 30) || !valid_genes(c.genes))
            return false;
        c.species = static_cast<species_kind>(species);
    }
    if (!(in >> n) || n > 10000000)
        return false;
    family_.resize(n);
    entity_id previous = 0;
    for (auto& a : family_) {
        int species{};
        in >> a.id >> a.mother >> a.father >> species >> a.generation >> a.born >> a.died >> a.genes;
        if (version >= 2)
            in >> std::quoted(a.nickname) >> a.favourite;
        int cause = 0;
        if (version >= 3)
            in >> cause >> a.predator;
        if (cause < 0 || cause > 5 || a.predator >= next_id_)
            return false;
        a.death_cause = static_cast<death_cause>(cause);
        if (a.nickname.size() > 64 || std::any_of(a.nickname.begin(), a.nickname.end(),
                                                  [](unsigned char c) { return c < 32 || c == 127; }))
            return false;
        if (a.id <= previous || a.id >= next_id_ || a.mother >= a.id || a.father >= a.id || species < 0 ||
            species > 2 || a.generation < 0 || a.generation > 1000000 || !valid_genes(a.genes) ||
            !std::isfinite(a.born) || !std::isfinite(a.died) || a.born < 0 || a.born > day() ||
            (a.died != -1 && (a.died < a.born || a.died > day())))
            return false;
        previous = a.id;
        a.species = static_cast<species_kind>(species);
    }
    for (const auto& a : family_) {
        const auto* mother = ancestor(a.mother);
        const auto* father = ancestor(a.father);
        if (a.mother == 0 && a.father == 0) {
            if (a.generation != 0)
                return false;
        } else if (!mother || !father || mother->species != a.species || father->species != a.species ||
                   a.generation != 1 + std::max(mother->generation, father->generation))
            return false;
        if ((a.died == -1) != (creature(a.id) != nullptr))
            return false;
    }
    for (const auto& c : creatures_) {
        const auto* a = ancestor(c.id);
        if (!a || a->species != c.species)
            return false;
    }
    if (!(in >> n) || n > 240)
        return false;
    history_.resize(n);
    double previous_sample_day = -1;
    for (auto& s : history_) {
        in >> s.day >> s.population[0] >> s.population[1] >> s.population[2] >> s.plants;
        if (!std::isfinite(s.day) || s.day < 0 || s.day > day() || s.day <= previous_sample_day ||
            s.plants < 0 || s.plants > static_cast<int>(max_plants))
            return false;
        previous_sample_day = s.day;
        int total_population = 0;
        for (int p : s.population)
            if (p < 0 || p > static_cast<int>(max_creatures))
                return false;
            else
                total_population += p;
        if (total_population > static_cast<int>(max_creatures))
            return false;
    }
    if (!(in >> n) || n > 32)
        return false;
    journal_.resize(n);
    for (auto& e : journal_) {
        in >> e.day >> std::quoted(e.message);
        if (!std::isfinite(e.day) || e.day < 0 || e.day > day() || e.message.size() > 1024)
            return false;
    }
    if (version >= 2) {
        in >> water_.pond >> water_.vapour >> water_.added >> water_.escaped >> water_.overflow;
        for (double value : {water_.pond, water_.vapour, water_.added, water_.escaped, water_.overflow})
            if (!std::isfinite(value) || value < 0)
                return false;
    }
    report_ = {};
    if (version >= 3) {
        in >> report_.born >> report_.hatched >> report_.hunted >> report_.starved >>
            report_.weather_deaths >> report_.old_age >> report_.seedlings >> report_.compost;
        for (int count : {report_.born, report_.hatched, report_.hunted, report_.starved,
                          report_.weather_deaths, report_.old_age, report_.seedlings})
            if (count < 0 || count > 10000000)
                return false;
        if (!std::isfinite(report_.compost) || report_.compost < 0 || report_.compost > 10000000)
            return false;
    }
    if (version >= 4) {
        in >> climate.opening >> climate.room_exposure >> climate.ambient_humidity >> climate.day_night;
        in >> vessel_.body_c >> vessel_.glass_c >> vessel_.film_kg >> vessel_.external_j >>
            vessel_.evaporated_kg >> vessel_.condensed_kg >> vessel_.air_in_kg >> vessel_.air_out_kg;
        if (!range(climate.opening, 0, 1) || !range(climate.room_exposure, 0, 1) ||
            !range(climate.ambient_humidity, 0, 1) || !physics::valid(vessel_) || water_.vapour > .1)
            return false;
    } else {
        vessel_ = {};
        vessel_.body_c = vessel_.glass_c = ambient_temperature();

        const double capacity =
            physics::saturation_density(vessel_.body_c) * physics::vessel_parameters{}.air_volume;
        vessel_.film_kg = std::max(0., water_.vapour - capacity * .75);
        water_.vapour -= vessel_.film_kg;
        if (vessel_.film_kg > 100)
            return false;
    }
    design_ = garden_design{vessel_form::legacy};
    if (version >= 5) {
        int form{}, mix{};
        in >> form >> mix >> design_.soil_depth >> design_.drainage_depth;
        design_.form = static_cast<vessel_form>(form);
        design_.mix = static_cast<soil_mix>(mix);
        if (!design_.valid())
            return false;
    }
    parameters_ = design_.parameters();
    if (water_.pond > reservoir_capacity())
        return false;
    if (version >= 6) {
        std::string tag;
        int matter_version{};
        if (!(in >> tag >> matter_version) || tag != "MATTER" || matter_version != 1)
            return false;
        in >> matter_.initial.carbon >> matter_.initial.nitrogen >> matter_.introduced.carbon >>
            matter_.introduced.nitrogen >> matter_.fixed_carbon >> matter_.respired_carbon >>
            report_.mineralized_nitrogen >> report_.immobilized_nitrogen;
        auto valid_mass = [](double value) { return std::isfinite(value) && value >= 0 && value <= 1e15; };
        for (double value : {matter_.initial.carbon, matter_.initial.nitrogen, matter_.introduced.carbon,
                             matter_.introduced.nitrogen, matter_.fixed_carbon, matter_.respired_carbon,
                             report_.mineralized_nitrogen, report_.immobilized_nitrogen})
            if (!valid_mass(value))
                return false;
        for (auto& soil : soil_) {
            in >> soil.litter_nitrogen >> soil.microbial_nitrogen;
            if (!valid_mass(soil.litter_nitrogen) || !valid_mass(soil.microbial_nitrogen))
                return false;
        }
        if (!(in >> n) || n != creatures_.size())
            return false;
        for (auto& c : creatures_) {
            entity_id id{};
            in >> id >> c.matter.carbon >> c.matter.nitrogen;
            if (id != c.id || !valid_mass(c.matter.carbon) || !valid_mass(c.matter.nitrogen))
                return false;
        }
        if (!(in >> n) || n != plants_.size())
            return false;
        for (auto& p : plants_) {
            entity_id id{};
            in >> id >> p.nectar_carbon;
            if (id != p.id || !range(p.nectar_carbon, 0, 4) ||
                (p.kind != plant_kind::flower && p.nectar_carbon != 0))
                return false;
        }
        if (!(in >> n) || n > 240)
            return false;
        matter_history_.resize(n);
        double previous = -1;
        for (auto& sample : matter_history_) {
            in >> sample.day;
            for (double& value : sample.nitrogen) {
                in >> value;
                if (!valid_mass(value))
                    return false;
            }
            in >> sample.carbon;
            if (!valid_mass(sample.carbon) || !std::isfinite(sample.day) || sample.day < 0 ||
                sample.day > day() || sample.day <= previous)
                return false;
            previous = sample.day;
        }
    } else
        initialize_matter();
    if (!in)
        return false;
    in >> std::ws;
    return in.eof();
}
bool world_state::save(const std::filesystem::path& path, std::string& error) const {
    try {
        const auto temporary = std::filesystem::path(path.string() + ".tmp");
        {
            std::ofstream out(temporary, std::ios::trunc);
            if (!out)
                throw std::runtime_error("Cannot open save file.");
            write(out);
            out.flush();
            if (!out)
                throw std::runtime_error("Cannot write save file.");
        }
        if (std::filesystem::exists(path))
            std::filesystem::copy_file(path, path.string() + ".bak",
                                       std::filesystem::copy_options::overwrite_existing);
        std::error_code ec;
        std::filesystem::rename(temporary, path, ec);

        if (ec && std::filesystem::exists(path)) {
            const auto backup = std::filesystem::path(path.string() + ".bak");
            std::filesystem::remove(path);
            try {
                std::filesystem::rename(temporary, path);
            } catch (...) {
                std::filesystem::rename(backup, path);
                throw;
            }

        } else if (ec)
            throw std::filesystem::filesystem_error("Cannot finish save", temporary, path, ec);
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
bool world_state::load(const std::filesystem::path& path, std::string& error) {
    try {
        if (std::filesystem::file_size(path) > 512ULL * 1024 * 1024)
            throw std::runtime_error("Save file is too large.");
        std::ifstream in(path);
        world_state candidate(0, false);
        if (!in || !candidate.read(in))
            throw std::runtime_error("This save is incomplete or incompatible. Your garden is unchanged.");
        *this = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
std::uint64_t world_state::digest() const {
    std::ostringstream out;
    write(out);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : out.str()) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}
}
