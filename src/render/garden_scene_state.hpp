#pragma once
#include "animal_motion.hpp"
#include "carry_pose.hpp"
#include "fauna.hpp"
#include "garden_scene.hpp"
#include "sengine/renderer.hpp"
#include "sengine/scene.hpp"
#include "substrate.hpp"
#include <algorithm>
#include <map>
#include <tuple>
namespace terrarium::render {
struct garden_scene_state {
    sengine::renderer graphics;
    sengine::scene resources;

  public:
    explicit garden_scene_state(sengine::window& window) : graphics(window), resources(graphics) {}

  public:
    std::unique_ptr<substrate_meshes> substrate;
    std::unique_ptr<fauna_meshes> fauna;
    sengine::material_id glass;
    sengine::scene_node sun;
    sengine::scene_asset asset, gardens, character;
    sengine::render_options options;
    std::unique_ptr<sengine::native_hud> hud;
    std::optional<std::tuple<int, bool, bool, bool>> settings;
    float near_plane{.045f};
    sengine::point player_head{};
    bool outside_body{};
    struct named {
        sengine::scene_node entity;
        sengine::mat4 bind;
        std::vector<sengine::scene_node> renderables{};
        bool visible{};
    };
    std::map<std::string, named> joints;
    std::array<sengine::scene_node, 10> jars{}, markers{};
    std::array<std::array<named, 4>, 10> vessels{};
    std::array<sengine::mat4, 10> jar_transforms{};
    std::array<named, 2> boots{};
    std::array<std::array<named, 320>, 10> plants{};
    std::array<std::array<std::array<named, 3>, 320>, 10> foliage{};
    std::array<unsigned, 10> previous_plants{};
    std::array<float, 3> plant_radius{};
    std::array<presentation::animal_motion, 10> animal_motion;
    std::array<named, 6> previews{};
    std::map<std::string, size_t> clips;
    size_t active_clip{}, previous_clip{};
    float clip_time{}, previous_clip_time{}, blend_time{1};
    bool clip_started{};
    std::array<sengine::mat4, 2> boot_attachments{};
};

struct carrying_pose {
    carry_pose pose;
    sengine::float3 held, grip;
    float blend{}, support{};
};
void collect(garden_scene_state&, garden_scene_state::named&, sengine::scene_node);
void show(garden_scene_state&, garden_scene_state::named&, bool);
void load_garden_scene(garden_scene_state&, const std::filesystem::path&, render_quality);
void update_gardens(garden_scene_state&, const greenhouse&, carrying_pose&);
void update_preview(garden_scene_state&, const greenhouse&, const std::optional<tending_preview>&);
void animate_gardener(garden_scene_state&, const terrarium::camera_pose&, double, const carrying_pose&);
inline sengine::float3 point(sengine::point p) {
    return {p.x, p.y, p.z};
}
inline float ease(float value) {
    value = std::clamp(value, 0.f, 1.f);
    return value * value * (3 - 2 * value);
}
}
