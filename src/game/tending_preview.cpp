#include "tending_preview.hpp"
#include <algorithm>
#include <cmath>

namespace terrarium {
tending_preview placement_preview(const world_state& world, garden_tool tool, vec2 position) {
    tending_preview preview{tool, position};
    const float radius = std::hypot(position.x, position.y);
    if (!std::isfinite(radius) || radius > world_state::radius + .01f)
        preview.problem = placement_problem::outside;
    else if (tool >= garden_tool::fern && tool <= garden_tool::flower) {
        if (!world.can_add_plant(plant_kind(int(tool) - 1), position))
            preview.problem = world.plants().size() >= world_state::max_plants
                                  ? placement_problem::plants_full
                                  : placement_problem::basin;
    } else if (tool >= garden_tool::aphid && tool <= garden_tool::ladybird) {
        if (!world.can_add_creature(species_kind(int(tool) - 4), position))
            preview.problem = world.creatures().size() >= world_state::max_creatures
                                  ? placement_problem::creatures_full
                                  : placement_problem::basin;
    } else if (tool == garden_tool::compost || tool == garden_tool::mulch) {
        if (radius > world_state::radius)
            preview.problem = placement_problem::outside;
        else if (world_state::in_pond(position))
            preview.problem = placement_problem::basin;
    }
    return preview;
}
const char* tending_preview::hint() const {
    switch (problem) {
    case placement_problem::outside:
        return "Choose a spot inside the glass";
    case placement_problem::basin:
        return "Choose soil outside the basin";
    case placement_problem::plants_full:
        return "This garden is full of plants";
    case placement_problem::creatures_full:
        return "This garden is full of creatures";
    case placement_problem::none:
        break;
    }
    if (tool == garden_tool::water)
        return "Hold to water";
    if (tool == garden_tool::compost)
        return "Click to add compost";
    if (tool == garden_tool::mulch)
        return "Click to add mulch";
    return tool <= garden_tool::flower ? "Click to plant" : "Click to introduce";
}
float creature_length(const creature_state& creature) {
    const float adult = creature.species == species_kind::snail      ? .045f
                        : creature.species == species_kind::ladybird ? .022f
                                                                     : .018f;
    const float growth = std::sqrt(std::clamp(creature.age / maturity_age(creature.species), 0.f, 1.f));
    return adult * creature.genes.size * (.55f + .45f * growth);
}
}
