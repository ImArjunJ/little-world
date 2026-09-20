#pragma once
#include "ecosystem.hpp"
#include "sengine/geometry.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace terrarium::presentation {
enum class surface_part { soil_cell, basin, water };
struct surface_vertex {
    sengine::point position, normal;
    vec2 uv;
};
struct surface_mesh {
    std::vector<surface_vertex> vertices;
    std::vector<std::uint32_t> indices;
};
inline constexpr float habitat_scale = .8f / world_state::radius;
surface_mesh make_surface_mesh(surface_part);
float basin_depth(const garden_design&);
float bed_offset(const world_state&, vec2);
float water_offset(const world_state&);
float water_radius(const world_state&);
float surface_offset(const world_state&, vec2);

std::optional<vec2> intersect_surface(const world_state&, sengine::point origin, sengine::point direction);
}
