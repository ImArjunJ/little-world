#pragma once
#include "animal_motion.hpp"
#include "carry_pose.hpp"
#include "fauna.hpp"
#include "garden_scene.hpp"
#include "gardener.hpp"
#include "sengine/renderer.hpp"
#include "substrate.hpp"
#include <algorithm>
#include <tuple>

namespace terrarium::render {
struct posed_node {
    sengine::model_node node;
    sengine::mat4 bind;
};
struct plant_nodes {
    posed_node root;
    std::array<sengine::model_node, 3> foliage;
};
struct garden_plot {
    sengine::model_node root, marker;
    std::array<sengine::model_node, 4> vessels;
    std::vector<plant_nodes> plants;
    sengine::mat4 transform;
    std::size_t visible_plants{};
    presentation::animal_motion motion;
};
struct garden_scene_state {
  public:
    explicit garden_scene_state(sengine::window& window) : graphics(window), resources(graphics) {}

  public:
    sengine::renderer graphics;
    sengine::scene resources;
    std::unique_ptr<sengine::model_instance> environment, gardens;
    std::unique_ptr<substrate_meshes> substrate;
    std::unique_ptr<fauna_meshes> fauna;
    std::unique_ptr<gardener> character;
    sengine::scene_node sun;
    sengine::render_options options;
    std::unique_ptr<sengine::native_hud> hud;
    std::optional<std::tuple<int, bool, bool, bool>> settings;
    float near_plane{.045f};
    sengine::point player_head{};
    bool outside_body{};
    std::vector<garden_plot> plots;
    std::array<float, 3> plant_radius{};
    std::array<posed_node, 6> previews;
};
struct carrying_pose {
    carry_pose pose;
    sengine::float3 held, grip;
    float blend{}, support{};
};
void load_garden_scene(garden_scene_state&, const std::filesystem::path&, render_quality);
void update_gardens(garden_scene_state&, const greenhouse&, carrying_pose&);
void update_preview(garden_scene_state&, const greenhouse&, const std::optional<tending_preview>&);
inline sengine::float3 point(sengine::point p) {
    return {p.x, p.y, p.z};
}
inline float ease(float value) {
    value = std::clamp(value, 0.f, 1.f);
    return value * value * (3 - 2 * value);
}
}
