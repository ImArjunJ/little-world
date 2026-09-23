#include "greenhouse_session.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace terrarium {
void greenhouse_session::walk(bool jump, double elapsed, bool focused) {
    const auto keys = display.keys();
    if (!pointer.captured() || !focused || !frontend.walking())
        return;
    using sengine::key_code;
    const auto before = explorer.camera(false);
    const terrarium::explorer_input movement{float(keys[key_code::w]) - float(keys[key_code::s]),
                                             float(keys[key_code::d]) - float(keys[key_code::a]),
                                             keys[key_code::left_shift], keys[key_code::left_control], jump};
    explorer.advance(std::min(.1, elapsed), game.locomotion(movement));
    if (!game.carry_clear(explorer.camera(false), landscape))
        explorer.relocate(before.feet, before.yaw, std::asin(before.direction.y));
}
void greenhouse_session::play_footsteps() {
    const auto audio = frontend.settings().audio;
    footsteps.volume(audio.enabled ? audio.master : 0);
    const auto position = explorer.position();
    const bool indoors = std::abs(position.x) < 4.6f && std::abs(position.z) < 3.6f;
    for (unsigned count = explorer.take_footsteps(); count > 0; --count)
        footsteps.step(indoors, explorer.speed() > 3 ? .26f : .18f);
    if (float impact = explorer.take_landing(); impact > 0)
        footsteps.step(indoors, std::min(.42f, impact * .055f));
}
void greenhouse_session::apply_window_preferences(bool focused) {
    frontend.apply_window_preferences();
    if (frontend.quit_requested())
        host_.scenes().quit();
    pointer.focus(focused);
    pointer.request(frontend.walking());
    sync_pointer();
}
void greenhouse_session::draw_interface(frame_input input, const sengine::window_metrics& viewport,
                                        double elapsed, bool focused) {
    const auto mouse = display.pointer();
    input.pointer.x = mouse.x * viewport.width / std::max(1, viewport.logical_width);
    input.pointer.y = mouse.y * viewport.height / std::max(1, viewport.logical_height);
    input.pointer.down = mouse.buttons & sengine::left_button;
    if (!focused)
        input.pointer = {};
    controls.viewport(float(viewport.width) / viewport.height);
    frontend.draw(viewport.width, viewport.height, viewport.scale, elapsed, input.pointer, focused);
}
void greenhouse_session::update_ambience(bool focused) {
    audio_environment environment{};
    if (const auto* garden = game.active()) {
        environment.pond = garden->world.pond_level();
        environment.raining = garden->world.climate.rain_until > garden->world.day();
    }
    ambience.update(environment, frontend.settings().audio, focused);
}
terrarium::camera_pose greenhouse_session::view_camera(const terrarium::camera_pose& player, float aspect) {
    auto camera = game.mode() == greenhouse_mode::explore || game.holding() ? player : controls.camera();
    if (!game.holding())
        return camera;
    const auto* garden = game.active();
    const float vertical_fov = garden && garden->world.design().form == vessel_form::tall ? 85.f : 75.f;
    const float carrying_fov = std::max(
        vertical_fov, 2.f * std::atan(std::tan(75.f * 3.14159265f / 360) / aspect) * 180 / 3.14159265f);
    camera.fov = std::lerp(camera.fov, carrying_fov, game.carry_blend());
    return camera;
}
void greenhouse_session::configure_view(const terrarium::camera_pose& camera) {
    const auto focal = controls.world_point({});
    const float distance = std::sqrt((camera.eye.x - focal.x) * (camera.eye.x - focal.x) +
                                     (camera.eye.y - focal.y) * (camera.eye.y - focal.y) +
                                     (camera.eye.z - focal.z) * (camera.eye.z - focal.z));
    const auto mode = game.mode();
    const bool inspecting = mode == greenhouse_mode::editor || mode == greenhouse_mode::enter_editor ||
                            mode == greenhouse_mode::leave_editor;
    const auto ui = frontend.settings();
    renderer.configure(ui.performance, ui.ambient_occlusion, ui.lens_blur, inspecting, distance);
}
void greenhouse_session::present_frame(const terrarium::camera_pose& camera,
                                       const sengine::window_metrics& viewport) {
    const bool exporting = !frontend.photograph().empty();
    try {
        if (renderer.frame(camera, viewport.width, viewport.height, frontend.photograph(), !exporting) &&
            exporting) {
            frontend.finish_photograph("Photograph kept in your photographs folder.");
        }
    } catch (const std::exception& error) {
        if (!exporting)
            throw;
        frontend.finish_photograph(error.what());
    }
}
void greenhouse_session::advance_world(const frame_input& input, double elapsed, bool focused) {
    walk(input.jump_pressed, elapsed, focused);
    play_footsteps();
    if (focused)
        game.advance(elapsed, frontend.living());
}
void greenhouse_session::draw_frame(frame_input input, const sengine::window_metrics& viewport,
                                    double elapsed, bool focused) {
    draw_interface(input, viewport, elapsed, focused);
    apply_window_preferences(focused);
    const auto player = explorer.camera(!frontend.settings().reduced_motion && !game.holding());
    renderer.present(game, player, focused ? elapsed : 0, focused ? frontend.preview() : std::nullopt);
    update_ambience(focused);
    const auto camera = view_camera(player, float(viewport.width) / viewport.height);
    configure_view(camera);
    present_frame(camera, viewport);
}
}
