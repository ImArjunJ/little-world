#include "garden_scene_state.hpp"
namespace terrarium::render {
namespace {
carrying_pose carrying(const greenhouse& game, const terrarium::camera_pose& player) {
    const auto* active = game.active();
    carrying_pose result;
    result.pose = compute_carry_pose(player, active ? active->world.design() : garden_design{});
    result.held = result.grip = point(result.pose.base);
    result.blend = result.support = game.carry_blend();
    if (game.mode() == greenhouse_mode::lifting)
        result.support = ease(game.progress() / .28f);
    if (game.mode() == greenhouse_mode::lowering)
        result.support = 1 - ease((game.progress() - .72f) / .28f);
    return result;
}
}
garden_scene::garden_scene(sengine::window& window, const std::filesystem::path& path, render_quality quality)
    : state_(std::make_unique<garden_scene_state>(window)) {
    load_garden_scene(*state_, path, quality);
}
garden_scene::~garden_scene() = default;
sengine::native_hud& garden_scene::hud() {
    return *state_->hud;
}
void garden_scene::configure(int detail, bool occlusion, bool soft_focus, bool inspecting,
                             float focus_distance) {
    auto& state = *state_;
    state.graphics.focus_distance(std::max(.08f, focus_distance));
    auto settings = std::tuple(detail, occlusion, soft_focus, inspecting);
    if (state.settings == settings)
        return;
    state.settings = settings;
    state.near_plane = inspecting ? .006f : .045f;
    state.options.temporal_aa = detail > 0;
    state.options.occlusion = occlusion && detail > 0;
    state.options.occlusion_radius = inspecting ? .035f : .25f;
    state.options.occlusion_power = .85f;
    state.options.occlusion_resolution = detail == 2 ? 1.f : .5f;
    state.options.occlusion_quality = detail == 2 ? 2 : 1;
    state.options.soft_shadows = detail == 2;
    state.options.depth_of_field = soft_focus && inspecting && detail > 0;
    state.options.focus_distance = std::max(.08f, focus_distance);
    shadow_resolution(state.resources, state.sun, detail == 0 ? 1024 : 2048);
    state.graphics.configure(state.options);
}
void garden_scene::present(const greenhouse& game, const terrarium::camera_pose& player, double seconds,
                           std::optional<tending_preview> preview) {
    auto& state = *state_;
    state.outside_body = game.mode() == greenhouse_mode::editor ||
                         (game.mode() == greenhouse_mode::enter_editor && game.progress() > .8f) ||
                         (game.mode() == greenhouse_mode::leave_editor && game.progress() < .2f);
    state.player_head = player.eye;
    auto carry = carrying(game, player);
    {
        const sengine::transform_scope transforms(state.resources);
        update_gardens(state, game, carry);
        update_preview(state, game, preview);
    }
    state.fauna->finish();
    state.character->animate(player, seconds, carry);
    state.gardens->synchronize();
}
bool garden_scene::frame(const terrarium::camera_pose& camera, unsigned width, unsigned height,
                         const std::filesystem::path& capture, bool capture_interface) {
    if (!width || !height)
        return false;
    auto& state = *state_;
    const auto eye = camera.eye;
    float head_distance =
        std::hypot(eye.x - state.player_head.x, eye.y - state.player_head.y, eye.z - state.player_head.z);
    state.graphics.visible_layers(state.outside_body && head_distance > .4f ? 0x03 : 0x01);
    return state.graphics.frame({camera.eye, camera.direction, camera.fov}, width, height, state.near_plane,
                                330.f, state.hud.get(), capture, capture_interface);
}
}
