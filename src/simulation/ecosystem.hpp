#pragma once

#include "construction.hpp"
#include "matter.hpp"
#include "vessel.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace terrarium {

using entity_id = std::uint64_t;
struct vec2 {
    float x{}, y{};
};
float distance_squared(vec2 a, vec2 b);
enum class species_kind : int { aphid, snail, ladybird };
enum class plant_kind : int { fern, clover, flower };
enum class death_cause : int { unknown, starvation, predation, cold, heat, old_age };
enum class food_kind : int { none, leaves, nectar, litter, aphid };
enum class plant_condition : int {
    growing,
    seed,
    thirsty,
    shade,
    scorched,
    cold,
    hot,
    poor_soil,
    grazed,
    mature,
    struggling,
    night
};
const char* death_reason(death_cause cause);
const char* plant_state_name(plant_condition state);
const char* plant_advice(plant_condition state);
float maturity_age(species_kind species);
enum class creature_activity : int { wandering, resting, grazing, hunting, fleeing, courting, drinking };
const char* activity_name(creature_activity activity);
const char* species_name(species_kind species);
const char* plant_name(plant_kind kind);
std::string creature_name(entity_id id);

struct inherited_traits {
    float speed{1}, size{1}, fertility{1}, tolerance{1};
};
struct creature_state {
    entity_id id{};
    species_kind species{};
    vec2 position{}, target{};
    float heading{}, age{}, energy{0.8f}, reproduction_cooldown{};
    inherited_traits genes{};
    creature_activity activity{creature_activity::wandering};
    float activity_time{}, meal_cooldown{};
    float incubation{}, protein{.5f};
    entity_id attention{};
    food_kind meal{food_kind::none};
    double last_meal{-1};
    death_cause death_cause{death_cause::unknown};
    entity_id predator{};
    matter_amount matter{};
};
struct ancestor_record {
    entity_id id{}, mother{}, father{};
    species_kind species{};
    int generation{};
    double born{}, died{-1};
    inherited_traits genes{};
    std::string nickname;
    bool favourite{};
    death_cause death_cause{death_cause::unknown};
    entity_id predator{};
};
struct plant_state {
    entity_id id{};
    plant_kind kind{};
    vec2 position{};
    float biomass{0.5f}, age{}, seed_cooldown{3};
    std::uint32_t shape{};
    float grazing{};
    entity_id parent{};
    double nectar_carbon{};
};
struct plant_status {
    float light{}, moisture{}, nutrients{}, growth{}, loss{};
    plant_condition state{plant_condition::growing};
};
struct day_report {
    int born{}, hatched{}, hunted{}, starved{}, weather_deaths{}, old_age{}, seedlings{};
    double compost{}, mineralized_nitrogen{}, immobilized_nitrogen{};
};
struct soil_cell {
    double water{0.64}, nutrients{0.75}, litter{0.18};
    double litter_nitrogen{2.7}, microbial_nitrogen{.0625};
};
struct water_state {

    double pond{4.5}, vapour{}, added{}, escaped{}, overflow{};
    static constexpr double pond_capacity = 8.0, cell_capacity = 0.025;
};
struct climate_state {
    float temperature{22}, sunlight{0.8f}, sun_angle{0.7f};
    bool seasons{true};
    double rain_until{};
    float opening{.15f}, room_exposure{.55f}, ambient_humidity{.5f};
    bool day_night{true};
};
struct population_sample {
    double day{};
    std::array<int, 3> population{};
    int plants{};
};
struct journal_entry {
    double day{};
    std::string message;
};

