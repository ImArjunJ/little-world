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
void user_interface::illustration(sengine::drawing::point2 c, float size, int stage, float time) {
    float sway = reduced_motion ? 0 : std::sin(time * 1.4f) * .04f;
    sengine::drawing::color green{101, 127, 72, 255}, pale{166, 180, 119, 255};
    draw_ellipse(c.x, c.y + size * .28f, size * .47f, size * .09f, rgb(72, 85, 46, 30));
    draw_line_ex({c.x - size * .45f, c.y + size * .22f}, {c.x + size * .45f, c.y + size * .22f}, 2,
                 rgb(160, 147, 104));
    for (int i = 0; i < 3; ++i) {
        float x = c.x + (i - 1) * size * .24f,
              growth = reduced_motion ? 1 : std::clamp(time * .6f - i * .18f, 0.f, 1.f);
        float h = size * (i == 1 ? .48f : .31f) * growth;
        draw_line_ex({x, c.y + size * .2f}, {x + sway * size, c.y + size * .2f - h}, 3, green);
        for (int j = 0; j < 3; ++j) {
            float y = c.y + size * .15f - h * j / 3;
            float span = size * .09f * growth;
            draw_ellipse(x - span * .62f, y, span, span * .40f, j % 2 ? green : pale);
            draw_ellipse(x + span * .62f, y - span * .48f, span, span * .40f, j % 2 ? pale : green);
        }
        if (stage == 0)
            draw_circle(x, c.y + size * .24f, 3, rgb(112, 88, 53));
    }
    if (stage == 1) {
        for (int i = 0; i < 12; ++i) {
            float t = reduced_motion ? .5f : std::fmod(time * .5f + i * .137f, 1.f);
            float x = c.x + std::sin(i * 7.f) * size * .43f, y = c.y - size * .65f + t * size * .9f;
            draw_line_ex({x, y}, {x - 3, y + 8}, 2, rgb(102, 147, 151, 155));
        }
    }
    if (stage >= 2) {
        float x = c.x + size * .20f + (reduced_motion ? 0 : std::sin(time * .7f) * size * .08f),
              y = c.y + size * .21f;
        draw_ellipse(x, y, size * .15f, size * .035f, rgb(134, 117, 75));
        draw_circle(x - size * .025f, y - size * .075f, size * .09f, rgb(190, 151, 88));
        for (int j = 0; j < 48; ++j) {
            float a = j * .2f, b = (j + 1) * .2f, r = .006f * std::exp(a * .20f),
                  s = .006f * std::exp(b * .20f);
            draw_line_ex(
                {x - size * .025f + std::cos(a) * size * r, y - size * .075f + std::sin(a) * size * r},
                {x - size * .025f + std::cos(b) * size * s, y - size * .075f + std::sin(b) * size * s}, 1.3f,
                rgb(123, 96, 56));
        }
        draw_line_ex({x + size * .11f, y}, {x + size * .15f, y - size * .09f}, 2, rgb(103, 105, 63));
        draw_circle(x + size * .15f, y - size * .09f, 2, ink);
    }
    if (stage == 3) {
        draw_ellipse_lines(c.x, c.y - size * .60f, size * .53f, size * .10f, rgb(151, 173, 150));
        draw_line_ex({c.x - size * .53f, c.y - size * .60f}, {c.x - size * .53f, c.y + size * .24f}, 2,
                     rgb(151, 173, 150));
        draw_line_ex({c.x + size * .53f, c.y - size * .60f}, {c.x + size * .53f, c.y + size * .24f}, 2,
                     rgb(151, 173, 150));
        draw_ellipse_lines(c.x, c.y + size * .24f, size * .53f, size * .10f, rgb(151, 173, 150));
    }
}
void user_interface::construction(sengine::drawing::rect page, float time) {
    const float x = page.x + 30, y = page.y + 26, cw = page.width - 60;
    constexpr std::array titles{"A home under glass", "Build the forest floor", "The first little residents"};
    constexpr std::array hints{"1 / 3   Choose your vessel", "2 / 3   Layer the ground",
                               "3 / 3   Choose a beginning"};
    text(titles[construction_step_], x, y, 34, ink, true);
    text(hints[construction_step_], x, y + 48, 16, faded);

    const float unit = 330, cx = x + 108, bottom = y + 239;
    const float radius = draft_design.radius() * unit, top = bottom - draft_design.height() * unit;
    const float drain_top = bottom - draft_design.drainage_depth * unit;
    const float soil_top = drain_top - draft_design.soil_depth * unit;
    sengine::drawing::color soil = draft_design.mix == soil_mix::sand   ? rgb(173, 146, 95)
                                   : draft_design.mix == soil_mix::loam ? rgb(114, 91, 64)
                                                                        : rgb(92, 80, 57);
    draw_ellipse(cx, bottom + 7, radius + 9, 9, rgb(77, 75, 43, 24));
    draw_rectangle_rec({cx - radius, top, radius * 2, bottom - top}, rgb(189, 208, 181, 40));
    draw_rectangle_rec({cx - radius, drain_top, radius * 2, bottom - drain_top}, rgb(157, 149, 127));
    draw_ellipse(cx, bottom, radius, 7, rgb(151, 143, 119));
    for (int i = 0; i < 28; ++i) {
        const float px = cx - radius + 6 + std::fmod(i * 23.7f, radius * 2 - 12);
        const float py = drain_top + 2 + std::fmod(i * 5.31f, std::max(1.f, bottom - drain_top - 4));
        draw_ellipse(px, py, 3.5f, 2.1f, i % 2 ? rgb(187, 175, 146) : rgb(117, 117, 103));
    }
    draw_rectangle_rec({cx - radius, soil_top, radius * 2, drain_top - soil_top}, soil);
    draw_ellipse(cx, soil_top, radius, 7, rgb(118, 113, 79));
    if (construction_step_ == 2 && draft_starter != starter::empty) {
        for (int i = 0; i < 5; ++i) {
            const float px = cx + (i - 2) * radius * .29f;
            const float sway = reduced_motion ? 0 : std::sin(time * 1.6f + i) * 2;
            const float stem = std::min(soil_top - top - 12, 19.f + (i % 3) * 9);
            draw_line_ex({px, soil_top}, {px + sway, soil_top - stem}, 2, rgb(98, 124, 67));
            for (int j = 0; j < 3; ++j) {
                draw_ellipse(px - 4 + sway, soil_top - stem * (j + 1) / 4, 6, 2.5f, rgb(127, 153, 88));
                draw_ellipse(px + 4 + sway, soil_top - stem * (j + 1) / 4 - 3, 6, 2.5f, rgb(106, 135, 71));
            }
            if (draft_starter == starter::meadow)
                draw_circle(px + sway, soil_top - stem, 4, rgb(231, 194, 127));
        }
    }
    draw_ellipse_lines(cx, top, radius, 7, rgb(133, 159, 135));
    draw_line_ex({cx - radius, top}, {cx - radius, bottom}, 2, rgb(133, 159, 135));
    draw_line_ex({cx + radius, top}, {cx + radius, bottom}, 2, rgb(133, 159, 135));
    draw_ellipse_lines(cx, bottom, radius, 7, rgb(133, 159, 135));
    draw_line_ex({cx - radius + 7, top + 10}, {cx - radius + 7, soil_top - 9}, 2, rgb(251, 250, 229, 190));
    const float tx = x + 228;
    text(vessel_name(draft_design.form), tx, y + 108, 24, ink, true);
    text(std::format("{:.0f} cm wide / {:.0f} cm tall", draft_design.radius() * 200,
                     draft_design.height() * 100),
         tx, y + 143, 15, faded);
    text(std::format("{:.1f} L of growing room", draft_design.parameters().air_volume * 1000), tx, y + 171,
         15, faded);
    text(std::format("{:.1f} L drainage reserve", draft_design.reservoir_capacity()), tx, y + 197, 15, faded);
    const float gap = 10, bw = (cw - gap * 2) / 3;
    if (construction_step_ == 0) {
        for (int i = 0; i < 3; ++i) {
            auto form = static_cast<vessel_form>(i + 1);
            if (button({x + i * (bw + gap), y + 265, bw, 40}, vessel_name(form), draft_design.form == form))
                draft_design.form = form;
        }
        wrapped(draft_design.form == vessel_form::pocket
                    ? "A small world to keep close. Less soil and water mean changes arrive sooner."
                : draft_design.form == vessel_form::bowl
                    ? "A generous floor for a spreading garden. More soil holds a larger store of water and "
                      "warmth."
                    : "A little canopy in a tall glass home. More air above the roots, with a smaller "
                      "footprint on your shelf.",
                x, y + 326, cw, 18, faded);
    } else if (construction_step_ == 1) {
        for (int i = 0; i < 3; ++i) {
            auto mix = static_cast<soil_mix>(i);
            if (button({x + i * (bw + gap), y + 265, bw, 40}, soil_name(mix), draft_design.mix == mix))
                draft_design.mix = mix;
        }
        wrapped(draft_design.mix == soil_mix::forest
                    ? "An airy blend that holds plenty of water. A forgiving start for ferns and clover."
                : draft_design.mix == soil_mix::loam
                    ? "A dense soil that drains slowly. Holds warmth, but "
                      "needs a lighter hand with the watering can."
                    : "A mineral soil that drains quickly. Roots dry sooner; "
                      "a deeper layer gives them a larger water store.",
                x, y + 318, cw, 16, faded);
        float soil_cm = draft_design.soil_depth * 100, drain_cm = draft_design.drainage_depth * 100;
        slider({x, y + 379, (cw - 28) / 2, 48}, "Soil depth", soil_cm, 4, 10, " cm");
        slider({x + (cw + 28) / 2, y + 379, (cw - 28) / 2, 48}, "Drainage", drain_cm, 1, 5, " cm");
        draft_design.soil_depth = std::clamp(double(soil_cm) / 100, .04, .10);
        draft_design.drainage_depth = std::clamp(double(drain_cm) / 100, .01, .05);
    } else {
        constexpr std::array labels{"Plant it myself", "Woodland", "Meadow"};
        for (int i = 0; i < 3; ++i)
            if (button({x + i * (bw + gap), y + 265, bw, 40}, labels[i], int(draft_starter) == i))
                draft_starter = static_cast<starter>(i);
        wrapped(
            draft_starter == starter::empty
                ? "Just the ground, ready for your hands. Add plants and little creatures when you are ready."
            : draft_starter == starter::woodland
                ? "Ferns and clover, two snails and a few aphids. Watch the leaves grow, then introduce "
                  "predators when the garden can feed them."
                : "Flowers and clover, two snails and a few aphids. Let the meadow settle before inviting "
                  "ladybirds into the food web.",
            x, y + 326, cw, 18, faded);
    }
    const float footer = page.y + page.height - 65;
    if (button({x, footer, 110, 38}, "Back") || is_key_pressed(SDL_SCANCODE_ESCAPE)) {
        if (construction_step_ == 0)
            show(screen_kind::new_garden);
        else {
            --construction_step_;
            show(screen_kind::construction);
        }
    } else if (button({x + cw - 190, footer, 190, 38}, construction_step_ == 2 ? "Bring it to life" : "Next",
                      true)) {
        if (construction_step_ == 2)
            request = ui_request::create;
        else {
            ++construction_step_;
            show(screen_kind::construction);
        }
    }
}
void user_interface::name_input(sengine::drawing::rect r) {
    text(renaming_ ? "A new name" : "Name your little world", r.x, r.y - 27, 16, faded);
    draw_rectangle_rec(r, rgb(255, 250, 230, 100));
    draw_line_ex({r.x, r.y + r.height}, {r.x + r.width, r.y + r.height}, 2, rgb(126, 137, 88));
    for (int code = get_char_pressed(); code > 0; code = get_char_pressed()) {
        if (code >= 32 && code != 127 && draft_name.size() < 60) {
            int size = 0;
            const char* utf = codepoint_to_utf8(code, &size);
            draft_name.append(utf, size);
        }
    }
    if (is_key_pressed(SDL_SCANCODE_BACKSPACE) && !draft_name.empty()) {
        auto i = draft_name.size() - 1;
        while (i > 0 && (static_cast<unsigned char>(draft_name[i]) & 0xc0) == 0x80)
            --i;
        draft_name.erase(i);
    }
    if ((is_key_down(SDL_SCANCODE_LCTRL) || is_key_down(SDL_SCANCODE_LGUI)) && is_key_pressed(SDL_SCANCODE_A))
        draft_name.clear();
    std::string visible = draft_name;
    while (measure_text_ex(display_, visible.c_str(), 30, 0).x > r.width - 28 && !visible.empty()) {
        auto i = std::size_t{1};
        while (i < visible.size() && (static_cast<unsigned char>(visible[i]) & 0xc0) == 0x80)
            ++i;
        visible.erase(0, i);
    }
    text(visible, r.x + 12, r.y + 9, 30, ink, true);
    float cursor = measure_text_ex(display_, visible.c_str(), 30, 0).x;
    if (reduced_motion || std::fmod(get_time(), 1) < .6)
        draw_line_ex({r.x + 14 + cursor, r.y + 10}, {r.x + 14 + cursor, r.y + 39}, 1, ink);
}
void user_interface::preferences(sengine::drawing::rect r) {
    float x = r.x, y = r.y, w = r.width;
    text("Make yourself comfortable", x, y, 34, ink, true);
    text("Display & detail", x, y + 58, 23, ink, true);
    if (button({x, y + 100, w, 36},
               automatic_dpi ? "Display scaling: follow the monitor" : "Display scaling: manual"))
        automatic_dpi = !automatic_dpi;
    text("Interface size", x, y + 163, 16, ink);
    if (button({x + w - 180, y + 148, 44, 44}, "-"))
        interface_size = std::max(.8f, interface_size - .1f);
    text(std::format("{:.0f}%", interface_size * 100), x + w - 117, y + 163, 16, ink);
    if (button({x + w - 44, y + 148, 44, 44}, "+"))
        interface_size = std::min(2.f, interface_size + .1f);
    if (button({x, y + 200, w, 32},
               borderless ? "Window: borderless fullscreen [F11]" : "Window: resizable [F11]"))
        borderless = !borderless;
    constexpr std::array labels{"Light", "Balanced", "Fine"};
    for (int i = 0; i < 3; ++i)
        if (button({x + i * (w + 12) / 3, y + 238, (w - 24) / 3, 40}, labels[i], performance == i)) {
            performance = i;
            ambient_occlusion = i != 0;
        }
    wrapped("Light suits smaller machines. Balanced keeps the greenhouse comfortable. Fine adds smoother "
            "edges and softer shadows.",
            x, y + 291, w, 15, faded);
    if (button({x, y + 344, w, 42}, ambient_occlusion ? "Contact shadows: on" : "Contact shadows: off",
               ambient_occlusion))
        ambient_occlusion = !ambient_occlusion;
    if (button({x, y + 402, w, 36},
               lens_blur ? "Close-up lens: soft focus" : "Close-up lens: everything clear", lens_blur))
        lens_blur = !lens_blur;
    if (button({x, y + 448, w, 36}, reduced_motion ? "Motion: gentle" : "Motion: animated", reduced_motion))
        reduced_motion = !reduced_motion;
    text("Sounds of the garden", x, y + 518, 25, ink, true);
    if (button({x, y + 560, w, 36}, audio.enabled ? "Sound: on" : "Sound: off", audio.enabled))
        audio.enabled = !audio.enabled;
    slider({x, y + 618, w, 40}, "Overall volume", audio.master, 0, 1, "%");
    slider({x, y + 681, w, 40}, "Leaves & distant birds", audio.forest, 0, 1, "%");
    slider({x, y + 744, w, 40}, "Water", audio.water, 0, 1, "%");
    slider({x, y + 807, w, 40}, "Rain", audio.rain, 0, 1, "%");
    if (button({x, y + 884, w, 42}, "Revisit the first-launch setup"))
        show(screen_kind::setup);
    if (button({x, y + 936, w, 42}, "Revisit the field guide"))
        show(screen_kind::welcome);
    draw_line_ex({x, y + 1016}, {x + w, y + 1016}, 1, rgb(181, 169, 132));
    illustration({x + 39, y + 1082}, 61, 0, reduced_motion ? 5 : get_time());
    text("For Shay", x + 92, y + 1043, 28, ink, true);
}
void user_interface::setup(sengine::drawing::rect page, float time) {
    float x = page.x + 30, y = page.y + 26, w = page.width - 60;
    text(std::format("WELCOME HOME  /  {} OF 3", setup_step_ + 1), x, y, 14, faded);
    constexpr std::array titles{"Make yourself comfortable", "Settle into the quiet", "A view to grow into"};
    float title_size =
        std::min(32.f, 32 * w / std::max(1.f, measure_text_ex(display_, titles[setup_step_], 32, 0).x));
    text(titles[setup_step_], x, y + 33, title_size, ink, true);
    illustration({x + w - 48, y + 129}, 70, setup_step_, time);
    if (setup_step_ == 0) {
        wrapped("First, let's find a comfortable text size. These changes appear straight away.", x, y + 93,
                w - 112, 17, ink);
        if (button({x, y + 184, w, 42},
                   automatic_dpi ? "Follow this monitor's display scale" : "Choose my display scale",
                   automatic_dpi))
            automatic_dpi = !automatic_dpi;
        text("Interface size", x, y + 254, 18, ink);
        if (button({x + w - 190, y + 240, 48, 44}, "-"))
            interface_size = std::max(.8f, interface_size - .1f);
        text(std::format("{:.0f}%", interface_size * 100), x + w - 120, y + 254, 18, ink);
        if (button({x + w - 48, y + 240, 48, 44}, "+"))
            interface_size = std::min(2.f, interface_size + .1f);
        wrapped("A fern uncurls. A snail takes the long way home. Nothing here needs to hurry.", x + 12,
                y + 313, w - 24, 19, ink);
        if (button({x, y + 376, w, 38},
                   borderless ? "Display: borderless fullscreen" : "Display: resizable window"))
            borderless = !borderless;
    } else if (setup_step_ == 1) {
        wrapped("Birdsong, soft rain, and room to breathe. Make it as quiet as you like.", x, y + 93, w - 112,
                17, ink);
        if (button({x, y + 184, w, 42}, audio.enabled ? "Garden sounds: on" : "Garden sounds: off",
                   audio.enabled))
            audio.enabled = !audio.enabled;
        slider({x + 8, y + 253, w - 16, 40}, "Overall volume", audio.master, 0, 1, "%");
        if (button({x, y + 330, w, 42},
                   reduced_motion ? "Gentle motion / fewer animations" : "Animated leaves & playful pages",
                   !reduced_motion))
            reduced_motion = !reduced_motion;
        text("All of this can be changed later in settings.", x + 12, y + 387, 14, faded);
    } else {
        wrapped(
            "Every garden can feel lovely. Choose the amount of detail that feels smooth on your machine.", x,
            y + 93, w - 112, 17, ink);
        constexpr std::array labels{"Light", "Balanced", "Fine"};
        for (int i = 0; i < 3; ++i)
            if (button({x + i * (w + 12) / 3, y + 193, (w - 24) / 3, 44}, labels[i], performance == i)) {
                performance = i;
                ambient_occlusion = i != 0;
            }
        wrapped(performance == 0   ? "A lighter touch for older laptops and smaller machines."
                : performance == 1 ? "A comfortable balance of detail and smooth movement."
                                   : "Finer edges and more detail for a capable graphics card.",
                x + 8, y + 262, w - 16, 17, ink);
        wrapped("You can change the detail level later in settings.", x + 8, y + 337, w - 16, 17, faded);
    }
    if (button({x + w - 176, page.y + page.height - 90, 176, 44},
               setup_step_ == 2 ? "Make yourself at home" : "Continue", true)) {
        if (setup_step_ == 2)
            request = ui_request::finish_setup;
        else {
            ++setup_step_;
            screen_since_ = get_time();
            hover_.fill(0);
        }
    }
    if (setup_step_ > 0 && button({x, page.y + page.height - 90, 105, 44}, "Back")) {
        --setup_step_;
        screen_since_ = get_time();
        hover_.fill(0);
    }
    for (int i = 0; i < 3; ++i)
        draw_circle(x + w / 2 - 18 + i * 18, page.y + page.height - 25, i == setup_step_ ? 4 : 2,
                    rgb(126, 138, 81));
}
void user_interface::draw_front(const std::vector<garden_entry>& entries) {
    sync_scale();
    begin_canvas();
    widget_ = 0;
    clipped_ = false;
    set_mouse_cursor(cursor::arrow);
    if (is_mouse_button_pressed())
        keyboard_focus_ = false;
    if (is_mouse_button_released())
        active_slider_ = -1;
    if (is_key_pressed(SDL_SCANCODE_TAB)) {
        keyboard_focus_ = true;
        focused_ = (focused_ + (is_key_down(SDL_SCANCODE_LSHIFT) ? std::max(1, widget_count_) - 1 : 1) +
                    std::max(1, widget_count_)) %
                   std::max(1, widget_count_);
    }
    float w = width(), h = height(),
          t = reduced_motion ? 5.f : static_cast<float>(get_time() - screen_since_);
    draw_rectangle_gradient_h(0, -20, w, h + 40, rgb(24, 37, 27, 208), rgb(34, 46, 28, 45));
    const screen_kind showing = screen_;
    if (showing == screen_kind::home) {
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
    } else {
        float pw = std::min(showing == screen_kind::gardens ? 850.f : 640.f, w - 48);
        float ph = showing == screen_kind::setup ? std::min(620.f, h - 80) : h - 80;
        sengine::drawing::rect page{(w - pw) / 2, showing == screen_kind::setup ? (h - ph) / 2 : 40.f, pw,
                                    ph};
        paper(page);
        float x = page.x + 30, y = page.y + 26, cw = pw - 60;
        if (showing == screen_kind::setup) {
            setup(page, t);
        } else if (showing == screen_kind::gardens) {
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
                text(std::format("{} / {}", shelf_page_ + 1, pages), x + cw - 125, page.y + page.height - 51,
                     14, faded);
                if (button({x + cw - 66, page.y + page.height - 62, 65, 34}, "Next"))
                    shelf_page_ = std::min(pages - 1, shelf_page_ + 1);
            }
        } else if (showing == screen_kind::delete_garden) {
            text("Delete this garden?", x, y, 38, ink, true);
            illustration({x + cw / 2, y + 151}, 145, 3, 5);
            float name_size =
                std::min(27.f, (cw - 24) * 27 /
                                   std::max(1.f, measure_text_ex(display_, delete_name_.c_str(), 27, 0).x));
            text(delete_name_, x + 12, y + 258, name_size, ink, true);
            wrapped("This permanently removes this garden, its creatures and family history, and its backup "
                    "saves.",
                    x + 12, y + 308, cw - 24, 18, faded);
            float button_width = (cw - 36) / 2;
            if (button({x + 12, page.y + page.height - 96, button_width, 44}, "Keep garden", true) ||
                is_key_pressed(SDL_SCANCODE_ESCAPE))
                show(screen_kind::gardens);
            else if (button({x + 24 + button_width, page.y + page.height - 96, button_width, 44},
                            "Delete permanently"))
                request = ui_request::delete_garden;
        } else if (showing == screen_kind::construction) {
            construction(page, t);
        } else if (showing == screen_kind::new_garden) {
            text(renaming_ ? "A name to grow into" : "A new little beginning", x, y, 38, ink, true);
            illustration({x + cw / 2, y + 168}, 160, renaming_ ? 3 : 0, t);
            name_input({x + 12, y + 284, cw - 24, 54});
            text("Type a name / Ctrl+A to start over", x + 12, y + 353, 14, faded);
            if (button({x + 12, y + 401, cw - 24, 46}, renaming_ ? "Keep this name" : "Choose a vessel",
                       true) ||
                is_key_pressed(SDL_SCANCODE_RETURN)) {
                if (renaming_)
                    request = ui_request::rename;
                else
                    show(screen_kind::construction);
            }
        } else if (showing == screen_kind::welcome) {
            constexpr std::array titles{"A home for little worlds", "A passing shower",
                                        "Meet your neighbours", "Make yourself at home"};
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
        } else if (showing == screen_kind::options) {
            clip_ = {x, y, cw, page.height - 105};
            clipped_ = true;
            if (check_collision_point_rec(mouse(), clip_))
                scroll_ = std::clamp(scroll_ - get_mouse_wheel_move() * 48, 0.f,
                                     std::max(0.f, 1140.f - clip_.height));
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
        if (showing != screen_kind::setup && showing != screen_kind::delete_garden &&
            showing != screen_kind::construction &&
            (button({x, page.y + page.height - 65, showing == screen_kind::welcome ? 140.f : 105.f, 38},
                    showing == screen_kind::welcome ? "Skip introduction" : "Back") ||
             is_key_pressed(SDL_SCANCODE_ESCAPE))) {
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
    if (get_time() < notify_until_) {
        auto length = std::min(w - 48, measure_text_ex(body_, notification_.c_str(), 16, 0).x + 32);
        draw_rectangle_rec({(w - length) / 2, h - 37, length, 31}, rgb(30, 43, 29, 245));
        text(notification_, (w - length) / 2 + 16, h - 30, 16, cream);
    }
    widget_count_ = widget_;
    end_transform();
}
void user_interface::draw_photo() {
    sync_scale();
    begin_canvas();
    widget_ = 0;
    set_mouse_cursor(cursor::arrow);
    if (is_mouse_button_pressed())
        keyboard_focus_ = false;
    if (is_key_pressed(SDL_SCANCODE_TAB)) {
        keyboard_focus_ = true;
        focused_ = (focused_ + 1) % 2;
    }
    float w = width(), h = height();
    paper({20, h - 128, w - 40, 108});
    text("A moment in the garden", 40, h - 114, 24, ink, true);
    text("Time is resting. Keep this view without the field kit.", 40, h - 86, 14, faded);
    if (button({w - 265, h - 67, 125, 42}, "Keep photograph", true))
        request = ui_request::export_photo;
    if (button({w - 127, h - 67, 88, 42}, "Return") || is_key_pressed(SDL_SCANCODE_ESCAPE))
        request = ui_request::finish_photo;
    if (get_time() < notify_until_)
        text(notification_, 30, 25, 16, cream, false, true);
    widget_count_ = widget_;
    end_transform();
}
}
