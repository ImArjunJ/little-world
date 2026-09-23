#include "drawing_color.hpp"
#include "greenhouse_controls.hpp"
#include "portraits.hpp"
#include "sengine/text_layout.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <format>
namespace terrarium {
using namespace sengine::drawing;
namespace {
constexpr sengine::drawing::color cream{239, 232, 206, 255}, brass{197, 170, 111, 255};
}
void user_interface::habitat_heading(world_state& world) {
    const float w = width();
    auto title = garden_name.empty() ? std::string("little world") : garden_name;
    float title_size =
        std::min(40.f, (w - 240) / std::max(1.f, measure_text_ex(display_, title.c_str(), 40, 0).x) * 40);
    text(title, 26, 20, title_size, cream, true, true);
    text(std::format("{}  /  Day {}", world.season(), static_cast<int>(world.day()) + 1), 29, 65, 15, cream,
         false, true);
    if (!editing_ && is_key_pressed(sengine::key_code::v))
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
}
void user_interface::panel_button(ui_control c, panel_kind panel, int symbol, const char* hint) {
    if (medallion(control_bounds(c), symbol, hint, panel_ == panel)) {
        panel_ = panel_ == panel ? panel_kind::none : panel;
        scroll_ = 0;
        editing_ = false;
    }
}
void user_interface::panel_controls() {
    panel_button(ui_control::soil_cell, panel_kind::soil_cell, 1, "The living soil  [N]");
    panel_button(ui_control::weather, panel_kind::weather, 8, "Weather  [C]");
    if (!editing_ && is_key_pressed(sengine::key_code::n)) {
        panel_ = panel_ == panel_kind::soil_cell ? panel_kind::none : panel_kind::soil_cell;
        scroll_ = 0;
    }
    panel_button(ui_control::journal, panel_kind::journal, 9, "Field journal  [J]");
    if (medallion(control_bounds(ui_control::return_to_world), 11, "Return to the greenhouse  [Esc]"))
        request = ui_request::greenhouse;
    if (!editing_ && is_key_pressed(sengine::key_code::c)) {
        panel_ = panel_ == panel_kind::weather ? panel_kind::none : panel_kind::weather;
        scroll_ = 0;
    }
    if (!editing_ && is_key_pressed(sengine::key_code::j)) {
        panel_ = panel_ == panel_kind::journal ? panel_kind::none : panel_kind::journal;
        scroll_ = 0;
    }
}
void user_interface::tool_controls() {
    constexpr std::array names{"Look closer", "Fern",  "Clover",   "Wildflower",
                               "Aphid",       "Snail", "Ladybird", "Watering can"};
    for (int i = 0; i < 8; ++i) {
        auto r = tool_bounds(static_cast<garden_tool>(i));
        if (medallion(r, i, names[i], static_cast<int>(tool) == i))
            tool = static_cast<garden_tool>(i);
        text(std::to_string(i + 1), r.x + r.width / 2 - 3, r.y + r.height + 8, 13, cream, false, true);
    }
}
void user_interface::time_controls() {
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
            (keyboard_focus_ && focused_ == id && is_key_pressed(sengine::key_code::enter)))
            speed = speeds[i];
    }
}
void user_interface::view_controls(greenhouse_controls& camera, double backlog) {
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
}
void user_interface::active_panel(world_state& world, greenhouse_controls& camera) {
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
}
void user_interface::habitat_status() {
    const float w = width();
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
}
}
