#pragma once
#include "ecosystem.hpp"

namespace terrarium {
enum class garden_tool { inspect, fern, clover, flower, aphid, snail, ladybird, water, compost, mulch };
enum class placement_problem { none, outside, basin, plants_full, creatures_full };
struct tending_preview {
    garden_tool tool;
    vec2 position;
    placement_problem problem{};

  public:
    bool allowed() const { return problem == placement_problem::none; }
    const char* hint() const;
};
tending_preview placement_preview(const world_state&, garden_tool, vec2);
float creature_length(const creature_state&);
}
