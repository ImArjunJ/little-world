#include "greenhouse_session.hpp"
#include <algorithm>
#include <cmath>

namespace terrarium {
using sengine::event_type;
using sengine::key_code;
bool greenhouse_session::close_requested() {
    if (game.save() && frontend.save_preferences())
        return true;
    if (!game.error().empty())
        frontend.notify(game.error());
    return false;
}
void greenhouse_session::window_event(const sengine::input_event& event) {
    switch (event.type) {
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
void greenhouse_session::walking_shortcut(frame_input& input, const sengine::input_event& event) {
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
void greenhouse_session::look_around(const sengine::input_event& event) {
    if (event.type != event_type::mouse_motion || !pointer.captured() || !frontend.walking())
        return;
    const auto before = explorer.camera(false);
    explorer.look(event.motion.dx * .0025f, -event.motion.dy * .0025f);
    if (!game.carry_clear(explorer.camera(false), landscape))
        explorer.relocate(before.feet, before.yaw, std::asin(before.direction.y));
}
void greenhouse_session::sync_pointer() {
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
void greenhouse_session::begin_frame() {
    frame_input_ = {};
    frontend.begin_events();
}
bool greenhouse_session::event(const sengine::input_event& event) {
    window_event(event);
    if ((event.type == event_type::key_down && !event.key.repeat) || event.type == event_type::mouse_down)
        pointer.retry();
    pointer_event(frame_input_, event);
    frontend.event(event, pointer.captured());
    walking_shortcut(frame_input_, event);
    look_around(event);
    return false;
}
}
