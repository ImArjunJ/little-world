#include "footstep_audio.hpp"
#include "greenhouse_controls.hpp"
#include "native_audio.hpp"
#include "native_frontend.hpp"
#include "render/filament_scene.hpp"
#include "sengine/pointer_capture.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "sengine/window.hpp"
namespace terrarium {
void run_game() {
    sengine::window display("Little World — greenhouse", 1440, 900);
    auto* window = display.handle();
    const auto data = sengine::window::executable_directory() / "data";
    render::filament_scene renderer(display.native(), data / "scene/greenhouse.gltf",
                                    render::render_quality::high, display.shared_context());
    display.release_context();
    auto landscape = sengine::load_landscape(data / "scene/landscape.bin");
    auto explorer = sengine::make_explorer(landscape, {2.95f, 0, 2.6f}, -.48f, -.17f);
    footstep_audio footsteps(true, data / "audio/footsteps");
    std::filesystem::path profile;
    const char *xdg = std::getenv("XDG_DATA_HOME"), *home = std::getenv("HOME");
#ifdef __APPLE__
    profile = xdg    ? std::filesystem::path(xdg)
              : home ? std::filesystem::path(home) / "Library/Application Support"
                     : std::filesystem::current_path();
#else
    profile = xdg    ? std::filesystem::path(xdg)
              : home ? std::filesystem::path(home) / ".local/share"
                     : std::filesystem::current_path();
#endif
    profile /= "little-world";
    greenhouse game(profile);
    greenhouse_controls interface(game, explorer, landscape);
    native_frontend frontend(game, interface, explorer, profile);
    frontend.start();
    native_audio ambience(true);
    bool fullscreen = false;
    bool running = true, camera_motion = true, text_input = false;
    sengine::pointer_capture pointer;
    sengine::focus(pointer, SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
    sengine::request_capture(pointer, frontend.walking());
    auto sync_pointer = [&] {
        if (SDL_GetWindowRelativeMouseMode(window) == sengine::captured(pointer))
            return;
        sengine::stop(explorer);
        footsteps.clear();
        if (!SDL_SetWindowRelativeMouseMode(window, sengine::captured(pointer))) {
            pointer.failed = true;
            frontend.ui.toast(SDL_GetError());
        }
    };
    sync_pointer();
    auto previous = std::chrono::steady_clock::now();
    while (running) {
        bool jump_pressed = false;
        sengine::hud_input hud_input;
        frontend.canvas.begin_events();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                                 event.window.windowID == SDL_GetWindowID(window))) {
                if (game.save() && frontend.save_preferences())
                    running = false;
                else if (!game.error().empty())
                    frontend.ui.toast(game.error());
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                sengine::focus(pointer, false);
                sengine::stop(explorer);
                footsteps.clear();
                sync_pointer();
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                sengine::focus(pointer, true);
                sync_pointer();
            }
            const bool input_event =
                event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP ||
                event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_MOUSE_MOTION ||
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                event.type == SDL_EVENT_MOUSE_WHEEL;
            if (input_event && !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS))
                continue;
            if (((event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
                pointer.failed = false;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
                hud_input.pressed = true;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT)
                hud_input.released = true;
            if (event.type == SDL_EVENT_MOUSE_WHEEL)
                hud_input.wheel += event.wheel.y;
            frontend.event(event, sengine::captured(pointer));
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && !frontend.typing()) {
                if (event.key.scancode == SDL_SCANCODE_SPACE && sengine::captured(pointer) &&
                    game.mode() == terrarium::greenhouse_mode::explore && frontend.walking())
                    jump_pressed = true;
                if (event.key.scancode == SDL_SCANCODE_M &&
                    game.mode() == terrarium::greenhouse_mode::explore && frontend.walking())
                    frontend.ui.reduced_motion = !frontend.ui.reduced_motion;
                if (event.key.scancode == SDL_SCANCODE_HOME &&
                    game.mode() == terrarium::greenhouse_mode::explore && frontend.walking())
                    sengine::relocate(explorer, {2.95f, 0, 2.6f}, -.48f, -.17f);
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION && sengine::captured(pointer) && frontend.walking()) {
                auto before = sengine::camera(explorer, false);
                sengine::look(explorer, event.motion.xrel * .0025f, -event.motion.yrel * .0025f);
                if (!game.carry_clear(sengine::camera(explorer, false), landscape))
                    sengine::relocate(explorer, before.feet, before.yaw, std::asin(before.direction.y));
            }
        }
        const auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const bool focused = SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS;
        const auto* keys = SDL_GetKeyboardState(nullptr);
        if (sengine::captured(pointer) && focused && frontend.walking()) {
            auto before = sengine::camera(explorer, false);
            sengine::advance(
                explorer, std::min(.1, elapsed),
                game.locomotion({float(keys[SDL_SCANCODE_W]) - float(keys[SDL_SCANCODE_S]),
                                 float(keys[SDL_SCANCODE_D]) - float(keys[SDL_SCANCODE_A]),
                                 keys[SDL_SCANCODE_LSHIFT], keys[SDL_SCANCODE_LCTRL], jump_pressed}));
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
        int width = 0, height = 0, lw = 0, lh = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        SDL_GetWindowSize(window, &lw, &lh);
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(25);
            continue;
        }
        float mx, my;
        auto buttons = SDL_GetMouseState(&mx, &my);
        hud_input.x = mx * width / std::max(1, lw);
        hud_input.y = my * height / std::max(1, lh);
        hud_input.down = buttons & SDL_BUTTON_LMASK;
        if (!focused) {
            hud_input = {};
        }
        float dpi = std::max(SDL_GetWindowDisplayScale(window), float(width) / std::max(1, lw));
        interface.viewport(float(width) / height);
        auto& hud = renderer.hud();
        hud.begin(width, height, 1.f);
        frontend.draw(hud, width, height, dpi, elapsed, hud_input, focused);

        if (frontend.typing() != text_input) {
            text_input = frontend.typing();
            if (text_input)
                SDL_StartTextInput(window);
            else
                SDL_StopTextInput(window);
        }
        if (frontend.ui.borderless != fullscreen) {
            if (SDL_SetWindowFullscreen(window, frontend.ui.borderless))
                fullscreen = frontend.ui.borderless;
            else {
                frontend.ui.borderless = fullscreen;
                frontend.ui.toast(SDL_GetError());
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
        const auto* display = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
        double hz = display ? std::clamp(double(display->refresh_rate), 30.0, 144.0) : 60.0;
        double remaining =
            1.0 / hz - std::chrono::duration<double>(std::chrono::steady_clock::now() - now).count();
        if (remaining > 0)
            SDL_DelayPrecise(Uint64(remaining * 1e9));
    }
    frontend.save_preferences();
    if (!game.save())
        throw std::runtime_error(game.error());
}
}
