#include "ui.hpp"
#include "drawing_color.hpp"
#include "greenhouse_controls.hpp"
#include "portraits.hpp"
#include "sengine/text_layout.hpp"
#include <algorithm>
#include <cmath>
#include <format>
namespace terrarium {
using namespace sengine::drawing;
namespace {
constexpr sengine::drawing::color ink{57, 62, 43, 255}, faded{106, 108, 82, 255}, cream{239, 232, 206, 255},
    brass{197, 170, 111, 255}, dark{40, 50, 30, 240};
}
float user_interface::width() const {
    return canvas_width() / scale_;
}
float user_interface::height() const {
    return canvas_height() / scale_;
}
float user_interface::footer_y() const {
    return height() - 82;
}
float user_interface::time_y() const {
    return height() - (width() < 1180 ? 145 : 65);
}
sengine::drawing::point2 user_interface::mouse() const {
    auto p = get_mouse_position();
    return {p.x / scale_, p.y / scale_ - entrance_};
}
sengine::drawing::rect user_interface::scaled(sengine::drawing::rect r) const {
    return {r.x * scale_, (r.y + entrance_) * scale_, r.width * scale_, r.height * scale_};
}
void user_interface::sync_scale() {
    float dpi = automatic_dpi ? std::max(1.f, display_scale().x) : 1;
    scale_ = std::clamp(interface_size * dpi, .75f, 3.f);
    scale_ = std::min(scale_, std::min(canvas_width() / 600.f, canvas_height() / 640.f));
    float elapsed = static_cast<float>(get_time() - screen_since_);
    float phase = std::clamp(elapsed / .42f, 0.f, 1.f);
    entrance_ = reduced_motion || screen_ == screen_kind::habitat || screen_ == screen_kind::greenhouse
                    ? 0
                    : 14 * std::pow(1 - phase, 3);
}
void user_interface::begin_canvas() {
    begin_transform({{0, entrance_ * scale_}, {0, 0}, scale_});
}
void user_interface::show(screen_kind screen) {
    if (screen == screen_kind::gardens &&
        (screen_ == screen_kind::home || screen_ == screen_kind::greenhouse))
        gardens_from_ = screen_;
    if (screen == screen_kind::new_garden && screen_ != screen_kind::construction)
        draft_from_ = screen_;
    if (screen == screen_kind::setup && screen_ != screen_kind::setup)
        setup_from_ = screen_;
    if (screen == screen_kind::welcome && screen_ != screen_kind::welcome)
        welcome_from_ = screen_ == screen_kind::options ? screen_kind::options : screen_kind::greenhouse;
    if (screen == screen_kind::options && screen_ != screen_kind::welcome && screen_ != screen_kind::setup)
        options_from_greenhouse_ = screen_ == screen_kind::greenhouse;
    screen_ = screen;
    screen_since_ = get_time();
    scroll_ = 0;
    hover_.fill(0);
    focused_ = -1;
    editing_ = false;
    active_slider_ = -1;
    if (screen == screen_kind::setup)
        setup_step_ = 0;
    if (screen == screen_kind::welcome)
        welcome_step_ = 0;
}
user_interface::user_interface() {
    body_.face = 0;
    display_.face = 1;
}
sengine::drawing::rect user_interface::tool_bounds(garden_tool t) const {
    float pitch = width() < 760 ? 54 : 60;
    float start = (width() - pitch * 8) / 2;
    return {start + static_cast<int>(t) * pitch + 4, footer_y(), pitch - 8, pitch - 8};
}
sengine::drawing::rect user_interface::panel_bounds() const {
    float w = width(), h = height();
    float pw = panel_ == panel_kind::journal ? std::min(760.f, w - 40) : std::min(356.f, w - 40);
    float x = panel_ == panel_kind::journal ? (w - pw) / 2 : w - pw - 24;
    return {x, 104, pw, std::min(panel_ == panel_kind::journal ? 650.f : 720.f, h - 250)};
}
sengine::drawing::rect user_interface::control_bounds(ui_control c) const {
    float w = static_cast<float>(width());
    auto p = panel_bounds();
    switch (c) {
    case ui_control::pause:
        return {24, time_y(), 40, 40};
    case ui_control::soil_cell:
        return {w - 222, 27, 40, 40};
    case ui_control::weather:
        return {w - 172, 27, 40, 40};
    case ui_control::journal:
        return {w - 122, 27, 40, 40};
    case ui_control::return_to_world:
        return {w - 72, 27, 40, 40};
    case ui_control::overview:
        return {w - 118, time_y(), 40, 40};
    case ui_control::observe:
        return {w - 68, time_y(), 40, 40};
    case ui_control::photograph:
        return {w - 168, time_y(), 40, 40};
    case ui_control::follow:
        return {p.x + 24, p.y + 302 - scroll_, p.width - 48, 38};
    case ui_control::favourite:
        return {p.x + p.width - 56, p.y + 216 - scroll_, 32, 32};
    case ui_control::name:
        return {p.x + 24, p.y + 216 - scroll_, p.width - 88, 40};
    case ui_control::save:
        return {p.x + p.width - 252, p.y + 54 - scroll_, 108, 34};
    case ui_control::load:
        return {p.x + p.width - 132, p.y + 54 - scroll_, 108, 34};
    }
    return {};
}
bool user_interface::over_garden(sengine::drawing::point2 p) const {
    if (screen_ != screen_kind::habitat)
        return false;
    p = {p.x / scale_, p.y / scale_};
    if (observation)
        return true;
    auto panel_hit = panel_bounds();
    panel_hit.x -= 18;
    panel_hit.y -= 18;
    panel_hit.width += 36;
    panel_hit.height += 36;
    if (panel_ != panel_kind::none && check_collision_point_rec(p, panel_hit))
        return false;
    for (auto c :
         {ui_control::pause, ui_control::soil_cell, ui_control::weather, ui_control::journal,
          ui_control::return_to_world, ui_control::overview, ui_control::observe, ui_control::photograph})
        if (check_collision_point_rec(p, control_bounds(c)))
            return false;
    if (check_collision_point_rec(p, {20, time_y() - 3, 264, 48}))
        return false;
    for (int i = 0; i < 8; ++i) {
        auto r = tool_bounds(static_cast<garden_tool>(i));
        r.height += 20;
        if (check_collision_point_rec(p, r))
            return false;
    }
    return true;
}
void user_interface::toast(std::string message) {
    notification_ = std::move(message);
    notify_until_ = get_time() + 4;
}
void user_interface::text(const std::string& value, float x, float y, float size,
                          sengine::drawing::color color, bool display, bool shadow) {
    sengine::drawing::font f = display ? display_ : body_;
    if (shadow)
        draw_text_ex(f, value.c_str(), {x + 1, y + 2}, size, 0, rgb(20, 30, 20, 170));
    draw_text_ex(f, value.c_str(), {x, y}, size, 0, color);
}
float user_interface::wrapped(const std::string& value, float x, float y, float width, float size,
                              sengine::drawing::color color, bool measure_only) {
    const auto lines = sengine::layout::wrap_text(value, width, [&](const std::string& line) {
        return measure_text_ex(body_, line.c_str(), size, 0).x;
    });
    for (const auto& line : lines) {
        if (!measure_only)
            text(line, x, y, size, color);
        y += size * 1.45f;
    }
    return y;
}

void user_interface::paper(sengine::drawing::rect r) {
    for (int i = 7; i > 0; --i)
        draw_rectangle_rec({r.x - i * .4f, r.y + i * .9f, r.width + i * .8f, r.height + i * .4f},
                           rgb(24, 30, 22, 6));
    draw_paper_rect(r);
    draw_rectangle_lines_ex({r.x + 7, r.y + 7, r.width - 14, r.height - 14}, 1, rgb(181, 169, 132, 150));
    draw_line_ex({r.x + 17, r.y + 20}, {r.x + 17, r.y + r.height - 20}, 1, rgb(181, 169, 132, 100));
}
float user_interface::hover_ease(int id, bool active) {
    auto& value = hover_[static_cast<std::size_t>(id) % hover_.size()];
    float target = active ? 1.f : 0.f;
    value = reduced_motion ? target
                           : value + (target - value) * (1 - std::exp(-14 * std::min(get_frame_time(), .1f)));
    return value;
}
bool user_interface::button(sengine::drawing::rect r, const std::string& value, bool active) {
    int id = widget_++;
    bool hover =
        check_collision_point_rec(mouse(), r) && (!clipped_ || check_collision_point_rec(mouse(), clip_));
    bool focused = keyboard_focus_ && focused_ == id;
    if (focused && clipped_) {
        if (r.y < clip_.y)
            scroll_ -= clip_.y - r.y;
        else if (r.y + r.height > clip_.y + clip_.height)
            scroll_ += r.y + r.height - clip_.y - clip_.height;
    }
    if (hover)
        set_mouse_cursor(cursor::hand);
    float glow = hover_ease(id, hover || focused);
    if (active)
        draw_rectangle_rec(r, color_lerp(rgb(73, 87, 50), rgb(96, 112, 63), glow));
    else
        draw_rectangle_rec(r, fade(rgb(196, 183, 143), glow * .6f));
    draw_line_ex({r.x, r.y + r.height - 1}, {r.x + r.width, r.y + r.height - 1}, active ? 2 : 1,
                 active ? brass : rgb(178, 167, 130));
    if (focused)
        draw_rectangle_lines_ex({r.x - 2, r.y - 2, r.width + 4, r.height + 4}, 1, brass);
    float fs = 17;
    sengine::drawing::point2 size = measure_text_ex(body_, value.c_str(), fs, 0);
    if (size.x > r.width - 10) {
        fs *= std::max(.7f, (r.width - 10) / size.x);
        size = measure_text_ex(body_, value.c_str(), fs, 0);
    }
    text(value, r.x + (r.width - size.x) / 2, r.y + (r.height - size.y) / 2, fs, active ? cream : ink);
    if (hover && is_mouse_button_pressed()) {
        focused_ = id;
        return true;
    }
    return focused && is_key_pressed(SDL_SCANCODE_RETURN) && !editing_;
}
void user_interface::icon(int id, sengine::drawing::point2 p, float s, sengine::drawing::color c) {
    float u = s / 32;
    auto project = [&](float x, float y) { return sengine::drawing::point2{p.x + x * u, p.y + y * u}; };
    auto line = [&](float x, float y, float a, float b, float thick = 1.5f) {
        draw_line_ex(project(x, y), project(a, b), thick * u, c);
    };
    auto circle = [&](float x, float y, float r) { draw_circle_lines(project(x, y), r * u, c); };
    if (id == 0) {
        circle(-3, -3, 8);
        line(3, 3, 12, 12, 2.3f);
        line(-8, -5, -4, -8, 1);
    } else if (id == 1) {
        line(-7, 12, 7, -13);
        for (int i = 0; i < 6; ++i) {
            float x = -5 + i * 2, y = 8 - i * 3.5f;
            line(x, y, x - 8 + i * .7f, y - 4);
            line(x, y, x + 7 - i * .65f, y - 1);
        }
    } else if (id == 2) {
        line(0, 3, -4, 13);
        for (int i = 0; i < 3; ++i) {
            float a = i * 2 * pi / 3 - pi / 2;
            draw_circle(project(std::cos(a) * 5, std::sin(a) * 5), 5 * u, c);
        }
        draw_circle(project(0, 0), 1.5f * u, dark);
    } else if (id == 3) {
        line(0, -2, 0, 13);
        line(0, 8, -7, 4);
        line(0, 5, 6, 2);
        for (int i = 0; i < 6; ++i) {
            float a = i * 2 * pi / 6;
            draw_circle(project(std::cos(a) * 6, -5 + std::sin(a) * 6), 3.2f * u, c);
        }
        draw_circle(project(0, -5), 3 * u, brass);
    } else if (id == 4 || id == 6) {
        draw_ellipse(static_cast<int>(p.x), static_cast<int>(p.y + u), 7 * u, 10 * u,
                     id == 6 ? rgb(187, 108, 80) : c);
        circle(0, -10, 3);
        for (int i = 0; i < 3; ++i) {
            float y = -5 + i * 5;
            line(-6, y, -11, y - 3);
            line(6, y, 11, y - 3);
        }
        line(0, -6, 0, 9, 1);
        if (id == 6)
            for (float side : {-1.f, 1.f})
                for (int j = 0; j < 2; ++j)
                    draw_circle(project(side * 3, -2 + j * 6), 1.5f * u, dark);
    } else if (id == 5) {
        line(-12, 10, 11, 10, 2);
        line(11, 10, 12, 2);
        line(12, 2, 9, -3);
        line(12, 2, 15, -3);
        circle(-2, 1, 8);
        for (int i = 0; i < 40; ++i) {
            float a = i * .19f, b = (i + 1) * .19f, r = 1 + i * .12f;
            draw_line_ex(project(-2 + std::cos(a) * r, 1 + std::sin(a) * r),
                         project(-2 + std::cos(b) * (r + .12f), 1 + std::sin(b) * (r + .12f)), u, c);
        }
    } else if (id == 7) {
        draw_triangle(project(0, -13), project(-7, 1), project(7, 1), c);
        draw_circle(project(0, 2), 7 * u, c);
        line(-3, 0, -3, 4, 1);
    } else if (id == 8) {
        circle(0, 0, 6);
        for (int i = 0; i < 8; ++i) {
            float a = i * pi / 4;
            line(std::cos(a) * 9, std::sin(a) * 9, std::cos(a) * 13, std::sin(a) * 13);
        }
    } else if (id == 9) {
        line(0, -9, 0, 12);
        line(-12, -10, -12, 9);
        line(12, -10, 12, 9);
        line(-12, -10, -2, -8);
        line(12, -10, 2, -8);
        line(-12, 9, 0, 12);
        line(12, 9, 0, 12);
        for (int j = 0; j < 3; ++j) {
            line(-9, -5 + j * 4, -3, -4 + j * 4, 1);
            line(3, -4 + j * 4, 9, -5 + j * 4, 1);
        }
    } else if (id == 10) {
        for (int i = 0; i < 3; ++i)
            draw_circle(project(-8 + i * 8, 0), 2 * u, c);
    } else if (id == 11) {
        line(-12, 0, 0, -11);
        line(0, -11, 12, 0);
        line(-8, -2, -8, 11);
        line(8, -2, 8, 11);
        line(-8, 11, 8, 11);
        line(-2, 11, -2, 4);
        line(3, 11, 3, 4);
    } else if (id == 12) {
        draw_ellipse_lines(static_cast<int>(p.x), static_cast<int>(p.y), 13 * u, 7 * u, c);
        circle(0, 0, 4);
    } else if (id == 13) {
        line(-4, -9, -4, 9, 3);
        line(4, -9, 4, 9, 3);
    } else if (id == 14) {
        draw_triangle(project(-5, -9), project(-5, 9), project(9, 0), c);
    } else if (id == 15) {
        for (int i = 0; i < 10; ++i) {
            float a = i * pi / 5 - pi / 2, b = (i + 1) * pi / 5 - pi / 2, r = i % 2 ? 5 : 12,
                  t = i % 2 ? 12 : 5;
            line(std::cos(a) * r, std::sin(a) * r, std::cos(b) * t, std::sin(b) * t);
        }
    } else if (id == 16) {
        line(-9, 10, 8, -10, 3);
        line(-10, 12, -7, 11);
        line(-8, 14, 10, 14, 1);
    } else if (id == 17) {
        circle(0, 0, 8);
        line(-13, 0, -5, 0);
        line(5, 0, 13, 0);
        line(0, -13, 0, -5);
        line(0, 5, 0, 13);
    } else if (id == 18) {
        line(-7, -7, 7, 7);
        line(-7, 7, 7, -7);
    } else if (id == 19) {
        line(-12, -7, -12, 10);
        line(-12, 10, 12, 10);
        line(12, 10, 12, -7);
        line(12, -7, 5, -7);
        line(5, -7, 3, -11);
        line(3, -11, -4, -11);
        line(-4, -11, -6, -7);
        line(-6, -7, -12, -7);
        circle(0, 1, 5);
    }
}
void user_interface::medallion_face(sengine::drawing::rect r, int symbol, bool active, float glow) {
    sengine::drawing::point2 center{r.x + r.width / 2, r.y + r.height / 2};
    draw_circle({center.x, center.y + 3}, r.width / 2, rgb(20, 29, 20, 75));
    draw_circle(center, r.width / 2, active ? rgb(101, 110, 69, 245) : rgb(40, 50, 30, 225));
    draw_circle_lines(center, r.width / 2 - 1 + glow,
                      active ? brass : color_lerp(rgb(143, 146, 108, 180), brass, glow));
    icon(symbol, center, r.width * .57f, active ? rgb(255, 239, 186) : cream);
}
bool user_interface::medallion(sengine::drawing::rect r, int symbol, const std::string& hint, bool active) {
    int id = widget_++;
    sengine::drawing::point2 center{r.x + r.width / 2, r.y + r.height / 2};
    bool hover = check_collision_point_circle(mouse(), center, r.width / 2) &&
                 (!clipped_ || check_collision_point_rec(mouse(), clip_));
    bool focused = keyboard_focus_ && focused_ == id;
    medallion_face(r, symbol, active, hover_ease(id, hover || focused));
    if (hover) {
        set_mouse_cursor(cursor::hand);
        float width = measure_text_ex(body_, hint.c_str(), 14, 0).x;
        float y = r.y > height() / 2 ? r.y - 30 : r.y + r.height + 9;
        text(hint, std::clamp(center.x - width / 2, 8.f, this->width() - width - 8), y, 14, cream, false,
             true);
    }
    if (focused)
        draw_circle_lines(center, r.width / 2 + 3, brass);
    if (hover && is_mouse_button_pressed()) {
        focused_ = id;
        return true;
    }
    return focused && is_key_pressed(SDL_SCANCODE_RETURN) && !editing_;
}
void user_interface::slider(sengine::drawing::rect r, const std::string& title, float& value, float low,
                            float high, const std::string& unit) {
    int id = widget_++;
    bool focus = keyboard_focus_ && focused_ == id;
    float ratio = std::clamp((value - low) / (high - low), 0.f, 1.f);
    text(title, r.x, r.y, 16, ink);
    std::string amount = unit == " cm" ? std::format("{:.1f}{}", value, unit)
                                       : std::format("{:.0f}{}", value * (unit == "%" ? 100 : 1), unit);
    float width = measure_text_ex(body_, amount.c_str(), 15, 0).x;
    text(amount, r.x + r.width - width, r.y, 15, faded);
    sengine::drawing::rect hit{r.x - 5, r.y + 22, r.width + 10, 28};
    bool hover =
        check_collision_point_rec(mouse(), hit) && (!clipped_ || check_collision_point_rec(mouse(), clip_));
    if (hover) {
        set_mouse_cursor(cursor::hand);
        if (is_mouse_button_pressed()) {
            active_slider_ = id;
            focused_ = id;
        }
    }
    if (active_slider_ == id && is_mouse_button_down())
        value = low + std::clamp((mouse().x - r.x) / r.width, 0.f, 1.f) * (high - low);
    if (focus) {
        if (is_key_pressed(SDL_SCANCODE_LEFT))
            value = std::max(low, value - (high - low) / 50);
        if (is_key_pressed(SDL_SCANCODE_RIGHT))
            value = std::min(high, value + (high - low) / 50);
    }
    ratio = std::clamp((value - low) / (high - low), 0.f, 1.f);
    draw_line_ex({r.x, r.y + 34}, {r.x + r.width, r.y + 34}, 2, rgb(155, 146, 110));
    for (int i = 0; i <= 10; ++i)
        draw_line_ex({r.x + i * r.width / 10, r.y + 31}, {r.x + i * r.width / 10, r.y + 37}, 1,
                     rgb(160, 151, 119));
    draw_circle({r.x + ratio * r.width, r.y + 34}, 6, rgb(95, 106, 60));
    draw_circle_lines({r.x + ratio * r.width, r.y + 34}, 7, focus ? ink : brass);
}
void user_interface::weather_panel(world_state& world, sengine::drawing::rect r) {
    float x = r.x + 28, y = r.y, w = r.width - 56;
    text("A little weather", x, y + 22, 32, ink, true);
    text(std::format("{}  /  Day {}", world.season(), static_cast<int>(world.day()) + 1), x, y + 62, 15,
         faded);
    if (button({x, y + 104, w, 42},
               world.day() < world.climate.rain_until ? "Another passing shower" : "Call a passing shower",
               world.day() < world.climate.rain_until))
        world.rain();
    slider({x, y + 176, w, 48}, "Room temperature", world.climate.temperature, 0, 45, " C");
    slider({x, y + 245, w, 48}, "Sunlight", world.climate.sunlight, 0, 1, "%");
    float angle = world.climate.sun_angle * 180 / pi;
    slider({x, y + 314, w, 48}, "Move the sun", angle, 0, 360, " deg");
    world.climate.sun_angle = angle * pi / 180;
    if (button({x, y + 391, w, 36},
               world.climate.seasons ? "Let the seasons turn" : "Keep an endless spring"))
        world.climate.seasons = !world.climate.seasons;
    draw_line_ex({x, y + 454}, {x + w, y + 454}, 1, rgb(176, 164, 129));
    text("The water in this world", x, y + 478, 25, ink, true);
    wrapped(world.design().form == vessel_form::legacy
                ? std::format("Soil: {:.0f}% damp / Pond: {:.0f}% full", world.moisture() * 100,
                              world.pond_level() * 100)
                : std::format("Soil: {:.0f}% damp / Drainage: {:.0f}% full", world.moisture() * 100,
                              world.water_cycle().pond / world.reservoir_capacity() * 100),
            x, y + 516, w, 15, faded);
    wrapped("Rain feeds the pool and soil. Evaporation cools the garden; cooler glass collects the water.", x,
            y + 548, w, 15, faded);
    if (button({x, y + 628, w, 36}, "Take the watering can  [8]")) {
        tool = garden_tool::water;
        close_panel();
    }
    text("Read the garden  [V]", x, y + 700, 25, ink, true);
    if (button({x, y + 743, w, 38}, "Water in the soil", environment_view == 1))
        environment_view = environment_view == 1 ? 0 : 1;
    if (button({x, y + 796, w, 38}, "Sun through the leaves", environment_view == 2))
        environment_view = environment_view == 2 ? 0 : 2;
    if (environment_view && button({x, y + 849, w, 34}, "Return to the natural view"))
        environment_view = 0;
    draw_line_ex({x, y + 910}, {x + w, y + 910}, 1, rgb(176, 164, 129));
    text("Under the glass", x, y + 934, 25, ink, true);
    wrapped(std::format("Garden {:.1f} C / Glass {:.1f} C\nAir {:.0f}% humid / {:.1f} g on the glass",
                        world.temperature(), world.vessel().glass_c, world.humidity() * 100,
                        world.vessel().film_kg * 1000),
            x, y + 978, w, 16, ink);
    wrapped(std::format("Daylight {:.0f} W/m2. {}", world.irradiance(),
                        world.daylight() < .01f             ? "The garden is resting after sunset."
                        : world.climate.room_exposure > .8f ? "This is a bright window spot."
                        : world.climate.room_exposure < .3f ? "This is a sheltered shelf spot."
                                                            : "This is gentle desk light."),
            x, y + 1060, w, 15, faded);
    slider({x, y + 1150, w, 48}, "Vent opening", world.climate.opening, 0, 1, "%");
    if (button({x, y + 1215, w, 36}, world.climate.opening > 0 ? "Close the vents" : "Open the vents"))
        world.climate.opening = world.climate.opening > 0 ? 0 : 1;
    wrapped("Closed vents keep water inside. Opening them exchanges moist air with the room and helps the "
            "garden cool.",
            x, y + 1270, w, 15, faded);
    if (button({x, y + 1362, w, 36}, world.climate.day_night ? "Day and night: on" : "Day and night: off"))
        world.climate.day_night = !world.climate.day_night;
    if (world.design().form != vessel_form::legacy) {
        const auto& design = world.design();
        text(vessel_name(design.form), x, y + 1440, 25, ink, true);
        wrapped(
            std::format(
                "{} / {:.1f} cm soil / {:.1f} cm gravel\n{:.1f} L stored below the roots, out of {:.1f} L.",
                soil_name(design.mix), design.soil_depth * 100, design.drainage_depth * 100,
                world.water_cycle().pond, world.reservoir_capacity()),
            x, y + 1482, w, 15, faded);
    }
}
void user_interface::plant_panel(world_state& world, greenhouse_controls& camera, sengine::drawing::rect r) {
    float x = r.x + 26, y = r.y, w = r.width - 52;
    const auto* plant = world.plant(selected_plant);
    if (!plant) {
        text("Back to the soil", x, y + 24, 30, ink, true);
        wrapped("This plant has returned to the leaf litter. Choose another plant to see how it is growing.",
                x, y + 80, w, 18, faded);
        return;
    }
    auto health = world.plant_health(*plant);
    text("A PAGE FROM THE FIELD JOURNAL", x, y + 24, 11, faded);
    end_transform();
    draw_plant_portrait(*plant, scaled({x + w / 2 - 90, y + 42, 180, 156}));
    begin_canvas();
    text(plant_name(plant->kind), x, y + 208, 32, ink, true);
    text(std::format("{:.1f} days growing{}", plant->age, plant->parent ? "  /  self-seeded" : ""), x,
         y + 249, 14, faded);
    text(plant_state_name(health.state), x, y + 284, 24, ink, true);
    wrapped(plant_advice(health.state), x, y + 320, w, 17, ink);
    std::array values{health.moisture, health.light, health.nutrients};
    constexpr std::array labels{"Water at the roots", "Light through the canopy", "Food in the soil"};
    for (int i = 0; i < 3; ++i) {
        float row = y + 422 + i * 55;
        text(labels[i], x, row, 15, faded);
        text(std::format("{:.0f}%", values[i] * 100), x + w - 38, row, 15, ink);
        draw_line_ex({x, row + 28}, {x + w, row + 28}, 4, rgb(199, 190, 155));
        draw_line_ex({x, row + 28}, {x + w * std::clamp(values[i], 0.f, 1.f), row + 28}, 4,
                     rgb(101, 122, 75));
    }
    wrapped(std::format("Leaf mass: {:+.1f}% of a full plant per day, before grazing.",
                        (health.growth - health.loss) * 100 / 1.6f),
            x, y + 583, w, 15, ink);
    if (button({x, y + 635, w, 40}, "A little water for these roots")) {
        world.water(plant->position, .18);
        toast("A drink for this patch of soil.");
    }
    if (button({x, y + 691, w, 38}, "See the sunlight  [V]", environment_view == 2))
        environment_view = environment_view == 2 ? 0 : 2;
    if (button({x, y + 744, w, 38}, "Look closer"))
        camera.focus(plant->position);
    text("A place to flourish", x, y + 810, 25, ink, true);
    wrapped(
        plant->kind == plant_kind::fern
            ? "Ferns prefer damp roots and gentle light. Taller neighbours can shelter them from strong sun."
        : plant->kind == plant_kind::clover ? "Clover grows close to the soil. Its leaves feed aphids and "
                                              "snails; keep enough growing to share."
                                            : "Flowers like bright light. Their nectar feeds ladybirds "
                                              "between hunts, but aphids are needed to raise young.",
        x, y + 849, w, 17, faded);
}
void user_interface::creature_panel(world_state& world, greenhouse_controls& camera,
                                    sengine::drawing::rect r) {
    float x = r.x + 26, y = r.y, w = r.width - 52;
    if (previous_selected_ != selected) {
        previous_selected_ = selected;
        child_page_ = 0;
        editing_ = false;
    }
    const auto* ancestor = world.ancestor(selected);
    if (!ancestor) {
        text("Small neighbours", x, y + 24, 30, ink, true);
        wrapped("Choose the looking glass, then click a little life in the garden.", x, y + 80, w, 17, faded);
        if (!world.creatures().empty() && button({x, y + 175, w, 40}, "Meet someone")) {
            selected = world.creatures().front().id;
            camera.focus(world.creatures().front().position);
        }
        return;
    }
    text("A PAGE FROM THE FIELD JOURNAL", x, y + 24, 11, faded);
    const creature_state* live = world.creature(selected);
    creature_state portrait_creature = live ? *live : creature_state{};
    if (!live)
        portrait_creature.age = 10;
    portrait_creature.species = ancestor->species;
    portrait_creature.genes = ancestor->genes;
    end_transform();
    draw_creature_portrait(portrait_creature, scaled({x + w / 2 - 90, y + 48, 180, 156}));
    begin_canvas();
    if (editing_) {
        int codepoint;
        while ((codepoint = get_char_pressed()) > 0) {
            if (codepoint >= 32 && codepoint != 127) {
                int bytes = 0;
                const char* encoded = codepoint_to_utf8(codepoint, &bytes);
                if (name_buffer_.size() + bytes <= 64)
                    name_buffer_.append(encoded, bytes);
            }
        }
        if (is_key_pressed(SDL_SCANCODE_BACKSPACE) && !name_buffer_.empty()) {
            auto i = name_buffer_.size() - 1;
            while (i > 0 && (static_cast<unsigned char>(name_buffer_[i]) & 0xc0) == 0x80)
                --i;
            name_buffer_.erase(i);
        }
        if (is_key_pressed(SDL_SCANCODE_RETURN)) {
            world.rename(selected, name_buffer_);
            editing_ = false;
            toast("A name to remember.");
        }
        if (is_key_pressed(SDL_SCANCODE_ESCAPE))
            editing_ = false;
        text(name_buffer_ + (static_cast<int>(get_time() * 2) % 2 ? "|" : ""), x, y + 217, 25, ink, true);
        draw_line_ex({x, y + 251}, {x + w - 44, y + 251}, 1, brass);
        text("Enter to keep it  /  Esc to cancel", x, y + 259, 12, faded);
    } else {
        text(world.name(selected), x, y + 215, 31, ink, true);
        sengine::drawing::rect rename_hit{x, y + 213, w - 45, 40};
        if (check_collision_point_rec(mouse(), rename_hit) && check_collision_point_rec(mouse(), clip_)) {
            set_mouse_cursor(cursor::text);
            if (is_mouse_button_pressed()) {
                editing_ = true;
                name_buffer_ = ancestor->nickname.empty() ? world.name(selected) : ancestor->nickname;
            }
        }
        icon(16, {x + w - 66, y + 239}, 17, faded);
        text(std::format("{}  /  Generation {}", species_name(ancestor->species), ancestor->generation), x,
             y + 255, 14, faded);
    }
    if (medallion({r.x + r.width - 56, y + 216, 32, 32}, 15,
                  ancestor->favourite ? "Remembered as a favourite" : "Remember this little one",
                  ancestor->favourite))
        world.toggle_favourite(selected);
    if (!editing_)
        text("Click their name to give them a new one", x, y + 280, 11, faded);
    if (live) {
        if (button({x, y + 302, w, 38},
                   followed == selected ? "Stop following  [F]" : "Follow this little one  [F]",
                   followed == selected)) {
            if (followed == selected)
                followed = 0;
            else {
                followed = selected;
                camera.focus(live->position);
            }
        }
        text(live->incubation > 0 ? "Waiting to hatch" : activity_name(live->activity), x, y + 360, 19, ink,
             true);
        text(std::format("{:.1f} days old  /  Energy {:.0f}%", live->age, live->energy / 1.5f * 100), x,
             y + 388, 14, faded);
    } else {
        wrapped(std::format("Remembered. Their story ended on day {:.1f}.", ancestor->died + 1), x, y + 309,
                w, 17, faded);
    }
    if (live) {
        wrapped(world.intention(*live), x, y + 425, w, 16, ink);
        wrapped(world.last_meal(*live), x, y + 515, w, 14, faded);
    } else {
        wrapped(death_reason(ancestor->death_cause), x, y + 385, w, 17, ink);
        if (ancestor->predator)
            wrapped("Caught by " + world.name(ancestor->predator) + ".", x, y + 450, w, 16, faded);
    }
    y += 150;
    draw_line_ex({x, y + 431}, {x + w, y + 431}, 1, rgb(181, 169, 132));
    text("Family roots", x, y + 452, 27, ink, true);
    if (ancestor->mother) {
        if (button({x, y + 491, (w - 10) / 2, 40}, world.name(ancestor->mother)))
            selected = ancestor->mother;
        if (button({x + (w + 10) / 2, y + 491, (w - 10) / 2, 40}, world.name(ancestor->father)))
            selected = ancestor->father;
        text("parents", x, y + 539, 12, faded);
    } else
        text("One of the first to call this garden home.", x, y + 496, 14, faded);
    std::vector<entity_id> children;
    for (const auto& child : world.family())
        if (child.mother == ancestor->id || child.father == ancestor->id)
            children.push_back(child.id);
    text(std::format("{} little ones", children.size()), x, y + 576, 17, ink, true);
    for (int i = 0; i < 3 && child_page_ * 3 + i < static_cast<int>(children.size()); ++i) {
        entity_id id = children[child_page_ * 3 + i];
        if (button({x, y + 606 + i * 40, w, 34}, world.name(id)))
            selected = id;
    }
    if (children.size() > 3 && button({x + w - 100, y + 745, 100, 30}, "More children"))
        child_page_ = (child_page_ + 1) % ((children.size() + 2) / 3);
    text("What runs in the family", x, y + 795, 25, ink, true);
    constexpr std::array labels{"Pace", "Size", "Fertility", "Resilience"};
    std::array values{ancestor->genes.speed, ancestor->genes.size, ancestor->genes.fertility,
                      ancestor->genes.tolerance};
    for (int i = 0; i < 4; ++i) {
        text(labels[i], x, y + 835 + i * 25, 14, faded);
        text(std::format("{:.2f}", values[i]), x + w - 38, y + 835 + i * 25, 14, ink);
    }
    wrapped("Faster feet use more food. Larger bodies need more energy. Resilience eases temperature stress, "
            "with a food cost above average.",
            x, y + 950, w, 15, faded);
    if (button({x, y + 1050, w, 36}, "Meet another neighbour")) {
        const auto& all = world.creatures();
        if (!all.empty()) {
            auto it = std::find_if(all.begin(), all.end(),
                                   [&](const creature_state& c) { return c.id == selected; });
            selected = it == all.end() || ++it == all.end() ? all.front().id : it->id;
            scroll_ = 0;
        }
    }
}
void user_interface::draw(world_state& world, greenhouse_controls& camera, double backlog) {
    sync_scale();
    begin_canvas();
    widget_ = 0;
    set_mouse_cursor(cursor::arrow);
    if (is_mouse_button_released())
        active_slider_ = -1;
    if (is_mouse_button_pressed())
        keyboard_focus_ = false;
    if (is_key_pressed(SDL_SCANCODE_TAB) && !editing_) {
        keyboard_focus_ = true;
        focused_ = (focused_ + (is_key_down(SDL_SCANCODE_LSHIFT) ? std::max(1, widget_count_) - 1 : 1) +
                    std::max(1, widget_count_)) %
                   std::max(1, widget_count_);
    }
    float w = width(), h = height();

    draw_rectangle_gradient_v(0, 0, w, 140, rgb(24, 35, 25, 176), transparent);
    draw_rectangle_gradient_v(0, h - 180, w, 180, transparent, rgb(24, 35, 25, 190));
    if (observation) {
        text("H  /  return to the field kit", 24, h - 30, 13, cream, false, true);
        widget_count_ = 0;
        end_transform();
        return;
    }
    auto title = garden_name.empty() ? std::string("little world") : garden_name;
    float title_size =
        std::min(40.f, (w - 240) / std::max(1.f, measure_text_ex(display_, title.c_str(), 40, 0).x) * 40);
    text(title, 26, 20, title_size, cream, true, true);
    text(std::format("{}  /  Day {}", world.season(), static_cast<int>(world.day()) + 1), 29, 65, 15, cream,
         false, true);
    if (!editing_ && is_key_pressed(SDL_SCANCODE_V))
        environment_view = (environment_view + 1) % 3;
    if (environment_view) {
        sengine::drawing::rect key{26, 122, 250, 57};
        draw_rectangle_rounded(key, .12f, 6, rgb(31, 43, 32, 230));
        text(environment_view == 1 ? "SOIL WATER  /  V to change" : "CANOPY LIGHT  /  V to change", 38, 132,
             12, cream);
        sengine::drawing::color low = environment_view == 1 ? rgb(181, 114, 68) : rgb(77, 74, 112);
        sengine::drawing::color high = environment_view == 1 ? rgb(63, 164, 172) : rgb(241, 201, 100);
        draw_rectangle_gradient_h(102, 157, 95, 7, low, high);
        text(environment_view == 1 ? "Dry" : "Shade", 38, 152, 13, cream);
        text(environment_view == 1 ? "Damp" : "Sun", 215, 152, 13, cream);
    }
    if (followed && world.creature(followed))
        text("Following " + world.name(followed), 29, 91, 14, cream, false, true);
    auto open = [&](ui_control c, panel_kind panel, int symbol, const char* hint) {
        if (medallion(control_bounds(c), symbol, hint, panel_ == panel)) {
            panel_ = panel_ == panel ? panel_kind::none : panel;
            scroll_ = 0;
            editing_ = false;
        }
    };
    open(ui_control::soil_cell, panel_kind::soil_cell, 1, "The living soil  [N]");
    open(ui_control::weather, panel_kind::weather, 8, "Weather  [C]");
    if (!editing_ && is_key_pressed(SDL_SCANCODE_N)) {
        panel_ = panel_ == panel_kind::soil_cell ? panel_kind::none : panel_kind::soil_cell;
        scroll_ = 0;
    }
    open(ui_control::journal, panel_kind::journal, 9, "Field journal  [J]");
    if (medallion(control_bounds(ui_control::return_to_world), 11, "Return to the greenhouse  [Esc]"))
        request = ui_request::greenhouse;
    if (!editing_ && is_key_pressed(SDL_SCANCODE_C)) {
        panel_ = panel_ == panel_kind::weather ? panel_kind::none : panel_kind::weather;
        scroll_ = 0;
    }
    if (!editing_ && is_key_pressed(SDL_SCANCODE_J)) {
        panel_ = panel_ == panel_kind::journal ? panel_kind::none : panel_kind::journal;
        scroll_ = 0;
    }
    constexpr std::array names{"Look closer", "Fern",  "Clover",   "Wildflower",
                               "Aphid",       "Snail", "Ladybird", "Watering can"};
    for (int i = 0; i < 8; ++i) {
        auto r = tool_bounds(static_cast<garden_tool>(i));
        if (medallion(r, i, names[i], static_cast<int>(tool) == i))
            tool = static_cast<garden_tool>(i);
        text(std::to_string(i + 1), r.x + r.width / 2 - 3, r.y + r.height + 8, 13, cream, false, true);
    }
    if (medallion(control_bounds(ui_control::pause), paused ? 14 : 13,
                  paused ? "Let time pass [Space]" : "Pause [Space]", paused))
        paused = !paused;
    constexpr std::array speeds{1., 10., 60., 240.};
    for (int i = 0; i < 4; ++i) {
        sengine::drawing::rect r{72.f + i * 40.f, time_y() + 4, 36, 32};
        bool active = speed == speeds[i];
        int id = widget_++;
        bool hover = check_collision_point_rec(mouse(), r);
        if (active)
            draw_line_ex({r.x + 3, r.y + 31}, {r.x + r.width - 3, r.y + 31}, 2, brass);
        if (hover)
            set_mouse_cursor(cursor::hand);
        text(std::format("{}x", static_cast<int>(speeds[i])), r.x + 3, r.y + 8, 14,
             active ? rgb(255, 238, 180) : cream, false, true);
        if ((hover && is_mouse_button_pressed()) ||
            (keyboard_focus_ && focused_ == id && is_key_pressed(SDL_SCANCODE_RETURN)))
            speed = speeds[i];
    }
    if (medallion(control_bounds(ui_control::overview), 11, "Return to the whole garden [Home]")) {
        camera.reset_camera();
        followed = 0;
    }
    if (medallion(control_bounds(ui_control::observe), 12, "Just watch [H]")) {
        observation = true;
        tool = garden_tool::inspect;
    }
    if (medallion(control_bounds(ui_control::photograph), 19, "Keep a photograph [P]"))
        request = ui_request::photo;
    if (backlog > 1)
        text("Letting time catch up carefully...", 26, time_y() - 24, 12, cream, false, true);
    if (panel_ != panel_kind::none) {
        auto rect = panel_bounds();
        paper(rect);
        clip_ = {rect.x + 20, rect.y + 12, rect.width - 40, rect.height - 24};
        auto measured_rect = rect;
        measured_rect.y -= scroll_;
        const float content = panel_ == panel_kind::creature_state ? 1250
                              : panel_ == panel_kind::plant_state  ? 1010
                              : panel_ == panel_kind::weather
                                  ? (world.design().form == vessel_form::legacy ? 1450 : 1590)
                              : panel_ == panel_kind::soil_cell ? 920
                                                                : journal_panel(world, measured_rect, true);
        const float max_scroll = std::max(0.f, content - rect.height + 24);
        if (check_collision_point_rec(mouse(), rect))
            scroll_ -= get_mouse_wheel_move() * 42;
        scroll_ = std::clamp(scroll_, 0.f, max_scroll);
        clipped_ = true;
        auto scissor = scaled(clip_);
        begin_scissor_mode(scissor.x, scissor.y, scissor.width, scissor.height);
        auto content_rect = rect;
        content_rect.y -= scroll_;
        if (panel_ == panel_kind::weather)
            weather_panel(world, content_rect);
        else if (panel_ == panel_kind::soil_cell)
            soil_panel(world, content_rect);
        else if (panel_ == panel_kind::plant_state)
            plant_panel(world, camera, content_rect);
        else if (panel_ == panel_kind::creature_state)
            creature_panel(world, camera, content_rect);
        else if (panel_ == panel_kind::journal)
            journal_panel(world, content_rect);
        end_scissor_mode();
        clipped_ = false;
        if (max_scroll > 0) {
            float thumb =
                std::clamp((rect.height - 40) * (rect.height - 24) / content, 24.f, rect.height - 40);
            draw_line_ex({rect.x + rect.width - 11, rect.y + 20},
                         {rect.x + rect.width - 11, rect.y + rect.height - 20}, 1, rgb(185, 175, 140));
            draw_line_ex(
                {rect.x + rect.width - 11, rect.y + 20 + scroll_ / max_scroll * (rect.height - 40 - thumb)},
                {rect.x + rect.width - 11,
                 rect.y + 20 + scroll_ / max_scroll * (rect.height - 40 - thumb) + thumb},
                3, rgb(143, 133, 95));
        }
        if (medallion({rect.x + rect.width - 18, rect.y - 16, 32, 32}, 18, "Close the page"))
            close_panel();
    }
    if (get_time() < notify_until_) {
        float width = std::min(w - 60, measure_text_ex(body_, notification_.c_str(), 15, 0).x + 32);
        float x = (w - width) / 2, y = footer_y() - (w < 1180 ? 100 : 48);
        draw_rectangle_rounded({x, y, width, 30}, .12f, 6, rgb(30, 40, 30, 215));
        text(notification_, x + 16, y + 7, 15, cream);
    } else if (panel_ == panel_kind::none) {
        std::string hint =
            tool == garden_tool::inspect
                ? "Click a plant or creature to meet it  /  scroll closer  /  V reads the garden"
            : tool == garden_tool::compost
                ? "Rich compost / click soil to add 0.5 g / 1 returns to inspection"
            : tool == garden_tool::mulch ? "Woody mulch / click soil to add 0.5 g / 1 returns to inspection"
            : tool == garden_tool::water ? "Hold over the soil to water a little patch"
                                         : "Click the soil to make a little room for life";
        float width = measure_text_ex(body_, hint.c_str(), 13, 0).x;
        text(hint, (w - width) / 2, footer_y() - (w < 1180 ? 80 : 30), 13, cream, false, true);
    }
    widget_count_ = widget_;
    end_transform();
}
}
