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
    brass{197, 170, 111, 255};
}
void user_interface::panel_text::text(const std::string& value, float x, float y, float size,
                                      sengine::drawing::color color, bool display) const {
    if (!measure_only_)
        ui_.text(value, x, y, size, color, display);
}
float user_interface::panel_text::wrapped(const std::string& value, float x, float y, float width, float size,
                                          sengine::drawing::color color) const {
    return ui_.wrapped(value, x, y, width, size, color, measure_only_);
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
    const auto lines = sengine::layout::wrap_text(value, width, [this, size](const std::string& line) {
        return measure_text_ex(body_, line.c_str(), size, 0).x;
    });
    for (const auto& line : lines) {
        if (!measure_only)
            text(line, x, y, size, color);
        y += size * 1.45f;
    }
    return y;
}

namespace {
std::vector<std::uint8_t> paper_pixels() {
    std::vector<std::uint8_t> pixels(64 * 64 * 4);
    std::uint32_t noise = 723;
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        noise = noise * 1664525u + 1013904223u;
        const int delta = (int(noise >> 28) - 8) / 2;
        pixels[offset] = 235 + delta;
        pixels[offset + 1] = 224 + delta;
        pixels[offset + 2] = 198 + delta;
        pixels[offset + 3] = 255;
    }
    return pixels;
}
}
void user_interface::paper(sengine::drawing::rect r) {
    for (int i = 7; i > 0; --i)
        draw_rectangle_rec({r.x - i * .4f, r.y + i * .9f, r.width + i * .8f, r.height + i * .4f},
                           rgb(24, 30, 22, 6));
    if (!paper_image_)
        paper_image_ = upload_image(64, 64, paper_pixels());
    draw_tiled_image(paper_image_, r, {128, 128});
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
    return focused && is_key_pressed(sengine::key_code::enter) && !editing_;
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
    return focused && is_key_pressed(sengine::key_code::enter) && !editing_;
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
        if (is_key_pressed(sengine::key_code::left))
            value = std::max(low, value - (high - low) / 50);
        if (is_key_pressed(sengine::key_code::right))
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
void user_interface::draw(world_state& world, greenhouse_controls& camera, double backlog) {
    sync_scale();
    begin_canvas();
    widget_ = 0;
    set_mouse_cursor(cursor::arrow);
    if (is_mouse_button_released())
        active_slider_ = -1;
    if (is_mouse_button_pressed())
        keyboard_focus_ = false;
    if (is_key_pressed(sengine::key_code::tab) && !editing_) {
        keyboard_focus_ = true;
        focused_ =
            (focused_ + (is_key_down(sengine::key_code::left_shift) ? std::max(1, widget_count_) - 1 : 1) +
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
    habitat_heading(world);
    panel_controls();
    tool_controls();
    time_controls();
    view_controls(camera, backlog);
    active_panel(world, camera);
    habitat_status();
    widget_count_ = widget_;
    end_transform();
}
}
