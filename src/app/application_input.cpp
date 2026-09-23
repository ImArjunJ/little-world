#include "application.hpp"
#include <algorithm>
#include <cmath>

namespace terrarium {
using sengine::event_type;
using sengine::key_code;
bool is_input(const sengine::input_event& event) {
    switch (event.type) {
    case event_type::key_down:
    case event_type::key_up:
    case event_type::text_input:
    case event_type::mouse_motion:
    case event_type::mouse_down:
    case event_type::mouse_up:
    case event_type::mouse_wheel:
        return true;
    default:
        return false;
    }
}
void application::window_event(const sengine::input_event& event) {
    switch (event.type) {
    case event_type::quit:
    case event_type::close:
        if (game.save() && frontend.save_preferences())
            running = false;
        else if (!game.error().empty())
            frontend.notify(game.error());
        break;
    case event_type::focus_lost:
        pointer.focus(false);
        explorer.stop();
        footsteps.clear();
        sync_pointer();
        break;
    case event_type::focus_gained:
        pointer.focus(true);
        sync_pointer();
        break;
    default:
        break;
    }
}
void pointer_event(frame_input& input, const sengine::input_event& event) {
    if (event.type == event_type::mouse_down && event.button.button == sengine::mouse_button::left)
        input.pointer.pressed = true;
    if (event.type == event_type::mouse_up && event.button.button == sengine::mouse_button::left)
        input.pointer.released = true;
    if (event.type == event_type::mouse_wheel)
        input.pointer.wheel += event.wheel.y;
}
void application::walking_shortcut(frame_input& input, const sengine::input_event& event) {
    if (event.type != event_type::key_down || event.key.repeat || frontend.typing())
        return;
    if (game.mode() != greenhouse_mode::explore || !frontend.walking())
        return;
    switch (event.key.code) {
    case key_code::space:
        input.jump_pressed = pointer.captured();
        break;
    case key_code::m:
        frontend.toggle_reduced_motion();
        break;
    case key_code::home:
        explorer.relocate({2.95f, 0, 2.6f}, -.48f, -.17f);
        break;
    default:
        break;
    }
}
void application::look_around(const sengine::input_event& event) {
    if (event.type != event_type::mouse_motion || !pointer.captured() || !frontend.walking())
        return;
    const auto before = explorer.camera(false);
    explorer.look(event.motion.dx * .0025f, -event.motion.dy * .0025f);
    if (!game.carry_clear(explorer.camera(false), landscape))
        explorer.relocate(before.feet, before.yaw, std::asin(before.direction.y));
}
void application::sync_pointer() {
    const bool capture = pointer.captured();
    if (display.captured() == capture)
        return;
    explorer.stop();
    footsteps.clear();
    if (!display.capture(capture)) {
        pointer.fail();
        frontend.notify(display.error());
    }
}
frame_input application::poll_events() {
    frame_input input;
    frontend.begin_events();
    sengine::input_event event;
    while (display.poll(event)) {
        window_event(event);
        if (is_input(event) && !display.metrics().focused)
            continue;
        if ((event.type == event_type::key_down && !event.key.repeat) || event.type == event_type::mouse_down)
            pointer.retry();
        pointer_event(input, event);
        frontend.event(event, pointer.captured());
        walking_shortcut(input, event);
        look_around(event);
    }
    return input;
}
}
