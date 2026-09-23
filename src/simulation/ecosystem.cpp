#include "ecosystem.hpp"
#include "ecosystem_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace terrarium {
using namespace ecosystem_detail;
namespace {
bool valid_position(vec2 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::hypot(p.x, p.y) <= world_state::radius + 0.01f;
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

void world_state::advance(std::uint64_t ticks) {
    for (std::uint64_t i = 0; i < ticks; ++i)
        step();
}
}
