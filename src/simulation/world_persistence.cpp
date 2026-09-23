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

#include "ecosystem_geometry.hpp"
namespace terrarium {
using namespace ecosystem_detail;
namespace {
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
class world_reader {
  public:
    world_reader(world_state& world, std::istream& in) : world(world), in(in) {}
    bool read() {
        return read_header() && read_soil() && read_plants() && read_creatures() && read_family() &&
               read_ancestry() && read_history() && read_journal() && read_water() && read_report() &&
               read_vessel() && read_design() && read_matter() && read_finish();
    }

  private:
    world_state& world;
    std::istream& in;
    int version{};
    std::unordered_set<entity_id> live_ids;

  private:
    bool read_header() {
        in.imbue(std::locale::classic());
        std::string magic;
        if (!(in >> magic >> version) || magic != "LITTLE_WORLD" || (version < 1 || version > 6))
            return false;
        if (!(in >> world.tick_ >> world.next_id_ >> world.random_))
            return false;
        if (world.tick_ > 1000000000000ULL || world.next_id_ == 0)
            return false;
        in >> world.climate.temperature >> world.climate.sunlight >> world.climate.sun_angle >>
            world.climate.seasons >> world.climate.rain_until;
        if (!range(world.climate.temperature, 0, 45) || !range(world.climate.sunlight, 0, 1) ||
            !range(world.climate.sun_angle, -10, 10) || !std::isfinite(world.climate.rain_until) ||
            world.climate.rain_until < 0)
            return false;
        return true;
    }
    bool read_soil() {
        for (auto& s : world.soil_) {
            in >> s.water >> s.nutrients;
            if (version >= 2)
                in >> s.litter;
            if (!range(s.water, 0, 1) || !range(s.nutrients, 0, version >= 6 ? 1e6 : 1) ||
                !range(s.litter, 0, version >= 6 ? 1e6 : 1))
                return false;
        }
        return true;
    }
    bool read_plants() {
        std::size_t n{};
        if (!(in >> n) || n > world_state::max_plants)
            return false;
        world.plants_.resize(n);
        for (auto& p : world.plants_) {
            int kind{};
            in >> p.id >> kind >> p.position.x >> p.position.y >> p.biomass >> p.age >> p.seed_cooldown >>
                p.shape;
            if (version >= 3)
                in >> p.grazing >> p.parent;
            if (!range(p.grazing, 0, 16) || p.parent >= p.id)
                return false;
            if (kind < 0 || kind > 2 || p.id == 0 || p.id >= world.next_id_ ||
                !live_ids.insert(p.id).second || !valid_position(p.position) || !range(p.biomass, 0, 1.6f) ||
                !range(p.age, 0, 1e9f) || !range(p.seed_cooldown, -1e9f, 10))
                return false;
            p.kind = static_cast<plant_kind>(kind);
        }
        return true;
    }
    bool read_creatures() {
        std::size_t n{};
        if (!(in >> n) || n > world_state::max_creatures)
            return false;
        world.creatures_.resize(n);
        for (auto& c : world.creatures_) {
            int species{};
            in >> c.id >> species >> c.position.x >> c.position.y >> c.target.x >> c.target.y >> c.heading >>
                c.age >> c.energy >> c.reproduction_cooldown >> c.genes;
            int activity = 0;
            if (version >= 2)
                in >> activity >> c.activity_time >> c.meal_cooldown;
            if (activity < 0 || activity > 6 || !range(c.activity_time, 0, 10) ||
                !range(c.meal_cooldown, 0, 10))
                return false;
            c.activity = static_cast<creature_activity>(activity);
            int meal = 0, cause = 0;
            if (version >= 3)
                in >> c.incubation >> c.protein >> c.attention >> meal >> c.last_meal >> cause >> c.predator;
            if (!range(c.incubation, 0, 2) || !range(c.protein, 0, 1) || c.attention >= world.next_id_ ||
                meal < 0 || meal > 4 || !std::isfinite(c.last_meal) || c.last_meal < -1 ||
                c.last_meal > world.day() || cause < 0 || cause > 5 || c.predator >= world.next_id_)
                return false;
            c.meal = static_cast<food_kind>(meal);
            c.death_cause = static_cast<death_cause>(cause);
            if (species < 0 || species > 2 || c.id == 0 || c.id >= world.next_id_ ||
                !live_ids.insert(c.id).second || !valid_position(c.position) || !valid_position(c.target) ||
                !range(c.heading, -7, 7) || !range(c.age, 0, 1e9f) || !range(c.energy, 0, 1.6f) ||
                !range(c.reproduction_cooldown, -1e9f, 30) || !valid_genes(c.genes))
                return false;
            c.species = static_cast<species_kind>(species);
        }
        return true;
    }
    bool read_family() {
        std::size_t n{};
        if (!(in >> n) || n > 10000000)
            return false;
        world.family_.resize(n);
        entity_id previous = 0;
        for (auto& a : world.family_) {
            int species{};
            in >> a.id >> a.mother >> a.father >> species >> a.generation >> a.born >> a.died >> a.genes;
            if (version >= 2)
                in >> std::quoted(a.nickname) >> a.favourite;
            int cause = 0;
            if (version >= 3)
                in >> cause >> a.predator;
            if (cause < 0 || cause > 5 || a.predator >= world.next_id_)
                return false;
            a.death_cause = static_cast<death_cause>(cause);
            if (a.nickname.size() > 64 || std::any_of(a.nickname.begin(), a.nickname.end(),
                                                      [](unsigned char c) { return c < 32 || c == 127; }))
                return false;
            if (a.id <= previous || a.id >= world.next_id_ || a.mother >= a.id || a.father >= a.id ||
                species < 0 || species > 2 || a.generation < 0 || a.generation > 1000000 ||
                !valid_genes(a.genes) || !std::isfinite(a.born) || !std::isfinite(a.died) || a.born < 0 ||
                a.born > world.day() || (a.died != -1 && (a.died < a.born || a.died > world.day())))
                return false;
            previous = a.id;
            a.species = static_cast<species_kind>(species);
        }
        return true;
    }
    bool read_ancestry() {
        for (const auto& a : world.family_) {
            const auto* mother = world.ancestor(a.mother);
            const auto* father = world.ancestor(a.father);
            if (a.mother == 0 && a.father == 0) {
                if (a.generation != 0)
                    return false;
            } else if (!mother || !father || mother->species != a.species || father->species != a.species ||
                       a.generation != 1 + std::max(mother->generation, father->generation))
                return false;
            if ((a.died == -1) != (world.creature(a.id) != nullptr))
                return false;
        }
        for (const auto& c : world.creatures_) {
            const auto* a = world.ancestor(c.id);
            if (!a || a->species != c.species)
                return false;
        }
        return true;
    }
    bool read_history() {
        std::size_t n{};
        if (!(in >> n) || n > 240)
            return false;
        world.history_.resize(n);
        double previous_sample_day = -1;
        for (auto& s : world.history_) {
            in >> s.day >> s.population[0] >> s.population[1] >> s.population[2] >> s.plants;
            if (!std::isfinite(s.day) || s.day < 0 || s.day > world.day() || s.day <= previous_sample_day ||
                s.plants < 0 || s.plants > static_cast<int>(world_state::max_plants))
                return false;
            previous_sample_day = s.day;
            int total_population = 0;
            for (int p : s.population)
                if (p < 0 || p > static_cast<int>(world_state::max_creatures))
                    return false;
                else
                    total_population += p;
            if (total_population > static_cast<int>(world_state::max_creatures))
                return false;
        }
        return true;
    }
    bool read_journal() {
        std::size_t n{};
        if (!(in >> n) || n > 32)
            return false;
        world.journal_.resize(n);
        for (auto& e : world.journal_) {
            in >> e.day >> std::quoted(e.message);
            if (!std::isfinite(e.day) || e.day < 0 || e.day > world.day() || e.message.size() > 1024)
                return false;
        }
        return true;
    }
    bool read_water() {
        if (version >= 2) {
            in >> world.water_.pond >> world.water_.vapour >> world.water_.added >> world.water_.escaped >>
                world.water_.overflow;
            for (double value : {world.water_.pond, world.water_.vapour, world.water_.added,
                                 world.water_.escaped, world.water_.overflow})
                if (!std::isfinite(value) || value < 0)
                    return false;
        }
        return true;
    }
    bool read_report() {
        world.report_ = {};
        if (version >= 3) {
            in >> world.report_.born >> world.report_.hatched >> world.report_.hunted >>
                world.report_.starved >> world.report_.weather_deaths >> world.report_.old_age >>
                world.report_.seedlings >> world.report_.compost;
            for (int count :
                 {world.report_.born, world.report_.hatched, world.report_.hunted, world.report_.starved,
                  world.report_.weather_deaths, world.report_.old_age, world.report_.seedlings})
                if (count < 0 || count > 10000000)
                    return false;
            if (!std::isfinite(world.report_.compost) || world.report_.compost < 0 ||
                world.report_.compost > 10000000)
                return false;
        }
        return true;
    }
    bool read_vessel() {
        if (version >= 4) {
            in >> world.climate.opening >> world.climate.room_exposure >> world.climate.ambient_humidity >>
                world.climate.day_night;
            in >> world.vessel_.body_c >> world.vessel_.glass_c >> world.vessel_.film_kg >>
                world.vessel_.external_j >> world.vessel_.evaporated_kg >> world.vessel_.condensed_kg >>
                world.vessel_.air_in_kg >> world.vessel_.air_out_kg;
            if (!range(world.climate.opening, 0, 1) || !range(world.climate.room_exposure, 0, 1) ||
                !range(world.climate.ambient_humidity, 0, 1) || !physics::valid(world.vessel_) ||
                world.water_.vapour > .1)
                return false;
        } else {
            world.vessel_ = {};
            world.vessel_.body_c = world.vessel_.glass_c = world.ambient_temperature();

            const double capacity =
                physics::saturation_density(world.vessel_.body_c) * physics::vessel_parameters{}.air_volume;
            world.vessel_.film_kg = std::max(0., world.water_.vapour - capacity * .75);
            world.water_.vapour -= world.vessel_.film_kg;
            if (world.vessel_.film_kg > 100)
                return false;
        }
        return true;
    }
    bool read_design() {
        world.design_ = garden_design{vessel_form::legacy};
        if (version >= 5) {
            int form{}, mix{};
            in >> form >> mix >> world.design_.soil_depth >> world.design_.drainage_depth;
            world.design_.form = static_cast<vessel_form>(form);
            world.design_.mix = static_cast<soil_mix>(mix);
            if (!world.design_.valid())
                return false;
        }
        world.parameters_ = world.design_.parameters();
        if (world.water_.pond > world.reservoir_capacity())
            return false;
        return true;
    }
    static bool valid_mass(double value) { return std::isfinite(value) && value >= 0 && value <= 1e15; }
    bool read_matter() {
        std::size_t n{};
        if (version >= 6) {
            std::string tag;
            int matter_version{};
            if (!(in >> tag >> matter_version) || tag != "MATTER" || matter_version != 1)
                return false;
            in >> world.matter_.initial.carbon >> world.matter_.initial.nitrogen >>
                world.matter_.introduced.carbon >> world.matter_.introduced.nitrogen >>
                world.matter_.fixed_carbon >> world.matter_.respired_carbon >>
                world.report_.mineralized_nitrogen >> world.report_.immobilized_nitrogen;
            for (double value : {world.matter_.initial.carbon, world.matter_.initial.nitrogen,
                                 world.matter_.introduced.carbon, world.matter_.introduced.nitrogen,
                                 world.matter_.fixed_carbon, world.matter_.respired_carbon,
                                 world.report_.mineralized_nitrogen, world.report_.immobilized_nitrogen})
                if (!valid_mass(value))
                    return false;
            for (auto& soil : world.soil_) {
                in >> soil.litter_nitrogen >> soil.microbial_nitrogen;
                if (!valid_mass(soil.litter_nitrogen) || !valid_mass(soil.microbial_nitrogen))
                    return false;
            }
            if (!(in >> n) || n != world.creatures_.size())
                return false;
            for (auto& c : world.creatures_) {
                entity_id id{};
                in >> id >> c.matter.carbon >> c.matter.nitrogen;
                if (id != c.id || !valid_mass(c.matter.carbon) || !valid_mass(c.matter.nitrogen))
                    return false;
            }
            if (!(in >> n) || n != world.plants_.size())
                return false;
            for (auto& p : world.plants_) {
                entity_id id{};
                in >> id >> p.nectar_carbon;
                if (id != p.id || !range(p.nectar_carbon, 0, 4) ||
                    (p.kind != plant_kind::flower && p.nectar_carbon != 0))
                    return false;
            }
            if (!(in >> n) || n > 240)
                return false;
            world.matter_history_.resize(n);
            double previous = -1;
            for (auto& sample : world.matter_history_) {
                in >> sample.day;
                for (double& value : sample.nitrogen) {
                    in >> value;
                    if (!valid_mass(value))
                        return false;
                }
                in >> sample.carbon;
                if (!valid_mass(sample.carbon) || !std::isfinite(sample.day) || sample.day < 0 ||
                    sample.day > world.day() || sample.day <= previous)
                    return false;
                previous = sample.day;
            }
        } else
            world.initialize_matter();
        return true;
    }
    bool read_finish() {
        if (!in)
            return false;
        in >> std::ws;
        return in.eof();
    }
};

bool world_state::read(std::istream& in) {
    return world_reader(*this, in).read();
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