class world_state {
  public:
    explicit world_state(std::uint32_t seed = 1402, bool populate = true);
    bool construct(const garden_design&);
    const garden_design& design() const { return design_; }
    const physics::vessel_parameters& physical_parameters() const { return parameters_; }
    double soil_cell_capacity() const {
        return design_.form == vessel_form::legacy
                   ? water_state::cell_capacity
                   : design_.soil_water_capacity() / (soil_width * soil_width);
    }
    double reservoir_capacity() const { return design_.reservoir_capacity(); }
    void step();
    void advance(std::uint64_t ticks);
    bool add_plant(plant_kind kind, vec2 position);
    entity_id add_creature(species_kind species, vec2 position);
    bool can_add_plant(plant_kind kind, vec2 position) const;
    bool can_add_creature(species_kind species, vec2 position) const;
    void rain(double days = 0.45);
    void water(vec2 position, double amount = 0.3);
    std::string name(entity_id id) const;
    bool rename(entity_id id, const std::string& name);
    bool toggle_favourite(entity_id id);
    const water_state& water_cycle() const { return water_; }
    double water_balance() const;
    matter_reading matter_sample() const;
    matter_amount matter_balance() const;
    const matter_account& matter_ledger() const { return matter_; }
    const std::deque<matter_reading>& matter_history() const { return matter_history_; }
    double mineral_capacity() const;
    double root_nutrients(vec2 point) const;
    bool amend(vec2 point, amendment kind, double grams = .5);
    float pond_level() const;
    static float pond_radius_squared(vec2 position);
    static bool in_pond(vec2 position);
    double day() const { return static_cast<double>(tick_) / ticks_per_day; }
    std::uint64_t ticks() const { return tick_; }
    float temperature() const;
    float ambient_temperature() const;
    float daylight() const;
    float irradiance() const;
    double humidity() const { return physics::relative_humidity(vessel_, water_.vapour, parameters_); }
    const physics::vessel_state& vessel() const { return vessel_; }
    float light() const;
    float moisture() const;
    const char* season() const;
    std::array<int, 3> populations() const;
    const ancestor_record* ancestor(entity_id id) const;
    const creature_state* creature(entity_id id) const;
    const soil_cell& soil_at(vec2 position) const;
    const plant_state* plant(entity_id id) const;
    float light_at(vec2 position, entity_id exclude = 0) const;
    float shelter_at(vec2 position) const;
    plant_status plant_health(const plant_state& plant) const;
    std::string intention(const creature_state& creature) const;
    std::string last_meal(const creature_state& creature) const;
    const day_report& today() const { return report_; }
    int eggs() const;
    static float plant_height(const plant_state& plant);
    static float growth_scale(const plant_state& plant);
    const std::vector<creature_state>& creatures() const { return creatures_; }
    const std::vector<plant_state>& plants() const { return plants_; }
    const std::vector<ancestor_record>& family() const { return family_; }
    const std::deque<population_sample>& history() const { return history_; }
    const std::deque<journal_entry>& journal() const { return journal_; }
    bool save(const std::filesystem::path& path, std::string& error) const;
    bool load(const std::filesystem::path& path, std::string& error);
    std::uint64_t digest() const;

  public:
    static constexpr float radius = 4.65f;
    static constexpr int soil_width = 24;
    static constexpr std::uint64_t ticks_per_day = 1800;
    static constexpr double fixed_step = 1.0 / 30.0;
    static constexpr std::size_t max_creatures = 1200, max_plants = 320;

    inline static constexpr vec2 pond_center{1.35f, .75f}, pond_radii{1.2f, .9f};

    climate_state climate;

  private:
    void initialize_matter();
    void decompose(double dt);
    void deposit(vec2 point, matter_amount matter);
    void digest_food(creature_state& creature, matter_amount meal);
    double root_nitrogen(vec2 point, double take = 0);
    static matter_amount body_matter(const creature_state& creature);
    float random(float low = 0, float high = 1);
    vec2 random_position();
    int soil_index(vec2 p) const;
    void metabolize(creature_state& c);
    void incubate(creature_state& c);
    void age_creature(creature_state& c, float temp);
    void move_creature(creature_state& c);
    void remove_dead_creatures();
    void sample_history();
    void report_day();
    void grow_plant(plant_state& p, const plant_status& condition, float dt);
    void scatter_seed(plant_state& p);
    void remove_dead_plants();
    float inherit_trait(float maternal, float paternal);
    void reproduce(std::size_t i, std::size_t adults);
    void ecology();
    void hydrology(double dt);
    void microclimate();
    void receive_water(double kg);
    void behavior();
    void record(std::string message);
    entity_id spawn(species_kind species, vec2 position, entity_id mother, entity_id father,
                    inherited_traits genes);
    void write(std::ostream& out) const;
    bool read(std::istream& in);

  private:
    friend class world_reader;
    friend class creature_behavior;
    std::uint64_t tick_{};
    entity_id next_id_{1};
    std::mt19937 random_;
    std::vector<creature_state> creatures_;
    std::vector<plant_state> plants_;
    std::vector<ancestor_record> family_;
    std::array<soil_cell, soil_width * soil_width> soil_{};
    std::deque<population_sample> history_;
    std::deque<journal_entry> journal_;
    water_state water_;
    physics::vessel_state vessel_;
    garden_design design_{vessel_form::legacy};
    physics::vessel_parameters parameters_;
    day_report report_;
    matter_account matter_;
    std::deque<matter_reading> matter_history_;
};

}
