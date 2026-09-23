#include "drawing_color.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <format>
namespace terrarium {
using namespace sengine::drawing;
namespace {
constexpr sengine::drawing::color ink{57, 62, 43, 255}, faded{106, 108, 82, 255}, cream{239, 232, 206, 255};
}
void user_interface::draw_home() {
    const float w = width(), h = height();

    float x = w < 850 ? 42 : 72, y = std::max(55.f, h * .15f);
    text("a small place to come back to", x + 3, y, 16, cream);
    text("little world", x, y + 28, w < 850 ? 68 : 88, cream, true, true);
    draw_line_ex({x + 3, y + 139}, {x + 90, y + 139}, 2, rgb(202, 182, 126));
    text("Grow something. Get to know someone.", x + 3, y + 160, 17, cream);
    sengine::drawing::rect menu{x, y + 219, 304, 240};
    paper(menu);
    if (button({x + 24, menu.y + 18, 256, 46}, "Step into the greenhouse", true))
        request = ui_request::greenhouse;
    if (button({x + 24, menu.y + 76, 256, 38}, "Your gardens"))
        show(screen_kind::gardens);
    if (button({x + 24, menu.y + 126, 256, 38}, "Settings & comfort"))
        show(screen_kind::options);
    if (button({x + 24, menu.y + 176, 256, 38}, "Goodbye for now"))
        request = ui_request::quit;
}
void user_interface::draw_gardens(sengine::drawing::rect page, float t,
                                  const std::vector<garden_entry>& entries) {
    const float x = page.x + 30, y = page.y + 26, cw = page.width - 60;

    text("Your little worlds", x, y, 38, ink, true);
    text("Each garden has its own story. Time rests while you are away.", x, y + 49, 15, faded);
    if (button({x, y + 85, 220, 40}, "Plant a new garden", true)) {
        new_garden();
    }
    int count = std::max(1, static_cast<int>((page.height - 260) / 128));
    int pages = std::max(1, (static_cast<int>(entries.size()) + count - 1) / count);
    shelf_page_ = std::clamp(shelf_page_, 0, pages - 1);
    if (entries.empty()) {
        illustration({x + cw / 2, y + 300}, 170, 3, t);
        text("There is room for a little life here.", x + cw / 2 - 145, y + 385, 23, ink, true);
    }
    for (int j = 0; j < count && shelf_page_ * count + j < static_cast<int>(entries.size()); ++j) {
        const auto& e = entries[shelf_page_ * count + j];
        float yy = y + 151 + j * 128;
        draw_line_ex({x, yy - 11}, {x + cw, yy - 11}, 1, rgb(187, 176, 141));
        illustration({x + 42, yy + 51}, 62, 3, 5);
        std::string label = e.name;
        while (measure_text_ex(display_, label.c_str(), 26, 0).x > cw - 116 && label.size() > 4) {
            auto end = label.size() - 1;
            while (end > 0 && (static_cast<unsigned char>(label[end]) & 0xc0) == 0x80)
                --end;
            label.erase(end);
        }
        text(label, x + 101, yy, 26, ink, true);
        text(std::format("Day {}  /  {} little neighbours", static_cast<int>(e.day) + 1,
                         e.population[0] + e.population[1] + e.population[2]),
             x + 102, yy + 34, 14, faded);
        float action_width = std::min(105.f, (cw - 120) / 4);
        float action_pitch = action_width + (cw >= 552 ? 12.f : 8.f);
        if (button({x + 96, yy + 62, action_width, 34}, "Visit", true)) {
            request = ui_request::open;
            request_id = e.id;
        }
        if (button({x + 96 + action_pitch, yy + 62, action_width, 34}, "Rename")) {
            renaming_ = true;
            request_id = e.id;
            draft_name = e.name;
            show(screen_kind::new_garden);
        }
        if (button({x + 96 + action_pitch * 2, yy + 62, action_width, 34}, "Make a copy")) {
            request = ui_request::duplicate;
            request_id = e.id;
        }
        if (button({x + 96 + action_pitch * 3, yy + 62, action_width, 34}, "Delete")) {
            request_id = e.id;
            delete_name_ = e.name;
            show(screen_kind::delete_garden);
        }
    }
    if (pages > 1) {
        if (button({x + cw - 207, page.y + page.height - 62, 65, 34}, "Prev"))
            shelf_page_ = std::max(0, shelf_page_ - 1);
        text(std::format("{} / {}", shelf_page_ + 1, pages), x + cw - 125, page.y + page.height - 51, 14,
             faded);
        if (button({x + cw - 66, page.y + page.height - 62, 65, 34}, "Next"))
            shelf_page_ = std::min(pages - 1, shelf_page_ + 1);
    }
}
void user_interface::draw_delete_garden(sengine::drawing::rect page) {
    const float x = page.x + 30, y = page.y + 26, cw = page.width - 60;

    text("Delete this garden?", x, y, 38, ink, true);
    illustration({x + cw / 2, y + 151}, 145, 3, 5);
    float name_size = std::min(
        27.f, (cw - 24) * 27 / std::max(1.f, measure_text_ex(display_, delete_name_.c_str(), 27, 0).x));
    text(delete_name_, x + 12, y + 258, name_size, ink, true);
    wrapped("This permanently removes this garden, its creatures and family history, and its backup "
            "saves.",
            x + 12, y + 308, cw - 24, 18, faded);
    float button_width = (cw - 36) / 2;
    if (button({x + 12, page.y + page.height - 96, button_width, 44}, "Keep garden", true) ||
        is_key_pressed(sengine::key_code::escape))
        show(screen_kind::gardens);
    else if (button({x + 24 + button_width, page.y + page.height - 96, button_width, 44},
                    "Delete permanently"))
        request = ui_request::delete_garden;
}
void user_interface::draw_new_garden(sengine::drawing::rect page, float t) {
    const float x = page.x + 30, y = page.y + 26, cw = page.width - 60;

    text(renaming_ ? "A name to grow into" : "A new little beginning", x, y, 38, ink, true);
    illustration({x + cw / 2, y + 168}, 160, renaming_ ? 3 : 0, t);
    name_input({x + 12, y + 284, cw - 24, 54});
    text("Type a name / Ctrl+A to start over", x + 12, y + 353, 14, faded);
    if (button({x + 12, y + 401, cw - 24, 46}, renaming_ ? "Keep this name" : "Choose a vessel", true) ||
        is_key_pressed(sengine::key_code::enter)) {
        if (renaming_)
            request = ui_request::rename;
        else
            show(screen_kind::construction);
    }
}
void user_interface::draw_welcome(sengine::drawing::rect page, float t) {
    const float x = page.x + 30, y = page.y + 26, cw = page.width - 60;

    constexpr std::array titles{"A home for little worlds", "A passing shower", "Meet your neighbours",
                                "Make yourself at home"};
    constexpr std::array descriptions{
        "Walk with WASD and look with the mouse. E opens a placed garden; Esc brings you back. "
        "F lifts it: walk gently, turn it with the wheel, and E sets it down.",
        "Inside a garden, choose plants from your field kit and click soil. Water thirsty roots "
        "or invite rain with R. V reads soil water and shade; the weather page adjusts its climate.",
        "Click a creature to see what it is doing and eating. Name a favourite, follow its family, "
        "and look for eggs. The journal explains the food web.",
        "Scroll closer, right-drag to orbit, or middle-drag to pan. Space pauses time; H hides your "
        "field kit. "
        "B returns to the greenhouse. Carry a garden gently to an empty placement."};
    text(std::format("A FIELD GUIDE  /  {} OF 4", welcome_step_ + 1), x, y, 13, faded);
    text(titles[welcome_step_], x, y + 34, 36, ink, true);
    illustration({x + cw / 2, y + 208}, 190, welcome_step_, t);
    wrapped(descriptions[welcome_step_], x + 12, y + 307, cw - 24, 18, ink);
    if (button({x + cw - 169, page.y + page.height - 130, 169, 44},
               welcome_step_ == 3 ? "Into the garden" : "A little further", true)) {
        if (welcome_step_ == 3)
            request = ui_request::finish_welcome;
        else {
            ++welcome_step_;
            screen_since_ = get_time();
        }
    }
    if (welcome_step_ > 0 && button({x, page.y + page.height - 130, 110, 44}, "Back")) {
        --welcome_step_;
        screen_since_ = get_time();
    }
    for (int i = 0; i < 4; ++i)
        draw_circle(x + cw / 2 - 27 + i * 18, page.y + page.height - 52, i == welcome_step_ ? 4 : 2,
                    rgb(126, 138, 81));
}
void user_interface::draw_options(sengine::drawing::rect page) {
    const float x = page.x + 30, y = page.y + 26, cw = page.width - 60;

    clip_ = {x, y, cw, page.height - 105};
    clipped_ = true;
    if (check_collision_point_rec(mouse(), clip_))
        scroll_ =
            std::clamp(scroll_ - get_mouse_wheel_move() * 48, 0.f, std::max(0.f, 1140.f - clip_.height));
    auto clip = scaled(clip_);
    begin_scissor_mode(clip.x, clip.y, clip.width, clip.height);
    preferences({x + 5, y - scroll_, cw - 20, 900});
    end_scissor_mode();
    clipped_ = false;
    if (!options_from_greenhouse_ && garden_name.empty())
        text("Scroll to explore", x + cw - 132, page.y + page.height - 47, 13, faded);
    if (options_from_greenhouse_ || !garden_name.empty()) {
        if (button({x + 118, page.y + page.height - 65, 136, 38}, "Title screen"))
            request = ui_request::home;
        if (button({x + 267, page.y + page.height - 65, cw - 267, 38}, "Save & quit"))
            request = ui_request::quit;
    }
}
void user_interface::front_navigation(screen_kind showing, sengine::drawing::rect page) {
    const float x = page.x + 30;
    if (showing != screen_kind::setup && showing != screen_kind::delete_garden &&
        showing != screen_kind::construction &&
        (button({x, page.y + page.height - 65, showing == screen_kind::welcome ? 140.f : 105.f, 38},
                showing == screen_kind::welcome ? "Skip introduction" : "Back") ||
         is_key_pressed(sengine::key_code::escape))) {
        if (showing == screen_kind::welcome)
            request = ui_request::finish_welcome;
        else if (showing == screen_kind::new_garden)
            show(draft_from_);
        else if (showing == screen_kind::gardens)
            show(gardens_from_);
        else if (showing == screen_kind::options && options_from_greenhouse_)
            show(screen_kind::greenhouse);
        else if (showing == screen_kind::options && !garden_name.empty())
            show(screen_kind::habitat);
        else
            show(screen_kind::home);
    }
}
}
