#include "drawing_color.hpp"
#include "greenhouse.hpp"
#include "journal.hpp"
#include "population_plot.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <format>
namespace terrarium {
using namespace sengine::drawing;
namespace {
constexpr sengine::drawing::color ink{57, 62, 43, 255}, faded{106, 108, 82, 255}, cream{239, 232, 206, 255};
}
void user_interface::draw_greenhouse(greenhouse& game, const terrarium::explorer& player,
                                     const terrarium::landscape& land) {
    sync_scale();
    begin_canvas();
    widget_ = 0;
    clipped_ = false;
    set_mouse_cursor(cursor::arrow);
    if (is_mouse_button_pressed())
        keyboard_focus_ = false;
    float w = width(), h = height();
    draw_rectangle_gradient_v(0, 0, w, 125, rgb(24, 35, 25, 145), transparent);
    draw_rectangle_gradient_v(0, h - 115, w, 115, transparent, rgb(24, 35, 25, 165));
    const bool carrying = game.holding();
    const auto* held = carrying ? game.active() : nullptr;
    const std::string title = held ? held->name : "The greenhouse";
    float size =
        std::min(36.f, (w - 200) * 36 / std::max(1.f, measure_text_ex(display_, title.c_str(), 36, 0).x));
    text(title, 28, 24, size, cream, true, true);
    if (held)
        text(std::format("Day {:.1f}  /  {:.1f} C  /  {:.0f}% humidity", held->world.day() + 1,
                         held->world.temperature(), held->world.humidity() * 100),
             30, 68, 14, cream, false, true);
    else
        text(std::format("{} little worlds", game.gardens().size()), 30, 68, 15, cream, false, true);
    if (!carrying && !game.transition()) {
        medallion_face({w - 122, 27, 40, 40}, 9);
        medallion_face({w - 72, 27, 40, 40}, 10);
        text("J", w - 106, 74, 13, cream, false, true);
        text("Esc", w - 63, 74, 13, cream, false, true);
    }
    if (!game.transition()) {
        draw_circle({w * .5f, h * .5f}, 2, fade(cream, .85f));
        draw_circle_lines({w * .5f, h * .5f}, 4, fade(ink, .5f));
    }
    const int target = game.target(player.camera(false), land, carrying);
    std::string label, action;
    if (game.transition()) {
        label = "A little care";
        action = carrying ? "Supporting the glass..." : "Returning to the greenhouse...";
    } else if (carrying) {
        label = target >= 0 ? greenhouse::location(target) : "Find a place to settle";
        action = target >= 0 ? "E  /  Place here     Wheel  /  Rotate"
                             : "Walk gently between placements  /  Wheel to rotate";
    } else if (auto* garden = game.at(target)) {
        label = garden->name;
        action = "E  /  Look inside     F  /  Lift carefully";
    }
    if (!label.empty()) {
        float pw = std::min(w - 48, std::max(380.f, measure_text_ex(body_, action.c_str(), 15, 0).x + 64));
        sengine::drawing::rect page{(w - pw) / 2, carrying ? 110.f : h - 158, pw, 91};
        paper(page);
        float title_size =
            std::min(25.f, (pw - 48) * 25 / std::max(1.f, measure_text_ex(display_, label.c_str(), 25, 0).x));
        text(label, page.x + 24, page.y + 17, title_size, ink, true);
        text(action, page.x + 24, page.y + 56, 15, faded);
    }
    if (!carrying) {
        if (auto* garden = game.find(game.tracked()); garden && garden->place >= 0) {
            auto a = greenhouse::spots()[garden->place].position, b = player.position();
            float dx = a.x - b.x, dz = a.z - b.z,
                  angle = std::remainder(std::atan2(dx, -dz) - player.camera(false).yaw, 2 * pi);
            auto direction = std::abs(angle) < .25f ? "Ahead" : angle > 0 ? "To your right" : "To your left";
            auto target_name = garden->name;
            float ww = std::min(w - 56, 420.f);
            float yy = game.at(target) ? 112.f : h - 179;
            paper({28, yy, ww, 95});
            float fs =
                std::min(23.f, (ww - 42) * 23 /
                                   std::max(1.f, measure_text_ex(display_, target_name.c_str(), 23, 0).x));
            text(target_name, 48, yy + 17, fs, ink, true);
            text(std::format("{} / {:.1f} m", direction, std::hypot(dx, dz)), 48, yy + 50, 15, faded);
            text(greenhouse::location(garden->place), 48, yy + 71, 12, faded);
        }
        text("WASD  walk  /  Shift  sprint  /  Space  jump  /  J  journal  /  Esc  settings", 28, h - 35, 14,
             cream, false, true);
    } else
        text("A steady pace  /  Both hands on the glass", 28, h - 35, 14, cream, false, true);
    if (!game.error().empty())
        text(game.error(), 28, 102, 16, cream, false, true);
    if (get_time() < notify_until_) {
        float text_width = measure_text_ex(body_, notification_.c_str(), 15, 0).x;
        float fs = std::min(15.f, (w - 92) * 15 / std::max(1.f, text_width));
        float pw = std::min(w - 60, text_width + 32);
        float x = (w - pw) / 2, y = h - 205;
        draw_rectangle_rounded({x, y, pw, 32}, .12f, 6, rgb(30, 40, 30, 230));
        text(notification_, x + 16, y + (32 - fs) / 2, fs, cream);
    }
    widget_count_ = widget_;
    end_transform();
}
void user_interface::draw_greenhouse_journal(greenhouse& game) {
    sync_scale();
    begin_canvas();
    widget_ = 0;
    clipped_ = false;
    set_mouse_cursor(cursor::arrow);
    if (is_mouse_button_pressed())
        keyboard_focus_ = false;
    if (is_key_pressed(sengine::key_code::tab)) {
        keyboard_focus_ = true;
        focused_ = (focused_ + 1) % std::max(1, widget_count_);
    }
    float w = width(), h = height(), pw = std::min(w - 48, 1040.f), ph = std::min(h - 56, 780.f);
    draw_rectangle_rec({0, 0, w, h}, rgb(24, 35, 25, 110));
    sengine::drawing::rect page{(w - pw) / 2, (h - ph) / 2, pw, ph};
    paper(page);
    float x = page.x + 30, y = page.y + 28, cw = pw - 60;
    float heading_size =
        std::min(34.f, (cw - 110) * 34 / measure_text_ex(display_, "The greenhouse journal", 34, 0).x);
    text("The greenhouse journal", x, y, heading_size, ink, true);
    text("Every garden, and the small lives within it", x + 1, y + 46, 15, faded);
    if (button({x + cw - 92, y, 92, 35}, "Close")) {
        game.leave();
        end_transform();
        return;
    }
    const bool narrow = pw < 820;
    float list_width = narrow ? cw : cw * .42f;
    float list_height = narrow ? (ph < 720 ? 69.f : 138.f) : ph - 190;
    int count = std::max(1, int(list_height / 69)),
        pages = std::max(1, (int(game.gardens().size()) + count - 1) / count);
    if (is_key_pressed(sengine::key_code::left))
        shelf_page_ = std::max(0, shelf_page_ - 1);
    if (is_key_pressed(sengine::key_code::right))
        shelf_page_ = std::min(pages - 1, shelf_page_ + 1);
    if (check_collision_point_rec(mouse(), {x, y + 91, list_width, list_height}))
        shelf_page_ -= int(get_mouse_wheel_move());
    shelf_page_ = std::clamp(shelf_page_, 0, pages - 1);
    if (!game.find(greenhouse_selection_) && !game.gardens().empty())
        greenhouse_selection_ = game.gardens().front().id;
    for (int row = 0; row < count && shelf_page_ * count + row < int(game.gardens().size()); ++row) {
        const auto& g = game.gardens()[shelf_page_ * count + row];
        float yy = y + 91 + row * 69;
        if (button({x, yy, list_width - (narrow ? 0 : 24), 33}, g.name, g.id == greenhouse_selection_))
            greenhouse_selection_ = g.id;
        text(greenhouse::location(g.place), x + 8, yy + 42, 12, faded);
    }
    float dx = narrow ? x : x + cw * .46f, dy = narrow ? y + 110 + list_height : y + 94,
          dw = narrow ? cw : cw * .54f;
    if (auto* g = game.find(greenhouse_selection_)) {
        garden_summary(*g, dx, dy, dw, narrow);
    } else {
        illustration({x + cw * .5f, y + 250}, 140, 0, get_time());
        wrapped("There is room for a little life here. Open your gardens to plant a new beginning.", x,
                y + 360, cw, 19, ink);
    }
    if (button({x, page.y + ph - 65, 190, 38}, "Your gardens"))
        show(screen_kind::gardens);
    if (pages > 1) {
        if (button({x + cw - 170, page.y + ph - 65, 75, 38}, "Prev"))
            shelf_page_ = std::max(0, shelf_page_ - 1);
        if (button({x + cw - 80, page.y + ph - 65, 75, 38}, "Next"))
            shelf_page_ = std::min(pages - 1, shelf_page_ + 1);
    }
    widget_count_ = widget_;
    end_transform();
}
void user_interface::garden_summary(const greenhouse_garden& garden, float dx, float dy, float dw,
                                    bool narrow) {
    float fs =
        std::min(28.f, dw * 28 / std::max(1.f, measure_text_ex(display_, garden.name.c_str(), 28, 0).x));
    text(garden.name, dx, dy, fs, ink, true);
    text(std::format("Day {:.1f} / {}", garden.world.day() + 1, garden.world.season()), dx, dy + 37, 15,
         faded);
    auto pop = garden.world.populations();
    text(std::format("{} plants / {} little neighbours", garden.world.plants().size(),
                     pop[0] + pop[1] + pop[2]),
         dx, dy + 68, 17, ink);
    text(std::format("{:.1f} C  /  {:.0f}% humidity  /  {:.0f}% soil water", garden.world.temperature(),
                     garden.world.humidity() * 100, garden.world.moisture() * 100),
         dx, dy + 98, 14, faded);
    text("A RECORD OF SMALL CHANGES", dx, dy + (narrow ? 120 : 140), 12, faded);
    journal_timeline timeline(garden.world);
    auto samples = timeline.samples();
    sengine::drawing::rect plot{dx, dy + (narrow ? 151 : 171), dw, narrow ? 48.f : 135.f};
    constexpr std::array<sengine::drawing::color, 3> colors{
        {{117, 140, 76, 255}, {178, 141, 75, 255}, {160, 88, 65, 255}}};
    for (int j = 0; j < 3; ++j) {
        draw_line_ex({plot.x, plot.y + j * plot.height / 2},
                     {plot.x + plot.width, plot.y + j * plot.height / 2}, 1, rgb(196, 183, 148));
        int ceiling = timeline.ceiling(j);
        const population_plot graph(timeline, plot, j, ceiling);
        for (size_t i = 1; i < samples.size(); ++i) {
            draw_line_ex(graph.project(samples[i - 1]), graph.project(samples[i]), 1.8f, colors[j]);
        }
    }
    float after = plot.y + plot.height + 18;
    text(std::format("Aphids {}   Snails {}   Ladybirds {}", pop[0], pop[1], pop[2]), dx, after, 14, faded);
    if (garden.place >= 0 && button({dx, after + 30, dw, 38}, "Find this garden", true)) {
        request = ui_request::open;
        request_id = garden.id;
    }
    if (!narrow && !garden.world.journal().empty()) {
        text("A note from the garden", dx, after + 100, 22, ink, true);
        wrapped(garden.world.journal().front().message, dx, after + 137, dw, 16, faded);
    }
}
}
