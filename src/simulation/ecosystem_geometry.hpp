#pragma once
#include "ecosystem.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace terrarium::ecosystem_detail {
inline constexpr float day_step = 1.0f / world_state::ticks_per_day;
inline constexpr float pi = std::numbers::pi_v<float>;
inline constexpr float reproduction_energy = .98f;
inline vec2 bounded(vec2 p) {
    const float length = std::hypot(p.x, p.y);
    if (length > world_state::radius) {
        p.x *= world_state::radius / length;
        p.y *= world_state::radius / length;
    }
    return p;
}
inline vec2 dry_ground(vec2 p) {
    if (!world_state::in_pond(p))
        return p;
    const float radius = std::sqrt(world_state::pond_radius_squared(p));
    if (radius < .0001f)
        return {world_state::pond_center.x, world_state::pond_center.y - world_state::pond_radii.y * 1.03f};
    return {world_state::pond_center.x + (p.x - world_state::pond_center.x) * 1.03f / radius,
            world_state::pond_center.y + (p.y - world_state::pond_center.y) * 1.03f / radius};
}
}
