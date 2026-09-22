#include "render/garden_scene.hpp"
#include "animal_motion.hpp"
#include "carry_pose.hpp"
#include "habitat_surface.hpp"
#include "render/fauna.hpp"
#include "render/substrate.hpp"
#include "sengine/renderer.hpp"
#include "sengine/scene.hpp"
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace terrarium::render {
using namespace sengine;
struct garden_scene::impl {
    sengine::renderer graphics;
    sengine::scene resources;
    explicit impl(sengine::window& window) : graphics(window), resources(graphics) {}
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
    void collect(named& n, sengine::scene_node entity) {
        n.renderables = renderable_descendants(resources, entity);
    }
    void show(named& n, bool enabled) {
        if (n.visible == enabled)
            return;
        n.visible = enabled;
        visible(resources, n.renderables, enabled);
    }
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

garden_scene::garden_scene(sengine::window& window, const std::filesystem::path& path, render_quality quality)
    : impl_(std::make_unique<impl>(window)) {
    auto& p = *impl_;
    auto& scene = p.resources;
    p.options.temporal_aa = quality == render_quality::high;
    p.options.occlusion = quality == render_quality::high;
    p.options.soft_shadows = quality == render_quality::high;
    p.options.fog = {true, 55, .003f, 320, .035f, {.60f, .72f, .79f}};
    p.graphics.configure(p.options);
    p.graphics.visible_layers(0x01);
    sun_options sun;
    sun.color = {1, .91f, .76f};
    sun.intensity = 28000;
    sun.direction = {.42f, -.46f, .78f};
    sun.shadow_size = quality == render_quality::high ? 2048 : 1024;
    sun.cascades = quality == render_quality::high ? 3 : 2;
    sun.shadow_far = quality == render_quality::high ? 90.f : 45.f;
    p.sun = add_sun(scene, sun);
    set_environment(scene, {});
    p.asset = load_scene(scene, path);
    for (auto entity : nodes(scene, p.asset)) {
        const auto label = node_name(scene, p.asset, entity);
        const char* name = label.c_str();
        if (name && std::string_view(name).starts_with("Glazing")) {
            auto instance = entity;
            if (renderable(scene, instance))
                cast_shadows(scene, instance, false);
        }
    }
    p.gardens = load_scene(scene, path.parent_path() / "gardens.glb", false);
    p.fauna = std::make_unique<fauna_meshes>(
        scene, load_scene(scene, path.parent_path() / "fauna.glb", false), path.parent_path());
    p.glass = load_material(scene, path.parent_path() / "glass");
    for (auto entity : nodes(scene, p.gardens)) {
        const auto label = node_name(scene, p.gardens, entity);
        const char* name = label.c_str();
        if (label.empty())
            continue;
        int a, b, c;
        if (std::string_view(name) == "Gardener boot left" || std::string_view(name) == "Gardener boot right")
            p.boots[std::string_view(name).ends_with("left") ? 0 : 1] = {entity,
                                                                         local_transform(scene, entity)};
        if (std::sscanf(name, "Placement %d", &a) == 1 && a >= 0 && a < 10)
            p.markers[a] = entity;
        if (std::sscanf(name, "Garden %d", &a) == 1 && a >= 0 && a < 10)
            p.jars[a] = entity;
        if (std::sscanf(name, "Plant %d %d", &a, &b) == 2 && a >= 0 && a < 10 && b >= 0 && b < 320)
            p.plants[a][b] = {entity, local_transform(scene, entity)};
        if (std::sscanf(name, "Foliage %d %d %d", &a, &b, &c) == 3 && a >= 0 && a < 10 && b >= 0 && b < 320 &&
            c >= 0 && c < 3)
            p.foliage[a][b][c] = {entity, local_transform(scene, entity)};
        if (std::sscanf(name, "Preview %d", &a) == 1 && a >= 0 && a < 6)
            p.previews[a] = {entity, local_transform(scene, entity)};
        if (std::sscanf(name, "Vessel %d %d", &a, &b) == 2 && a >= 0 && a < 10 && b >= 0 && b < 4) {
            p.vessels[a][b] = {entity, local_transform(scene, entity)};
            auto r = entity;
            if (renderable(scene, r)) {
                cast_shadows(scene, r, false);
                if (b == 0 || b == 3)
                    set_material(scene, r, p.glass, 0);
            }
        }
    }
    for (unsigned i = 0; i < 10; ++i) {
        for (auto& variants : p.foliage[i])
            for (auto& n : variants)
                p.collect(n, n.entity);
        for (auto& n : p.vessels[i])
            p.collect(n, n.entity);
        visible(scene, p.markers[i], true);
    }
    for (auto& n : p.previews) {
        p.collect(n, n.entity);
        for (auto entity : n.renderables)
            cast_shadows(scene, entity, false);
    }
    p.substrate = std::make_unique<substrate_meshes>(scene, material_at(scene, p.vessels[0][1].entity, 0),
                                                     path.parent_path() / "water");
    for (unsigned kind = 0; kind < p.plant_radius.size(); ++kind) {
        for (auto entity : p.foliage[0][0][kind].renderables) {
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
                        p.plant_radius[kind] = std::max(p.plant_radius[kind], std::hypot(corner.x, corner.z));
                    }
        }
        if (!std::isfinite(p.plant_radius[kind]) || p.plant_radius[kind] <= 0)
            throw std::runtime_error("Imported plant canopy has invalid bounds");
    }
    for (auto& n : p.boots) {
        p.collect(n, n.entity);
        p.show(n, true);
    }
    p.character = load_scene(scene, path.parent_path() / "gardener.glb");
    for (auto entity : nodes(scene, p.character)) {
        const auto label = node_name(scene, p.character, entity);
        const char* name = label.c_str();
        auto t = entity;
        if (!label.empty() && t)
            p.joints[name] = {entity, local_transform(scene, t)};
        if (name &&
            (std::string_view(name) == "First person hidden head" || std::string_view(name) == "Eyebrows" ||
             std::string_view(name) == "Eyes" || std::string_view(name) == "Hair_Buns")) {
            auto r = entity;
            if (renderable(scene, r))
                layers(scene, r, 0x02);
        }
    }
    const auto clips = animation_names(scene, p.character);
    for (size_t i = 0; i < clips.size(); ++i)
        p.clips[clips[i]] = i;
    for (auto name : {"Idle_Loop", "Walk_Loop", "Jog_Fwd_Loop", "Sprint_Loop", "Crouch_Idle_Loop",
                      "Crouch_Fwd_Loop", "Jump_Loop"})
        if (!p.clips.contains(name))
            throw std::runtime_error(std::string("Missing gardener animation: ") + name);
    for (unsigned i = 0; i < 2; ++i) {
        auto foot = world_transform(scene, p.joints.at(i ? "foot_r" : "foot_l").entity);
        p.boot_attachments[i] =
            inverse(foot) * sengine::translation(sengine::float3{foot[3].x, 0, foot[3].z}) * p.boots[i].bind;
    }
    p.hud = std::make_unique<sengine::native_hud>(p.graphics, path.parent_path() / "hud",
                                                  path.parent_path().parent_path() / "fonts/Body.ttf");
}
garden_scene::~garden_scene() = default;

sengine::native_hud& garden_scene::hud() {
    return *impl_->hud;
}
void garden_scene::configure(int detail, bool occlusion, bool soft_focus, bool inspecting,
                             float focus_distance) {
    auto& p = *impl_;
    p.graphics.focus_distance(std::max(.08f, focus_distance));
    auto settings = std::tuple(detail, occlusion, soft_focus, inspecting);
    if (p.settings == settings)
        return;
    p.settings = settings;
    p.near_plane = inspecting ? .006f : .045f;
    p.options.temporal_aa = detail > 0;
    p.options.occlusion = occlusion && detail > 0;
    p.options.occlusion_radius = inspecting ? .035f : .25f;
    p.options.occlusion_power = .85f;
    p.options.occlusion_resolution = detail == 2 ? 1.f : .5f;
    p.options.occlusion_quality = detail == 2 ? 2 : 1;
    p.options.soft_shadows = detail == 2;
    p.options.depth_of_field = soft_focus && inspecting && detail > 0;
    p.options.focus_distance = std::max(.08f, focus_distance);
    shadow_resolution(p.resources, p.sun, detail == 0 ? 1024 : 2048);
    p.graphics.configure(p.options);
}
void garden_scene::present(const greenhouse& game, const sengine::camera_pose& player, double seconds,
                           std::optional<tending_preview> preview) {
    auto& p = *impl_;
    p.outside_body = game.mode() == greenhouse_mode::editor ||
                     (game.mode() == greenhouse_mode::enter_editor && game.progress() > .8f) ||
                     (game.mode() == greenhouse_mode::leave_editor && game.progress() < .2f);
    p.player_head = player.eye;
    auto& scene = p.resources;
    auto point = [](sengine::point a) { return sengine::float3{a.x, a.y, a.z}; };
    const sengine::float3 forward{std::sin(player.yaw), 0, -std::cos(player.yaw)},
        right{std::cos(player.yaw), 0, std::sin(player.yaw)};
    const auto* active = game.active();
    const auto carry = compute_carry_pose(player, active ? active->world.design() : garden_design{});
    const auto held = point(carry.base);
    const float hold = game.carry_blend();
    auto ease = [](float x) {
        x = std::clamp(x, 0.f, 1.f);
        return x * x * (3 - 2 * x);
    };
    float t = game.progress(), support = hold;
    if (game.mode() == greenhouse_mode::lifting) {
        support = ease(t / .28f);
    }
    if (game.mode() == greenhouse_mode::lowering) {
        support = 1 - ease((t - .72f) / .28f);
    }
    sengine::float3 grip = held;
    begin_transforms(scene);
    for (size_t i = 0; i < 10; ++i) {
        const greenhouse_garden* g = nullptr;
        for (const auto& candidate : game.gardens())
            if (candidate.place == int(i)) {
                g = &candidate;
                break;
            }
        auto mark = point(greenhouse::spots()[i].position);
        mark.y += .009f;
        set_transform(scene, p.markers[i],
                      sengine::translation(game.mode() == greenhouse_mode::carry && (!g || g == active)
                                               ? mark
                                               : sengine::float3{0, -100, 0}));
        for (auto& n : p.vessels[i])
            p.show(n, g != nullptr);
        auto root = p.jars[i];
        if (!g) {
            p.substrate->update(i, nullptr, {});
            for (unsigned j = 0; j < p.previous_plants[i]; ++j)
                for (auto& n : p.foliage[i][j])
                    p.show(n, false);
            p.previous_plants[i] = 0;
            p.animal_motion[i] = {};
            continue;
        }
        float radius = float(g->world.design().radius()), height = float(g->world.design().height());
        float soil_top = float(g->world.design().soil_depth + g->world.design().drainage_depth) / height;
        float drain = float(g->world.design().drainage_depth) / height,
              soil = float(g->world.design().soil_depth) / height;
        set_transform(scene, p.vessels[i][1].entity,
                      sengine::translation(sengine::float3{0, drain - .08f * soil / .155f, 0}) *
                          sengine::scaling(sengine::float3{1, soil / .155f, 1}));
        set_transform(scene, p.vessels[i][2].entity, sengine::scaling(sengine::float3{1, drain / .08f, 1}));
        auto location = point(greenhouse::spots()[i].position);
        if (g == active && game.holding()) {
            auto anchor = location;
            if (game.mode() == greenhouse_mode::lowering)
                anchor = point(greenhouse::spots()[game.destination()].position);
            location = anchor * (1 - hold) + held * hold;
            grip = location;
        }
        auto transform = sengine::translation(location) *
                         sengine::rotation(g->rotation, sengine::float3{0, 1, 0}) *
                         sengine::scaling(sengine::float3{radius, height, radius});
        if (transform != p.jar_transforms[i]) {
            set_transform(scene, root, transform);
            p.jar_transforms[i] = transform;
        }
        p.substrate->update(i, &g->world, transform);
        p.animal_motion[i].update(g->world, g->id);
        for (size_t j = 0; j < std::max<size_t>(p.previous_plants[i], g->world.plants().size()); ++j) {
            auto& n = p.plants[i][j];
            auto inst = n.entity;
            if (j >= g->world.plants().size()) {
                for (auto& f : p.foliage[i][j])
                    p.show(f, false);
                continue;
            }
            const auto& plant = g->world.plants()[j];
            for (unsigned k = 0; k < 3; ++k) {
                auto& f = p.foliage[i][j][k];
                p.show(f, k == unsigned(plant.kind));
            }
            float growth = std::clamp(world_state::growth_scale(plant), .15f, 1.f);
            const float x = plant.position.x / world_state::radius * .80f;
            const float z = plant.position.y / world_state::radius * .80f;
            const float clearance = std::max(.015f, .88f - std::hypot(x, z)) * radius;
            const float desired_height = (1 - soil_top) * height * (.30f + .42f * growth);
            const float plant_height =
                std::min(desired_height, clearance / p.plant_radius[unsigned(plant.kind)]);
            set_transform(scene, inst,
                          sengine::translation(sengine::float3{x, soil_top + .004f, z}) *
                              sengine::rotation(float(plant.shape % 628) * .01f, sengine::float3{0, 1, 0}) *
                              sengine::scaling(sengine::float3{plant_height / radius, plant_height / height,
                                                               plant_height / radius}) *
                              n.bind);
        }
        for (const auto& bug : g->world.creatures()) {
            const float size = creature_length(bug);
            const auto pose =
                transform *
                sengine::translation(sengine::float3{
                    bug.position.x / world_state::radius * .8f,
                    soil_top + (presentation::bed_offset(g->world, bug.position) + .001f) / height,
                    bug.position.y / world_state::radius * .8f}) *
                sengine::scaling(sengine::float3{size / radius, size / height, size / radius}) *
                sengine::rotation(-bug.heading, sengine::float3{0, 1, 0});
            p.fauna->place(bug.species, pose, p.animal_motion[i].weights(bug.id));
        }
    }
    p.fauna->finish();
    for (unsigned kind = 0; kind < p.previews.size(); ++kind) {
        auto& n = p.previews[kind];
        const bool visible = preview && active && active->place >= 0 &&
                             game.mode() == greenhouse_mode::editor && int(preview->tool) == int(kind) + 1;
        p.show(n, visible);
        if (!visible)
            continue;
        const auto& design = active->world.design();
        const float radius = float(design.radius()), height = float(design.height());
        const float soil_top = float(design.soil_depth + design.drainage_depth) / height;
        const float x = preview->position.x / world_state::radius * .8f;
        const float z = preview->position.y / world_state::radius * .8f;
        float size;
        if (kind < 3) {
            plant_state plant{};
            plant.kind = plant_kind(kind);
            plant.biomass = .45f;
            const float growth = std::clamp(world_state::growth_scale(plant), .15f, 1.f);
            const float clearance = std::max(.015f, .88f - std::hypot(x, z)) * radius;
            size =
                std::min((1 - soil_top) * height * (.30f + .42f * growth), clearance / p.plant_radius[kind]);
        } else {
            creature_state creature{};
            creature.species = species_kind(kind - 3);
            creature.age = maturity_age(creature.species);
            size = creature_length(creature);
        }
        set_transform(
            scene, n.entity,
            p.jar_transforms[active->place] *
                sengine::translation(sengine::float3{
                    x,
                    soil_top +
                        (kind < 3
                             ? presentation::surface_offset(active->world, preview->position) / height + .004f
                             : (presentation::bed_offset(active->world, preview->position) + .001f) / height),
                    z}) *
                sengine::scaling(sengine::float3{size / radius, size / height, size / radius}) * n.bind);
    }
    end_transforms(scene);
    p.previous_plants.fill(0);
    for (const auto& g : game.gardens())
        if (g.place >= 0) {
            p.previous_plants[g.place] = g.world.plants().size();
        }

    for (auto& [name, j] : p.joints)
        set_transform(scene, j.entity, j.bind);
    const bool crouched = player.eye.y - player.feet.y < 1.40f;
    const char* clip = !player.grounded      ? "Jump_Loop"
                       : crouched            ? (player.speed > .15f ? "Crouch_Fwd_Loop" : "Crouch_Idle_Loop")
                       : player.speed > 4.f  ? "Sprint_Loop"
                       : player.speed > 3.1f ? "Jog_Fwd_Loop"
                       : player.speed > .15f ? "Walk_Loop"
                                             : "Idle_Loop";
    auto selected = p.clips.at(clip);
    if (!p.clip_started || selected != p.active_clip) {
        p.previous_clip = p.active_clip;
        p.previous_clip_time = p.clip_time;
        p.active_clip = selected;
        p.blend_time = p.clip_started ? 0.f : 1.f;
        p.clip_started = true;
        p.clip_time = 0;
    }
    const float duration = animation_duration(scene, p.character, p.active_clip);
    p.clip_time = player.grounded && player.speed > .15f
                      ? std::fmod(player.stride / (2 * 3.14159265f), 1.f) * duration
                      : std::fmod(p.clip_time + float(seconds), duration);
    animate(scene, p.character, p.active_clip, p.clip_time);
    p.blend_time += float(seconds);
    if (p.blend_time < .18f)
        blend_animation(scene, p.character, p.previous_clip, p.previous_clip_time, ease(p.blend_time / .18f));
    auto body_root = root(scene, p.character);
    auto body_position = point(player.feet);
    const auto body_rotation = sengine::rotation(3.14159265f - player.yaw, sengine::float3{0, 1, 0});
    set_transform(scene, body_root, sengine::translation(body_position) * body_rotation);
    auto entity = [&](const std::string& name) { return p.joints.at(name).entity; };
    auto world = [&](const std::string& name) { return world_transform(scene, entity(name)); };
    auto position = [&](const std::string& name) { return world(name)[3].xyz(); };
    auto rotate_world = [&](const std::string& name, sengine::float3 axis, float radians) {
        auto inst = entity(name);
        auto w = world_transform(scene, inst);
        auto pos = w[3].xyz();
        auto parent = sengine::parent(scene, inst);
        auto pw = parent ? world_transform(scene, parent) : sengine::mat4{};
        set_transform(scene, inst,
                      inverse(pw) * sengine::translation(pos) * sengine::rotation(radians, axis) *
                          sengine::translation(-pos) * w);
    };
    auto aim = [&](const std::string& name, const std::string& child, sengine::float3 target) {
        auto a = position(name);
        auto from = normalize(position(child) - a), to = normalize(target - a);
        auto axis = cross(from, to);
        float l = length(axis);
        if (l > .00001f)
            rotate_world(name, axis / l, std::acos(std::clamp(dot(from, to), -1.f, 1.f)));
    };
    auto ik = [&](const std::string& upper, const std::string& lower, const std::string& end,
                  sengine::float3 target, sengine::float3 pole) {
        auto a = position(upper);
        float l1 = length(position(lower) - a), l2 = length(position(end) - position(lower));
        auto delta = target - a;
        float len = std::clamp(length(delta), std::abs(l1 - l2) + .001f, l1 + l2 - .001f);
        auto dir = normalize(delta);
        auto side = normalize(pole - a - dir * dot(pole - a, dir));
        float along = (l1 * l1 - l2 * l2 + len * len) / (2 * len);
        auto middle = a + dir * along + side * std::sqrt(std::max(0.f, l1 * l1 - along * along));
        aim(upper, lower, middle);
        aim(lower, end, a + dir * len);
    };
    if (player.grounded) {
        const float lowest_ankle = std::min(position("foot_l").y, position("foot_r").y);
        body_position.y += player.feet.y + .07f - lowest_ankle;
        set_transform(scene, body_root, sengine::translation(body_position) * body_rotation);
    }

    std::array<sengine::float3, 2> authored_hands{position("hand_l"), position("hand_r")};
    if (support > 0) {
        for (auto& [name, joint] : p.joints) {
            if (!(name.starts_with("upperarm_") || name.starts_with("lowerarm_") ||
                  name.starts_with("hand_") || name.starts_with("index_") || name.starts_with("middle_") ||
                  name.starts_with("ring_") || name.starts_with("pinky_") || name.starts_with("thumb_")))
                continue;
            auto instance = joint.entity;
            auto local = local_transform(scene, instance);
            auto rotation = slerp(rotation_of(local), rotation_of(joint.bind), support);
            auto blended = sengine::rotation(rotation);
            blended[3] = local[3];
            set_transform(scene, instance, blended);
        }
    }
    float reach_lean =
        (std::clamp((dot(grip - body_position, forward) - .68f) * 1.8f, 0.f, .6f) * (1 - hold) +
         .08f * hold) *
        support;
    if (reach_lean > 0)
        rotate_world("spine_01", right, -reach_lean);
    for (auto side : {std::string("l"), std::string("r")}) {

        float sign = side == "l" ? -1.f : 1.f;
        const unsigned boot_index = side == "l" ? 0 : 1;
        auto& boot = p.boots[boot_index];
        set_transform(scene, boot.entity, world("foot_" + side) * p.boot_attachments[boot_index]);
        if (support <= 0)
            continue;
        auto shoulder = position("upperarm_" + side);
        auto rest_hand = authored_hands[boot_index];
        auto held_hand = point(carry.wrists[boot_index]) + grip - held;
        auto hand = rest_hand * (1 - support) + held_hand * support;
        ik("upperarm_" + side, "lowerarm_" + side, "hand_" + side, hand,
           shoulder + right * sign * .30f + sengine::float3{0, -.45f, 0} - forward * .05f);
        auto toward = support > .01f ? forward * .06f - right * sign * .09f : sengine::float3{0, -.12f, 0};
        aim("hand_" + side, "middle_01_" + side, position("hand_" + side) + toward);
        const auto wrist = position("hand_" + side);
        const auto finger_axis = normalize(position("middle_01_" + side) - wrist);
        auto palm =
            normalize(cross(position("index_01_" + side) - wrist, position("pinky_01_" + side) - wrist)) *
            sign;
        palm = normalize(palm - finger_axis * dot(palm, finger_axis));
        auto up = normalize(sengine::float3{0, 1, 0} - finger_axis * finger_axis.y);
        const float roll = std::atan2(dot(finger_axis, cross(palm, up)), dot(palm, up));
        rotate_world("hand_" + side, finger_axis, roll * support);
        for (auto f : {"index", "middle", "ring", "pinky"})
            for (auto segment : {"01", "02", "03"}) {
                std::string name = std::string(f) + "_" + segment + "_" + side;
                auto inst = entity(name);
                auto local = local_transform(scene, inst);
                set_transform(scene, inst,
                              local * sengine::rotation(support * .08f, sengine::float3{1, 0, 0}));
            }
    }
    update_bones(scene, p.character);
}

bool garden_scene::frame(const sengine::camera_pose& camera, unsigned width, unsigned height,
                         const std::filesystem::path& capture, bool capture_interface) {
    if (!width || !height)
        return false;
    auto& p = *impl_;
    const auto eye = camera.eye;
    float head_distance =
        std::hypot(eye.x - p.player_head.x, eye.y - p.player_head.y, eye.z - p.player_head.z);
    p.graphics.visible_layers(p.outside_body && head_distance > .4f ? 0x03 : 0x01);
    return p.graphics.frame(camera, width, height, p.near_plane, 330.f, p.hud.get(), capture,
                            capture_interface);
}
}
