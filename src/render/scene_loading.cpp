#include "garden_scene_state.hpp"
#include <stdexcept>
namespace terrarium::render {
using namespace sengine;
namespace {
void configure_lighting(garden_scene_state& state, render_quality quality) {
    auto& scene = state.resources;
    state.options.temporal_aa = quality == render_quality::high;
    state.options.occlusion = quality == render_quality::high;
    state.options.soft_shadows = quality == render_quality::high;
    state.options.fog = {true, 55, .003f, 320, .035f, {.60f, .72f, .79f}};
    state.graphics.configure(state.options);
    state.graphics.visible_layers(0x01);
    sun_options sun;
    sun.color = {1, .91f, .76f};
    sun.intensity = 28000;
    sun.direction = {.42f, -.46f, .78f};
    sun.shadow_size = quality == render_quality::high ? 2048 : 1024;
    sun.cascades = quality == render_quality::high ? 3 : 2;
    sun.shadow_far = quality == render_quality::high ? 90.f : 45.f;
    state.sun = add_sun(scene, sun);
    set_environment(scene, {});
}
void load_greenhouse(garden_scene_state& state, const std::filesystem::path& path) {
    auto& scene = state.resources;
    state.asset = load_scene(scene, path);
    for (auto entity : nodes(scene, state.asset)) {
        const auto label = node_name(scene, state.asset, entity);
        const char* name = label.c_str();
        if (name && std::string_view(name).starts_with("Glazing")) {
            auto instance = entity;
            if (renderable(scene, instance))
                cast_shadows(scene, instance, false);
        }
    }
}
void bind_garden_nodes(garden_scene_state& state) {
    auto& scene = state.resources;
    for (auto entity : nodes(scene, state.gardens)) {
        const auto label = node_name(scene, state.gardens, entity);
        const char* name = label.c_str();
        if (label.empty())
            continue;
        int a, b, c;
        if (std::string_view(name) == "Gardener boot left" || std::string_view(name) == "Gardener boot right")
            state.boots[std::string_view(name).ends_with("left") ? 0 : 1] = {entity,
                                                                             local_transform(scene, entity)};
        if (std::sscanf(name, "Placement %d", &a) == 1 && a >= 0 && a < 10)
            state.markers[a] = entity;
        if (std::sscanf(name, "Garden %d", &a) == 1 && a >= 0 && a < 10)
            state.jars[a] = entity;
        if (std::sscanf(name, "Plant %d %d", &a, &b) == 2 && a >= 0 && a < 10 && b >= 0 && b < 320)
            state.plants[a][b] = {entity, local_transform(scene, entity)};
        if (std::sscanf(name, "Foliage %d %d %d", &a, &b, &c) == 3 && a >= 0 && a < 10 && b >= 0 && b < 320 &&
            c >= 0 && c < 3)
            state.foliage[a][b][c] = {entity, local_transform(scene, entity)};
        if (std::sscanf(name, "Preview %d", &a) == 1 && a >= 0 && a < 6)
            state.previews[a] = {entity, local_transform(scene, entity)};
        if (std::sscanf(name, "Vessel %d %d", &a, &b) == 2 && a >= 0 && a < 10 && b >= 0 && b < 4) {
            state.vessels[a][b] = {entity, local_transform(scene, entity)};
            auto r = entity;
            if (renderable(scene, r)) {
                cast_shadows(scene, r, false);
                if (b == 0 || b == 3)
                    set_material(scene, r, state.glass, 0);
            }
        }
    }
}
void prepare_visibility(garden_scene_state& state) {
    auto& scene = state.resources;
    for (unsigned i = 0; i < 10; ++i) {
        for (auto& variants : state.foliage[i])
            for (auto& n : variants)
                collect(state, n, n.entity);
        for (auto& n : state.vessels[i])
            collect(state, n, n.entity);
        visible(scene, state.markers[i], true);
    }
    for (auto& n : state.previews) {
        collect(state, n, n.entity);
        for (auto entity : n.renderables)
            cast_shadows(scene, entity, false);
    }
}
void measure_canopies(garden_scene_state& state) {
    auto& scene = state.resources;
    for (unsigned kind = 0; kind < state.plant_radius.size(); ++kind) {
        for (auto entity : state.foliage[0][0][kind].renderables) {
            const auto bounds = node_bounds(scene, entity);
            const auto transform = world_transform(scene, entity);
            for (int x : {-1, 1})
                for (int y : {-1, 1})
                    for (int z : {-1, 1}) {
                        const auto corner =
                            transform *
                            sengine::float4(bounds.center + bounds.half_extent *
                                                                sengine::float3{float(x), float(y), float(z)},
                                            1);
                        state.plant_radius[kind] =
                            std::max(state.plant_radius[kind], std::hypot(corner.x, corner.z));
                    }
        }
        if (!std::isfinite(state.plant_radius[kind]) || state.plant_radius[kind] <= 0)
            throw std::runtime_error("Imported plant canopy has invalid bounds");
    }
}
void load_character(garden_scene_state& state, const std::filesystem::path& path) {
    auto& scene = state.resources;
    state.character = load_scene(scene, path.parent_path() / "gardener.glb");
    for (auto entity : nodes(scene, state.character)) {
        const auto label = node_name(scene, state.character, entity);
        const char* name = label.c_str();
        auto t = entity;
        if (!label.empty() && t)
            state.joints[name] = {entity, local_transform(scene, t)};
        if (name &&
            (std::string_view(name) == "First person hidden head" || std::string_view(name) == "Eyebrows" ||
             std::string_view(name) == "Eyes" || std::string_view(name) == "Hair_Buns")) {
            auto r = entity;
            if (renderable(scene, r))
                layers(scene, r, 0x02);
        }
    }
}
void bind_animation(garden_scene_state& state) {
    auto& scene = state.resources;
    const auto clips = animation_names(scene, state.character);
    for (size_t i = 0; i < clips.size(); ++i)
        state.clips[clips[i]] = i;
    for (auto name : {"Idle_Loop", "Walk_Loop", "Jog_Fwd_Loop", "Sprint_Loop", "Crouch_Idle_Loop",
                      "Crouch_Fwd_Loop", "Jump_Loop"})
        if (!state.clips.contains(name))
            throw std::runtime_error(std::string("Missing gardener animation: ") + name);
    for (unsigned i = 0; i < 2; ++i) {
        auto foot = world_transform(scene, state.joints.at(i ? "foot_r" : "foot_l").entity);
        state.boot_attachments[i] = inverse(foot) *
                                    sengine::translation(sengine::float3{foot[3].x, 0, foot[3].z}) *
                                    state.boots[i].bind;
    }
}
}
void collect(garden_scene_state& state, garden_scene_state::named& group, scene_node entity) {
    group.renderables = renderable_descendants(state.resources, entity);
}
void show(garden_scene_state& state, garden_scene_state::named& group, bool enabled) {
    if (group.visible == enabled)
        return;
    group.visible = enabled;
    visible(state.resources, group.renderables, enabled);
}
void load_garden_scene(garden_scene_state& state, const std::filesystem::path& path, render_quality quality) {
    auto& scene = state.resources;
    configure_lighting(state, quality);
    load_greenhouse(state, path);
    state.gardens = load_scene(scene, path.parent_path() / "gardens.glb", false);
    state.fauna = std::make_unique<fauna_meshes>(
        scene, load_scene(scene, path.parent_path() / "fauna.glb", false), path.parent_path());
    state.glass = load_material(scene, path.parent_path() / "glass.filamat");
    bind_garden_nodes(state);
    prepare_visibility(state);
    state.substrate = std::make_unique<substrate_meshes>(
        scene, material_at(scene, state.vessels[0][1].entity, 0), path.parent_path() / "water.filamat");
    measure_canopies(state);
    for (auto& n : state.boots) {
        collect(state, n, n.entity);
        show(state, n, true);
    }
    load_character(state, path);
    bind_animation(state);
    state.hud = std::make_unique<sengine::native_hud>(state.graphics, path.parent_path() / "hud.filamat",
                                                      path.parent_path().parent_path() / "fonts/Body.ttf",
                                                      path.parent_path().parent_path() / "fonts/Display.ttf");
}
}
