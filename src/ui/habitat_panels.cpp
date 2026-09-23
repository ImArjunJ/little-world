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
constexpr sengine::drawing::color ink{57, 62, 43, 255}, faded{106, 108, 82, 255}, brass{197, 170, 111, 255};
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
    creature_name_field(world, ancestor, x, y, w);
    if (medallion({r.x + r.width - 56, y + 216, 32, 32}, 15,
                  ancestor->favourite ? "Remembered as a favourite" : "Remember this little one",
                  ancestor->favourite))
        world.toggle_favourite(selected);
    if (!editing_)
        text("Click their name to give them a new one", x, y + 280, 11, faded);
    creature_status(world, camera, ancestor, live, x, y, w);
    creature_family(world, ancestor, x, y + 150, w);
}
void user_interface::creature_name_field(world_state& world, const ancestor_record* ancestor, float x,
                                         float y, float w) {
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
        if (is_key_pressed(sengine::key_code::backspace) && !name_buffer_.empty()) {
            auto i = name_buffer_.size() - 1;
            while (i > 0 && (static_cast<unsigned char>(name_buffer_[i]) & 0xc0) == 0x80)
                --i;
            name_buffer_.erase(i);
        }
        if (is_key_pressed(sengine::key_code::enter)) {
            world.rename(selected, name_buffer_);
            editing_ = false;
            toast("A name to remember.");
        }
        if (is_key_pressed(sengine::key_code::escape))
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
}
void user_interface::creature_status(world_state& world, greenhouse_controls& camera,
                                     const ancestor_record* ancestor, const creature_state* live, float x,
                                     float y, float w) {
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
}
void user_interface::creature_family(world_state& world, const ancestor_record* ancestor, float x, float y,
                                     float w) {
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
            auto it = std::ranges::find(all, selected, &creature_state::id);
            selected = it == all.end() || ++it == all.end() ? all.front().id : it->id;
            scroll_ = 0;
        }
    }
}
}
