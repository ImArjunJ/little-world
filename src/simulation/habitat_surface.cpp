#include "habitat_surface.hpp"
#include <algorithm>
#include <numbers>

namespace terrarium::presentation {
float basin_depth(const garden_design& design) {
    return std::min(.018f, float(design.soil_depth) * .35f);
}
namespace {
float wet_extent_squared(const world_state& world) {
    float fill = std::clamp(world.pond_level(), 0.f, 1.f);
    return .5f - std::sin(std::asin(1 - 2 * fill) / 3);
}
}
float bed_offset(const world_state& world, vec2 point) {
    float recess = std::max(0.f, 1 - world_state::pond_radius_squared(point));
    return -basin_depth(world.design()) * recess * recess;
}
float water_offset(const world_state& world) {
    float recess = 1 - wet_extent_squared(world);
    return -basin_depth(world.design()) * recess * recess;
}
float water_radius(const world_state& world) {
    return std::sqrt(std::max(0.f, wet_extent_squared(world)));
}
float surface_offset(const world_state& world, vec2 point) {
    return world_state::in_pond(point) ? std::max(bed_offset(world, point), water_offset(world)) : 0;
}
std::optional<vec2> intersect_surface(const world_state& world, sengine::point origin,
                                      sengine::point direction) {
    if (direction.y >= -.00001f)
        return {};
    const float plane = -origin.y / direction.y;
    if (plane < 0)
        return {};
    const float end = (-basin_depth(world.design()) - origin.y) / direction.y;
    const float scale = float(world.design().radius()) * habitat_scale;
    auto point = [&](float t) {
        return vec2{(origin.x + direction.x * t) / scale, (origin.z + direction.z * t) / scale};
    };
    float low = plane, high = end;

    constexpr int segments = 32;
    for (int i = 1; i <= segments; ++i) {
        const float t = std::lerp(plane, end, float(i) / segments);
        if (origin.y + direction.y * t <= surface_offset(world, point(t))) {
            low = std::lerp(plane, end, float(i - 1) / segments);
            high = t;
            break;
        }
    }
    for (int i = 0; i < 22; ++i) {
        float mid = (low + high) * .5f;
        if (origin.y + direction.y * mid > surface_offset(world, point(mid)))
            low = mid;
        else
            high = mid;
    }
    auto result = point((low + high) * .5f);
    return std::hypot(result.x, result.y) <= world_state::radius * 1.05f ? std::optional(result)
                                                                         : std::nullopt;
}
surface_mesh make_surface_mesh(surface_part part) {
    constexpr unsigned sides = 96, rings = 20;
    constexpr float tau = 2 * std::numbers::pi_v<float>;
    const auto center = world_state::pond_center;
    const auto radii = world_state::pond_radii;
    surface_mesh mesh;
    for (unsigned ring = 0; ring <= rings; ++ring) {
        float v = float(ring) / rings;
        for (unsigned side = 0; side <= sides; ++side) {
            float angle = tau * side / sides, cx = std::cos(angle), cz = std::sin(angle);
            surface_vertex vertex{};
            if (part == surface_part::water) {
                vertex.position = {v * cx, 0, v * cz};
                vertex.normal = {0, 1, 0};
                vertex.uv = {v * cx, v * cz};
            } else {
                float dx = radii.x * habitat_scale * cx, dz = radii.y * habitat_scale * cz;
                const float x = center.x * habitat_scale, z = center.y * habitat_scale;
                if (part == surface_part::soil_cell) {
                    const float a = dx * dx + dz * dz, b = x * dx + z * dz;
                    const float t = (-b + std::sqrt(b * b + a * (.92f * .92f - x * x - z * z))) / a;
                    float along = std::lerp(1.f, t, v);
                    vertex.position = {x + dx * along, 0, z + dz * along};
                    vertex.normal = {0, 1, 0};
                } else {
                    float recess = 1 - v * v;
                    vertex.position = {x + dx * v, -recess * recess, z + dz * v};
                    float nx = -4 * v * recess * cx / (radii.x * habitat_scale);
                    float nz = -4 * v * recess * cz / (radii.y * habitat_scale);
                    float length = std::sqrt(nx * nx + 1 + nz * nz);
                    vertex.normal = {nx / length, 1 / length, nz / length};
                }
                vertex.uv = {vertex.position.x * .5f + .5f, vertex.position.z * .5f + .5f};
            }
            mesh.vertices.push_back(vertex);
        }
    }
    for (unsigned ring = 0; ring < rings; ++ring)
        for (unsigned side = 0; side < sides; ++side) {
            unsigned a = ring * (sides + 1) + side, b = a + sides + 1;
            mesh.indices.insert(mesh.indices.end(), {a, a + 1, b + 1, a, b + 1, b});
        }
    return mesh;
}
}
