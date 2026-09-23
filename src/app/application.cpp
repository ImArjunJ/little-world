#include "application.hpp"
#include <algorithm>
#include <stdexcept>

namespace terrarium {
namespace {
void pace_frame(std::chrono::steady_clock::time_point started, double refresh_rate) {
    const double budget = 1.0 / std::clamp(refresh_rate, 30.0, 144.0);
    const double spent = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    sengine::sleep_for(budget - spent);
}
}
application::application() {
    pointer.focus(display.metrics().focused);
    pointer.request(frontend.walking());
    sync_pointer();
}

void application::run() {
    auto previous = std::chrono::steady_clock::now();
    while (running) {
        auto input = poll_events();
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const bool focused = display.metrics().focused;
        advance_world(input, elapsed, focused);
        const auto viewport = display.metrics();
        if (viewport.minimized || !viewport.width || !viewport.height) {
            sengine::sleep_for(.025);
            continue;
        }
        draw_frame(input, viewport, elapsed, focused);
        pace_frame(now, viewport.refresh_rate);
    }
    frontend.save_preferences();
    if (!game.save())
        throw std::runtime_error(game.error());
}
}
