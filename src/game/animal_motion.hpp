#pragma once
#include "ecosystem.hpp"
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

namespace terrarium::presentation {
using animal_weights = std::array<float, 4>;
animal_weights animal_gait(species_kind, double phase, float activity);

class animal_motion {
  public:
    void update(const world_state&, std::string_view garden);
    const animal_weights& weights(entity_id) const;
    size_t size() const { return tracks_.size(); }

  private:
    struct track {
        vec2 position;
        double phase{};
        float activity{};
        std::uint64_t seen{};
        animal_weights weights{};
    };
    std::string garden_;
    std::uint64_t tick_{};
    bool initialized_{};
    std::unordered_map<entity_id, track> tracks_;
};
}
