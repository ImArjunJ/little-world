#include "drawing_color.hpp"
#include "journal.hpp"
#include "ui.hpp"
#include <algorithm>
#include <format>
namespace terrarium {
using namespace sengine::drawing;
namespace {
constexpr sengine::drawing::color ink{57, 62, 43, 255}, faded{106, 108, 82, 255};
}
float user_interface::journal_panel(const world_state& world, sengine::drawing::rect r, bool measure_only) {
    auto text = [&](const std::string& value, float x, float y, float size, sengine::drawing::color color,
                    bool display = false) {
        if (!measure_only)
            this->text(value, x, y, size, color, display);
    };
    auto wrapped = [&](const std::string& value, float x, float y, float width, float size,
                       sengine::drawing::color color) {
        return this->wrapped(value, x, y, width, size, color, measure_only);
    };
    float x = r.x + 30, y = r.y, w = r.width - 60;
    text("The field journal", x, y + 23, 35, ink, true);
    text(std::format("{} / Day {}", world.season(), static_cast<int>(world.day()) + 1), x, y + 67, 14, faded);
    if (!measure_only && button(control_bounds(ui_control::save), "Save [F5]"))
        request = ui_request::save;
    if (!measure_only && button(control_bounds(ui_control::load), "Reload [F9]"))
        request = ui_request::restore;
    auto census = world.populations();
    float row = wrapped(std::format("{} aphids, {} snails, {} ladybirds. {} plants reaching for the light.",
                                    census[0], census[1], census[2], world.plants().size()),
                        x, y + 110, w, 17, ink);
    row = std::max(y + 174, row + 15);
    text("Familiar faces", x, row, 26, ink, true);
    row += 41;
    int favourites = 0;
    for (const auto& a : world.family())
        if (a.favourite) {
            ++favourites;
            if (!measure_only &&
                button({x, row, w, 36}, std::format("{}  /  {}{}", world.name(a.id), species_name(a.species),
                                                    a.died < 0 ? "" : "  /  remembered"))) {
                selected = a.id;
                reveal_family();
            }
            row += 44;
        }
    if (favourites == 0) {
        row = wrapped("Mark a creature with a star to keep its story here, even after it is gone.", x, row, w,
                      15, faded) +
              17;
    }
    row += 20;
    text("Life through the days", x, row, 26, ink, true);
    row += 42;
    row =
        wrapped("Recorded counts, including eggs. Each plot has its own count scale. Hover to read a sample.",
                x, row, w, 15, faded) +
        30;
    const int columns = w < 420 ? 1 : 2;
    if (!measure_only) {
        const journal_timeline timeline(world);
        constexpr std::array<const char*, 4> series_names{"Aphids", "Snails", "Ladybirds", "Plants"};
        constexpr std::array<sengine::drawing::color, 4> series_colors{
            sengine::drawing::color{91, 119, 63, 255}, sengine::drawing::color{150, 105, 67, 255},
            sengine::drawing::color{173, 78, 62, 255}, sengine::drawing::color{57, 117, 105, 255}};
        const float plot_width = (w - (columns - 1) * 26) / columns;
        for (int series = 0; series < 4; ++series) {
            float cx = x + (series % columns) * (plot_width + 26);
            float cy = row + (series / columns) * 178;
            sengine::drawing::rect plot{cx + 34, cy + 45, plot_width - 40, 96};
            const int ceiling = timeline.ceiling(series);
            const bool hover =
                check_collision_point_rec(mouse(), plot) && check_collision_point_rec(mouse(), clip_);
            const auto index =
                hover ? timeline.nearest((mouse().x - plot.x) / plot.width) : timeline.samples().size() - 1;
            const auto& observation = timeline.samples()[index];
            text(std::format("{}  {}", series_names[series], journal_timeline::count(observation, series)),
                 cx, cy, 19, series_colors[series], true);
            if (hover)
                text(std::format("Day {:.2f}{}", observation.day + 1,
                                 index + 1 == timeline.samples().size() ? " / now" : ""),
                     cx, cy + 23, 13, faded);
            for (int line = 0; line <= 2; ++line) {
                float yy = plot.y + plot.height * line / 2;
                draw_line_ex({plot.x, yy}, {plot.x + plot.width, yy}, 1, rgb(203, 192, 161));
            }
            text(std::to_string(ceiling), cx, plot.y - 5, 13, faded);
            text("0", cx + 12, plot.y + plot.height - 6, 13, faded);
            auto point = [&](const population_sample& sample) {
                return sengine::drawing::point2{
                    plot.x + static_cast<float>(timeline.fraction(sample.day)) * plot.width,
                    plot.y + plot.height *
                                 (1 - static_cast<float>(journal_timeline::count(sample, series)) / ceiling)};
            };
            const auto& samples = timeline.samples();
            for (std::size_t i = 1; i < samples.size(); ++i)
                draw_line_ex(point(samples[i - 1]), point(samples[i]), 1.8f, series_colors[series]);
            auto live = point(samples.back());
            draw_circle(live, 3, series_colors[series]);
            if (hover) {
                auto chosen = point(observation);
                draw_line_ex({chosen.x, plot.y}, {chosen.x, plot.y + plot.height}, 1, faded);
                draw_circle(chosen, 4, series_colors[series]);
            }
            if (timeline.first_day() != timeline.last_day())
                text(std::format("Day {:.2f}", timeline.first_day() + 1), plot.x, plot.y + plot.height + 9,
                     13, faded);
            std::string end_label = std::format("{:.2f} now", timeline.last_day() + 1);
            text(end_label, plot.x + plot.width - measure_text_ex(body_, end_label.c_str(), 13, 0).x,
                 plot.y + plot.height + 9, 13, faded);
        }
    }
    row += (4 / columns) * 178;
    row = wrapped("A census is recorded every sixth of a garden day; the final dot is live. Lines join "
                  "observations.",
                  x, row, w, 14, faded) +
          28;
    text("The life of this garden", x, row, 26, ink, true);
    row += 52;
    if (!measure_only) {
        constexpr std::array food_icons{2, 4, 6};
        constexpr std::array food_labels{"Leaves", "Aphids", "Ladybirds"};
        for (int i = 0; i < 3; ++i) {
            float center = x + (i + .5f) * w / 3;
            icon(food_icons[i], {center, row + 14}, 32, ink);
            float label_width = measure_text_ex(body_, food_labels[i], 15, 0).x;
            text(food_labels[i], center - label_width / 2, row + 43, 15, faded);
            if (i < 2) {
                float arrow = x + (i + 1) * w / 3;
                draw_line_ex({arrow - 17, row + 14}, {arrow + 17, row + 14}, 1.5f, faded);
                draw_line_ex({arrow + 11, row + 8}, {arrow + 17, row + 14}, 1.5f, faded);
                draw_line_ex({arrow + 11, row + 20}, {arrow + 17, row + 14}, 1.5f, faded);
            }
        }
    }
    row += 88;
    row = wrapped("Leaves feed aphids; ladybirds hunt aphids and sip nectar. Flowers help adults through a "
                  "prey shortage, but breeding still needs aphid meals. Snails eat leaves and litter; "
                  "damp litter returns nutrients to the soil.",
                  x, row, w, 17, ink) +
          28;
    const auto& today = world.today();
    text("Today in the garden", x, row, 26, ink, true);
    row += 42;
    row = wrapped(std::format("{} new lives, {} hatched, {} new seeds. {} eggs are resting in the garden.",
                              today.born, today.hatched, today.seedlings, world.eggs()),
                  x, row, w, 16, ink) +
          16;
    const int losses = today.hunted + today.starved + today.weather_deaths + today.old_age;
    row = wrapped(losses ? std::format("Losses: {} caught by ladybirds, {} ran out of food, {} from "
                                       "temperature stress, {} from old age.",
                                       today.hunted, today.starved, today.weather_deaths, today.old_age)
                         : "No creatures lost today.",
                  x, row, w, 16, faded) +
          28;
    row = matter_journal(world, x, row, w, measure_only);
    text("Notes from the garden", x, row, 26, ink, true);
    row += 44;
    for (const auto& event : world.journal()) {
        text(std::format("DAY {:.1f}", event.day + 1), x, row, 11, faded);
        row = wrapped(event.message, x, row + 20, w, 16, ink) + 24;
    }
    if (world.journal().empty())
        row = wrapped("No notes yet. Tend the garden and watch its story grow.", x, row, w, 16, faded) + 24;
    return row - y;
}
}
