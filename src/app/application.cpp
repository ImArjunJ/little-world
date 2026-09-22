#include "footstep_audio.hpp"
#include "greenhouse_controls.hpp"
#include "native_audio.hpp"
#include "native_frontend.hpp"
#include "render/garden_scene.hpp"
#include "sengine/events.hpp"
#include "sengine/pointer_capture.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>

#include "sengine/window.hpp"
namespace terrarium {
void run_game() {
    sengine::window display("Little World — greenhouse", 1440, 900);
    const auto data = sengine::window::executable_directory() / "data";
    render::garden_scene renderer(display, data / "scene/greenhouse.gltf", render::render_quality::high);
    auto landscape = sengine::load_landscape(data / "scene/landscape.bin");
    auto explorer = sengine::make_explorer(landscape, {2.95f, 0, 2.6f}, -.48f, -.17f);
    footstep_audio footsteps(true, data / "audio/footsteps");
    const auto profile = sengine::user_data_directory("little-world");
    greenhouse game(profile);
    greenhouse_controls interface(game, explorer, landscape);
    native_frontend frontend(game, interface, explorer, profile);
    frontend.canvas.display = &display;
    frontend.start();
    native_audio ambience(true);
    bool fullscreen = false;
    bool running = true, camera_motion = true, text_input = false;
    sengine::pointer_capture pointer;
    sengine::focus(pointer, display.metrics().focused);
    sengine::request_capture(pointer, frontend.walking());
    auto sync_pointer = [&] {
        if (display.captured() == sengine::captured(pointer))
            return;
        sengine::stop(explorer);
        footsteps.clear();
        if (!display.capture(sengine::captured(pointer))) {
            pointer.failed = true;
            frontend.ui.toast(display.error());
        }
    };
    sync_pointer();
    auto previous = std::chrono::steady_clock::now();
    while (running) {
        bool jump_pressed = false;
        sengine::hud_input hud_input;
        frontend.canvas.begin_events();
        sengine::input_event event;
        while (display.poll(event)) {
            if (event.type == sengine::event_type::quit || event.type == sengine::event_type::close) {
                if (game.save() && frontend.save_preferences())
                    running = false;
                else if (!game.error().empty())
                    frontend.ui.toast(game.error());
            }
            if (event.type == sengine::event_type::focus_lost) {
                sengine::focus(pointer, false);
                sengine::stop(explorer);
                footsteps.clear();
                sync_pointer();
            }
            if (event.type == sengine::event_type::focus_gained) {
                sengine::focus(pointer, true);
                sync_pointer();
            }
            const bool input_event =
                event.type == sengine::event_type::key_down || event.type == sengine::event_type::key_up ||
                event.type == sengine::event_type::text_input ||
                event.type == sengine::event_type::mouse_motion ||
                event.type == sengine::event_type::mouse_down ||
                event.type == sengine::event_type::mouse_up || event.type == sengine::event_type::mouse_wheel;
            if (input_event && !(display.metrics().focused))
                continue;
            if (((event.type == sengine::event_type::key_down && !event.key.repeat) ||
                 event.type == sengine::event_type::mouse_down))
                pointer.failed = false;
            if (event.type == sengine::event_type::mouse_down &&
                event.button.button == sengine::mouse_button::left)
                hud_input.pressed = true;
            if (event.type == sengine::event_type::mouse_up &&
                event.button.button == sengine::mouse_button::left)
                hud_input.released = true;
            if (event.type == sengine::event_type::mouse_wheel)
                hud_input.wheel += event.wheel.y;
            frontend.event(event, sengine::captured(pointer));
            if (event.type == sengine::event_type::key_down && !event.key.repeat && !frontend.typing()) {
                if (event.key.code == sengine::key_code::space && sengine::captured(pointer) &&
                    game.mode() == terrarium::greenhouse_mode::explore && frontend.walking())
                    jump_pressed = true;
                if (event.key.code == sengine::key_code::m &&
                    game.mode() == terrarium::greenhouse_mode::explore && frontend.walking())
                    frontend.ui.reduced_motion = !frontend.ui.reduced_motion;
                if (event.key.code == sengine::key_code::home &&
                    game.mode() == terrarium::greenhouse_mode::explore && frontend.walking())
                    sengine::relocate(explorer, {2.95f, 0, 2.6f}, -.48f, -.17f);
            }
            if (event.type == sengine::event_type::mouse_motion && sengine::captured(pointer) &&
                frontend.walking()) {
                auto before = sengine::camera(explorer, false);
                sengine::look(explorer, event.motion.dx * .0025f, -event.motion.dy * .0025f);
                if (!game.carry_clear(sengine::camera(explorer, false), landscape))
                    sengine::relocate(explorer, before.feet, before.yaw, std::asin(before.direction.y));
            }
        }
        const auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const bool focused = display.metrics().focused;
        const auto keys = display.keys();
        if (sengine::captured(pointer) && focused && frontend.walking()) {
            auto before = sengine::camera(explorer, false);
            sengine::advance(
                explorer, std::min(.1, elapsed),
                game.locomotion({float(keys[sengine::key_code::w]) - float(keys[sengine::key_code::s]),
                                 float(keys[sengine::key_code::d]) - float(keys[sengine::key_code::a]),
                                 keys[sengine::key_code::left_shift], keys[sengine::key_code::left_control],
                                 jump_pressed}));
            if (!game.carry_clear(sengine::camera(explorer, false), landscape))
                sengine::relocate(explorer, before.feet, before.yaw, std::asin(before.direction.y));
        }
        footsteps.volume(frontend.ui.audio.enabled ? frontend.ui.audio.master : 0);
        for (unsigned count = sengine::take_footsteps(explorer); count > 0; --count) {
            auto p = explorer.position;
            footsteps.step(std::abs(p.x) < 4.6f && std::abs(p.z) < 3.6f,
                           sengine::speed(explorer) > 3 ? .26f : .18f);
        }
        if (float impact = sengine::take_landing(explorer); impact > 0) {
            auto p = explorer.position;
            footsteps.step(std::abs(p.x) < 4.6f && std::abs(p.z) < 3.6f, std::min(.42f, impact * .055f));
        }

        if (focused)
            game.advance(elapsed, frontend.living());
        const auto metrics = display.metrics();
        const auto [width, height, lw, lh] =
            std::array{metrics.width, metrics.height, metrics.logical_width, metrics.logical_height};
        if (metrics.minimized || !width || !height) {
            sengine::sleep_for(.025);
            continue;
        }
        const auto mouse = display.pointer();
        const auto mx = mouse.x, my = mouse.y;
        const auto buttons = mouse.buttons;
        hud_input.x = mx * width / std::max(1, lw);
        hud_input.y = my * height / std::max(1, lh);
        hud_input.down = buttons & sengine::left_button;
        if (!focused) {
            hud_input = {};
        }
        float dpi = metrics.scale;
        interface.viewport(float(width) / height);
        auto& hud = renderer.hud();
        hud.begin(width, height, 1.f);
        frontend.draw(hud, width, height, dpi, elapsed, hud_input, focused);

        if (frontend.typing() != text_input) {
            text_input = frontend.typing();
            display.text_input(text_input);
        }
        if (frontend.ui.borderless != fullscreen) {
            if (display.fullscreen(frontend.ui.borderless))
                fullscreen = frontend.ui.borderless;
            else {
                frontend.ui.borderless = fullscreen;
                frontend.ui.toast(display.error());
            }
        }
        camera_motion = !frontend.ui.reduced_motion;
        if (frontend.quit)
            running = false;
        sengine::focus(pointer, focused);
        sengine::request_capture(pointer, frontend.walking());
        sync_pointer();
        auto player = sengine::camera(explorer, camera_motion && !game.holding());
        renderer.present(game, player, focused ? elapsed : 0, focused ? frontend.preview() : std::nullopt);
        const auto* active_garden = game.active();
        terrarium::audio_environment environment{};
        if (active_garden) {
            environment.pond = active_garden->world.pond_level();
            environment.raining = active_garden->world.climate.rain_until > active_garden->world.day();
        }
        ambience.update(environment, frontend.ui.audio, focused);
        auto camera = game.mode() == terrarium::greenhouse_mode::explore || game.holding()
                          ? player
                          : interface.camera();
        if (game.holding()) {
            float aspect = float(width) / height;
            const float vertical_fov =
                active_garden && active_garden->world.design().form == terrarium::vessel_form::tall ? 85.f
                                                                                                    : 75.f;
            const float carrying_fov =
                std::max(vertical_fov,
                         2.f * std::atan(std::tan(75.f * 3.14159265f / 360) / aspect) * 180 / 3.14159265f);
            camera.fov = std::lerp(camera.fov, carrying_fov, game.carry_blend());
        }
        auto focal = interface.world_point({});
        const float focus_distance = std::sqrt((camera.eye.x - focal.x) * (camera.eye.x - focal.x) +
                                               (camera.eye.y - focal.y) * (camera.eye.y - focal.y) +
                                               (camera.eye.z - focal.z) * (camera.eye.z - focal.z));
        renderer.configure(frontend.ui.performance, frontend.ui.ambient_occlusion, frontend.ui.lens_blur,
                           game.mode() == terrarium::greenhouse_mode::editor ||
                               game.mode() == terrarium::greenhouse_mode::enter_editor ||
                               game.mode() == terrarium::greenhouse_mode::leave_editor,
                           focus_distance);
        const bool exporting = !frontend.photograph.empty();
        const auto& output = frontend.photograph;
        try {
            if (renderer.frame(camera, unsigned(width), unsigned(height), output, !exporting)) {
                if (exporting) {
                    frontend.ui.toast("Photograph kept in your photographs folder.");
                    frontend.photograph.clear();
                }
            }
        } catch (const std::exception& error) {
            if (!exporting)
                throw;
            frontend.ui.toast(error.what());
            frontend.photograph.clear();
        }
        double hz = std::clamp(metrics.refresh_rate, 30.0, 144.0);
        double remaining =
            1.0 / hz - std::chrono::duration<double>(std::chrono::steady_clock::now() - now).count();
        if (remaining > 0)
            sengine::sleep_for(remaining);
    }
    frontend.save_preferences();
    if (!game.save())
        throw std::runtime_error(game.error());
}
}
