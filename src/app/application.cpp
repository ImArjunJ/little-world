#include "application.hpp"
#include "greenhouse_session.hpp"
#include <stdexcept>

namespace terrarium {
void application::run() {
    auto& session = host_.scenes().emplace<greenhouse_session>(host_);
    host_.run();
    session.save();
}
greenhouse_session::greenhouse_session(sengine::application& host) : host_(host), display(host.display()) {
    pointer.focus(display.metrics().focused);
    pointer.request(frontend.walking());
    sync_pointer();
}
void greenhouse_session::save() {
    frontend.save_preferences();
    if (!game.save())
        throw std::runtime_error(game.error());
}
void greenhouse_session::update(const sengine::runtime_frame& frame) {
    focused_ = host_.viewport().focused;
    advance_world(frame_input_, frame.seconds, focused_);
}
void greenhouse_session::render(const sengine::runtime_frame& frame) {
    const auto& viewport = host_.viewport();
    draw_frame(frame_input_, viewport, frame.seconds, focused_);
}
}
