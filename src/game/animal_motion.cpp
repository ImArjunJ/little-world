#include "animal_motion.hpp"
#include "habitat_surface.hpp"
#include "tending_preview.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace terrarium::presentation {
animal_weights animal_gait(species_kind species, double phase, float activity) {
    constexpr float tau = 2 * std::numbers::pi_v<float>;
    const float cycle = float(phase - std::floor(phase));
    activity = std::clamp(activity, 0.f, 1.f);
    if (species == species_kind::snail)
        return {activity * std::cos(tau * cycle), activity * std::sin(tau * cycle),
                activity * .7f * std::sin(tau * cycle + .4f), activity * .7f * std::sin(tau * cycle + 2.f)};
    animal_weights weights{};
    for (unsigned group = 0; group < 2; ++group) {
        const float t = std::fmod(cycle + group * .5f, 1.f);
        if (t < .6f) {
            weights[group * 2] = activity * (1 - 2 * t / .6f);
        } else {
            const float swing = (t - .6f) / .4f;
            weights[group * 2] = activity * (-1 + 2 * swing * swing * (3 - 2 * swing));
            weights[group * 2 + 1] = activity * std::pow(std::sin(std::numbers::pi_v<float> * swing), 2);
        }
    }
    return weights;
}
void animal_motion::update(const world_state& world, std::string_view garden) {
    if (garden_ != garden || world.ticks() < tick_) {
        tracks_.clear();
        garden_ = garden;
        initialized_ = false;
    }
    if (initialized_ && tick_ == world.ticks())
        return;
    const double seconds = initialized_ ? (world.ticks() - tick_) * world_state::fixed_step : 0;
    const float response = float(1 - std::exp(-seconds * 18));
    const float metres = float(world.design().radius()) * habitat_scale;
    tick_ = world.ticks();
    initialized_ = true;
    for (const auto& creature : world.creatures()) {
        auto [it, inserted] = tracks_.try_emplace(creature.id, track{creature.position});
        auto& track = it->second;
        const float distance =
            std::hypot(creature.position.x - track.position.x, creature.position.y - track.position.y) *
            metres;
        const float length = creature_length(creature);
        if (!inserted && seconds > 0) {
            track.phase = std::fmod(track.phase + distance / (length * .3f), 1.0);
            track.activity = std::lerp(track.activity, distance > 1e-8f ? 1.f : 0.f, response);
        }
        track.position = creature.position;
        track.seen = tick_;
        track.weights = animal_gait(creature.species, track.phase, track.activity);
    }
    std::erase_if(tracks_, [&](const auto& entry) { return entry.second.seen != tick_; });
}
const animal_weights& animal_motion::weights(entity_id id) const {
    static constexpr animal_weights resting{};
    const auto it = tracks_.find(id);
    return it == tracks_.end() ? resting : it->second.weights;
}
}
