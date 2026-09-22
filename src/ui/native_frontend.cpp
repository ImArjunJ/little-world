#include "native_frontend.hpp"
#include "drawing_color.hpp"
#include "habitat_surface.hpp"
#include <chrono>
#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace terrarium {
using namespace sengine::drawing;
native_frontend::native_frontend(greenhouse& game, greenhouse_controls& controls, sengine::explorer& explorer,
                                 std::filesystem::path root)
    : game_(game), controls_(controls), explorer_(explorer), root_(std::move(root)) {
    load_preferences();
    preferences_written_ = preferences();
}
void native_frontend::load_preferences() {
    std::ifstream in(std::filesystem::exists(root_ / "settings") ? root_ / "settings"
                                                                 : root_ / "garden.save.prefs");
    audio_settings audio;
    bool motion = false;
    int quality = 1;
    if (in >> audio.enabled >> audio.master >> audio.forest >> audio.water >> audio.rain >> motion >>
        quality) {
        bool valid = quality >= 1 && quality <= 2;
        for (float v : {audio.master, audio.forest, audio.water, audio.rain})
            valid &= std::isfinite(v) && v >= 0 && v <= 1;
        if (valid) {
            ui.audio = audio;
            ui.reduced_motion = motion;
            ui.performance = quality;
        }
    }
    float size = 1;
    bool automatic = true, seen = false, ao = true, lens = false;
    int preset = 1;
    if (in >> size >> automatic >> seen >> ao >> lens >> preset)
        if (std::isfinite(size) && size >= .8f && size <= 2 && preset >= 0 && preset <= 2) {
            ui.interface_size = size;
            ui.automatic_dpi = automatic;
            ui.onboarding_seen = seen;
            ui.ambient_occlusion = ao;
            ui.lens_blur = lens;
            ui.performance = preset;
        }
    bool setup = false, obsolete_rtx = false, fullscreen = false;
    if (in >> setup >> obsolete_rtx) {
        ui.setup_seen = setup;
        if (in >> fullscreen)
            ui.borderless = fullscreen;
    }
}
std::string native_frontend::preferences() const {
    std::ostringstream out;
    out << std::setprecision(9) << ui.audio.enabled << ' ' << ui.audio.master << ' ' << ui.audio.forest << ' '
        << ui.audio.water << ' ' << ui.audio.rain << ' ' << ui.reduced_motion << ' '
        << (ui.performance == 2 ? 2 : 1) << ' ' << ui.interface_size << ' ' << ui.automatic_dpi << ' '
        << ui.onboarding_seen << ' ' << ui.ambient_occlusion << ' ' << ui.lens_blur << ' ' << ui.performance
        << ' ' << ui.setup_seen << " 0 " << ui.borderless << '\n';
    return out.str();
}
bool native_frontend::save_preferences() {
    auto state = preferences();
    if (state == preferences_written_)
        return true;
    try {
        std::filesystem::create_directories(root_);
        auto temp = root_ / "settings.tmp";
        std::ofstream out(temp);
        out << state;
        out.close();
        if (!out)
            throw std::runtime_error("Cannot write settings.");
        std::filesystem::rename(temp, root_ / "settings");
        preferences_written_ = state;
        return true;
    } catch (const std::exception& e) {
        ui.toast(e.what());
        return false;
    }
}
void native_frontend::start() {
    ui.show(ui.setup_seen ? screen_kind::home : screen_kind::setup);
    initial_setup_ = !ui.setup_seen;
}
void native_frontend::event(const sengine::input_event& e, bool captured) {
    canvas.event(e);
    if (e.type == sengine::event_type::key_down && !e.key.repeat && e.key.code == sengine::key_code::f11 &&
        !game_.holding())
        ui.borderless = !ui.borderless;
    if (ui.screen() == screen_kind::greenhouse) {
        if (game_.mode() == greenhouse_mode::explore && e.type == sengine::event_type::key_down &&
            !e.key.repeat && e.key.code == sengine::key_code::escape) {
            canvas.pressed[sengine::key_code::escape] = false;
            ui.show(screen_kind::options);
            sengine::stop(explorer_);
            return;
        }
        controls_.event(e, captured);
        if (auto notice = controls_.take_notice(); !notice.empty())
            ui.toast(notice);
    } else if (ui.screen() == screen_kind::habitat && !ui.typing()) {
        if (e.type == sengine::event_type::key_down && !e.key.repeat &&
            e.key.code == sengine::key_code::escape) {
            canvas.pressed[sengine::key_code::escape] = false;
            return_to_greenhouse();
            return;
        }

        if (e.type == sengine::event_type::mouse_motion || e.type == sengine::event_type::mouse_wheel) {
            if (ui.over_garden(get_mouse_position())) {
                controls_.event(e, false);
                if (e.type == sengine::event_type::mouse_motion &&
                    (e.motion.buttons & sengine::middle_button))
                    ui.followed = 0;
            }
        }
    }
}
sengine::drawing::point2 native_frontend::screen_point(vec2 point, float elevation) const {
    auto camera = controls_.camera();
    auto p = controls_.world_point(point, elevation);
    auto d = camera.direction;
    float flat = std::hypot(d.x, d.z);
    sengine::point right{-d.z / flat, 0, d.x / flat},
        up{-d.y * right.z, d.x * right.z - d.z * right.x, d.y * right.x};
    sengine::point delta{p.x - camera.eye.x, p.y - camera.eye.y, p.z - camera.eye.z};
    auto dot = [](sengine::point a, sengine::point b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    float depth = dot(delta, d);
    if (depth <= .005f)
        return {-10000, -10000};
    float t = std::tan(camera.fov * pi / 360), w = canvas.width, h = canvas.height;
    return {(dot(delta, right) / (depth * t * w / h) + 1) * w * .5f,
            (1 - dot(delta, up) / (depth * t)) * h * .5f};
}
entity_id native_frontend::pick_creature(const world_state& world, sengine::drawing::point2 mouse) const {
    entity_id result = 0;
    float nearest = 20 * ui.scale();
    for (const auto& c : world.creatures()) {
        auto p = screen_point(c.position, .004f + presentation::bed_offset(world, c.position) -
                                              presentation::surface_offset(world, c.position));
        float d = std::hypot(p.x - mouse.x, p.y - mouse.y);
        if (d < nearest) {
            nearest = d;
            result = c.id;
        }
    }
    return result;
}
entity_id native_frontend::pick_plant(const world_state& world, sengine::drawing::point2 mouse) const {
    entity_id result = 0;
    float nearest = 32 * ui.scale();
    for (const auto& plant : world.plants()) {
        auto p = screen_point(plant.position, float(world.design().height()) * .2f);
        float d = std::hypot(p.x - mouse.x, p.y - mouse.y);
        if (d < nearest) {
            nearest = d;
            result = plant.id;
        }
    }
    return result;
}
void native_frontend::update_editor_input(double seconds) {
    if (ui.screen() == screen_kind::habitat)
        if (auto* world = game_.editing_world())
            tend(*world, seconds);
    controls_.advance(seconds, ui.reduced_motion);
}
std::optional<tending_preview> native_frontend::preview() const {
    const auto* garden = game_.active();
    const auto mouse = sengine::drawing::point2{canvas.input.x, canvas.input.y};
    if (ui.screen() != screen_kind::habitat || game_.mode() != greenhouse_mode::editor || !garden ||
        ui.typing() || ui.observation || ui.tool == garden_tool::inspect || !ui.over_garden(mouse))
        return {};
    auto soil = controls_.soil_point(mouse.x, mouse.y, canvas.width, canvas.height);
    if (!soil)
        return {};
    return placement_preview(garden->world, ui.tool, *soil);
}
void native_frontend::return_to_greenhouse() {
    game_.leave();
    ui.close_panel();
    ui.show(screen_kind::greenhouse);
    ui.garden_name.clear();
}
void native_frontend::tend(world_state& world, double dt) {
    if (ui.typing())
        return;
    auto key = [&](sengine::key_code k) { return canvas.pressed[k]; };
    if (key(sengine::key_code::b)) {
        return_to_greenhouse();
        return;
    }
    if (key(sengine::key_code::space))
        ui.paused = !ui.paused;
    if (key(sengine::key_code::r))
        world.rain();
    if (key(sengine::key_code::h)) {
        ui.observation = !ui.observation;
        ui.tool = garden_tool::inspect;
    }
    if (key(sengine::key_code::m))
        ui.audio.enabled = !ui.audio.enabled;
    if (key(sengine::key_code::home)) {
        controls_.reset_camera();
        ui.followed = 0;
    }
    if (key(sengine::key_code::f) && world.creature(ui.selected)) {
        ui.followed = ui.followed == ui.selected ? 0 : ui.selected;
    }
    if (key(sengine::key_code::p))
        ui.request = ui_request::photo;
    if (key(sengine::key_code::f5))
        ui.request = ui_request::save;
    if (key(sengine::key_code::f9))
        ui.request = ui_request::restore;
    for (int i = 0; i < 8; ++i)
        if (key(sengine::key_code(int(sengine::key_code::digit_1) + i)))
            ui.tool = garden_tool(i);
    if (canvas.down[sengine::key_code::q])
        controls_.rotate_camera(float(-dt));
    if (canvas.down[sengine::key_code::e])
        controls_.rotate_camera(float(dt));
    float right = float(canvas.down[sengine::key_code::d]) - float(canvas.down[sengine::key_code::a]);
    float forward = float(canvas.down[sengine::key_code::w]) - float(canvas.down[sengine::key_code::s]);
    if (right || forward) {
        float step = float(std::min(.1, dt)) * 3 / std::max(1.f, std::hypot(right, forward));
        controls_.pan(right * step, forward * step);
        ui.followed = 0;
    }
    if (ui.followed) {
        if (auto* c = world.creature(ui.followed))
            controls_.focus(c->position);
        else {
            ui.toast(world.name(ui.followed) + "'s story lives on in the journal.");
            ui.followed = 0;
        }
    }
    auto mouse = get_mouse_position();
    if (!ui.over_garden(mouse))
        return;
    auto soil = controls_.soil_point(mouse.x, mouse.y, canvas.width, canvas.height);
    if (canvas.input.pressed) {
        if (ui.tool == garden_tool::inspect) {
            if (auto id = pick_creature(world, mouse)) {
                ui.selected = id;
                ui.reveal_family();
            } else if (auto id = pick_plant(world, mouse))
                ui.reveal_plant(id);
        } else if (soil) {
            if (ui.tool == garden_tool::compost || ui.tool == garden_tool::mulch)
                world.amend(*soil, ui.tool == garden_tool::compost ? amendment::compost : amendment::wood);
            else if (int(ui.tool) < 4) {
                if (!world.add_plant(plant_kind(int(ui.tool) - 1), *soil))
                    ui.toast(placement_preview(world, ui.tool, *soil).hint());
            } else if (int(ui.tool) < 7) {
                auto id = world.add_creature(species_kind(int(ui.tool) - 4), *soil);
                ui.toast(id ? world.name(id) + " has moved in."
                            : placement_preview(world, ui.tool, *soil).hint());
            }
        }
    }
    if (soil && ui.tool == garden_tool::water && canvas.input.down)
        world.water(*soil, std::min(.1, dt) * .7);
}
void native_frontend::requests() {
    auto action = ui.request;
    ui.request = ui_request::none;
    if (action == ui_request::none)
        return;
    if (action == ui_request::save) {
        ui.toast(game_.save() ? "Tucked away safely." : game_.error());
    } else if (action == ui_request::restore) {
        if (auto* world = game_.editing_world()) {
            std::string error;
            if (world->load(game_.active_path(), error)) {
                game_.reset_editor_clock();
                ui.selected = ui.selected_plant = ui.followed = 0;
                ui.toast("Your saved world is back.");
            } else
                ui.toast(error);
        }
    } else if (action == ui_request::greenhouse || action == ui_request::continue_game) {
        return_to_greenhouse();
    } else if (action == ui_request::finish_setup) {
        ui.setup_seen = true;
        ui.show(initial_setup_ ? screen_kind::home : ui.setup_origin());
        initial_setup_ = false;
        save_preferences();
    } else if (action == ui_request::finish_welcome) {
        ui.onboarding_seen = true;
        ui.show(ui.welcome_origin());
        if (game_.mode() == greenhouse_mode::journal)
            game_.leave();
        save_preferences();
    } else if (action == ui_request::home || action == ui_request::quit) {
        if (game_.save() && save_preferences()) {
            if (action == ui_request::quit)
                quit = true;
            else {
                if (game_.mode() == greenhouse_mode::editor || game_.mode() == greenhouse_mode::journal)
                    game_.leave();
                ui.garden_name.clear();
                ui.show(screen_kind::home);
            }
        } else
            ui.toast(game_.error());
    } else if (action == ui_request::photo) {
        ui.show(screen_kind::photo);
    } else if (action == ui_request::finish_photo) {
        ui.show(screen_kind::habitat);
        ui.close_panel();
    } else if (action == ui_request::export_photo) {
        try {
            std::filesystem::create_directories(root_ / "photographs");
            photograph =
                root_ / "photographs" /
                ("garden-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                 ".png");
        } catch (const std::filesystem::filesystem_error&) {
            ui.toast("Couldn't open the photographs folder. Check that your save location is writable.");
        }
    } else {
        if (game_.mode() == greenhouse_mode::explore)
            game_.open_journal();
        bool ok = false;
        if (action == ui_request::create) {
            ok = game_.create(ui.draft_name, ui.draft_design, ui.draft_starter);
            if (ok) {
                ui.show(ui.onboarding_seen ? screen_kind::greenhouse : screen_kind::welcome);
                game_.track(game_.gardens().back().id);
            }
        } else if (action == ui_request::rename) {
            ok = game_.rename(ui.request_id, ui.draft_name);
            if (ok)
                ui.show(screen_kind::gardens);
        } else if (action == ui_request::delete_garden) {
            ok = game_.remove(ui.request_id);
            if (ok)
                ui.show(screen_kind::gardens);
        } else if (action == ui_request::duplicate) {
            ok = game_.duplicate(ui.request_id);
        } else if (action == ui_request::open) {
            ok = game_.track(ui.request_id);
            if (ok) {
                ui.show(screen_kind::greenhouse);
                ui.toast("Follow the marker to your garden.");
            }
        }
        if (!ok)
            ui.toast(game_.error().empty() ? "This garden is in storage. Free a placement to bring it out."
                                           : game_.error());
    }
}
void native_frontend::draw(sengine::native_hud& hud, int width, int height, float dpi, double dt,
                           sengine::hud_input input, bool focused) {
    canvas.draw(hud, width, height, dpi, float(dt), input, focused);
    if (ui.screen() == screen_kind::greenhouse && game_.mode() == greenhouse_mode::editor)
        ui.show(screen_kind::habitat);
    if (ui.screen() == screen_kind::habitat && game_.mode() != greenhouse_mode::editor)
        ui.show(screen_kind::greenhouse);
    if (ui.screen() == screen_kind::gardens && game_.mode() == greenhouse_mode::explore)
        game_.open_journal();
    if (ui.screen() == screen_kind::habitat) {
        auto* w = game_.editing_world();
        if (w) {
            if (editor_id_ != game_.selected()) {
                editor_id_ = game_.selected();
                ui.selected = ui.followed = ui.selected_plant = 0;
                ui.tool = garden_tool::inspect;
                ui.close_panel();
            }
            ui.garden_name = game_.active()->name;
            ui.sync_scale();
            update_editor_input(dt);
            if (ui.screen() == screen_kind::habitat) {
                draw_environment(*w, hud);
                if (focused)
                    draw_preview(hud);
                ui.draw(*w, controls_, game_.active()->clock.pending);
                game_.set_speed(ui.paused ? 0 : ui.speed);
            }
        }
    } else if (ui.screen() == screen_kind::photo)
        ui.draw_photo();
    else if (ui.screen() == screen_kind::greenhouse) {
        if (game_.mode() == greenhouse_mode::journal)
            ui.draw_greenhouse_journal(game_);
        else
            ui.draw_greenhouse(game_, explorer_, controls_.landscape());
    } else if (ui.screen() != screen_kind::greenhouse) {
        std::vector<garden_entry> entries;
        for (const auto& g : game_.gardens())
            entries.push_back({g.id, g.name, g.world.day(), g.world.populations()});
        ui.draw_front(entries);
    }
    requests();
    preferences_delay_ += dt;
    if (preferences_delay_ >= 1) {
        save_preferences();
        preferences_delay_ = 0;
    }
}
void native_frontend::draw_preview(sengine::native_hud& hud) {
    const auto guide = preview();
    if (!guide)
        return;
    const float scale = ui.scale();
    const auto color =
        guide->allowed() ? sengine::ink{.72f, .82f, .48f, .95f} : sengine::ink{.95f, .43f, .33f, .95f};
    const float radius = guide->tool == garden_tool::water ? std::sqrt(1.2f) : .28f;
    for (int i = 0; i < 64; ++i) {
        const float a = i * 2 * pi / 64, b = (i + 1) * 2 * pi / 64;
        const auto p = screen_point(
            {guide->position.x + radius * std::cos(a), guide->position.y + radius * std::sin(a)}, .001f);
        const auto q = screen_point(
            {guide->position.x + radius * std::cos(b), guide->position.y + radius * std::sin(b)}, .001f);
        if (p.x > -9999 && q.x > -9999)
            hud.line(p.x, p.y, q.x, q.y, 1.5f * scale, color);
    }
    const std::string label = guide->hint();
    const float font = 16 * scale, width = hud.measure(label, font) + 20 * scale;
    const float x = std::clamp(canvas.input.x + 18 * scale, 0.f, canvas.width - width);
    const float y = std::clamp(canvas.input.y + 24 * scale, 0.f, canvas.height - 34 * scale);
    draw_rectangle_rounded({x, y, width, 30 * scale}, .25f, 8, rgb(43, 49, 37));
    hud.text(x + 10 * scale, y + 5 * scale, label, font, {.96f, .93f, .84f, 1});
}
void native_frontend::draw_environment(const world_state& world, sengine::native_hud& hud) {
    if (!ui.environment_view || ui.observation)
        return;
    struct point {
        sengine::drawing::point2 screen;
        sengine::ink color;
        bool visible{};
    };
    constexpr int side = world_state::soil_width + 1;
    std::array<point, side * side> points;
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            vec2 p{-5.f + x * 10.f / world_state::soil_width, -5.f + y * 10.f / world_state::soil_width};
            auto& point = points[y * side + x];
            point.screen = screen_point(p, .001f);
            point.visible = std::hypot(p.x, p.y) < world_state::radius && !world_state::in_pond(p) &&
                            point.screen.x > -9999;
            float value =
                std::clamp(ui.environment_view == 1 ? world.soil_at(p).water : world.light_at(p), 0., 1.);
            sengine::drawing::color low = ui.environment_view == 1 ? rgb(181, 114, 68) : rgb(77, 74, 112);
            sengine::drawing::color high = ui.environment_view == 1 ? rgb(63, 164, 172) : rgb(241, 201, 100);
            auto c = color_lerp(low, high, value);
            point.color = {c.r / 255.f, c.g / 255.f, c.b / 255.f, .48f};
        }
    auto triangle = [&](const point& a, const point& b, const point& c) {
        if (a.visible && b.visible && c.visible)
            hud.triangle({a.screen.x, a.screen.y}, {b.screen.x, b.screen.y}, {c.screen.x, c.screen.y},
                         a.color, b.color, c.color);
    };
    for (int y = 0; y < side - 1; ++y)
        for (int x = 0; x < side - 1; ++x) {
            int i = y * side + x;
            triangle(points[i], points[i + side], points[i + side + 1]);
            triangle(points[i], points[i + side + 1], points[i + 1]);
        }
}

}
