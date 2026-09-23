#include "drawing_color.hpp"
#include "journal.hpp"
#include "ui.hpp"
#include <algorithm>
#include <format>
#include <numeric>
namespace terrarium {
using namespace sengine::drawing;
namespace {
constexpr sengine::drawing::color ink{57, 62, 43, 255}, faded{106, 108, 82, 255};
constexpr std::array<sengine::drawing::color, 5> colors{
    {{189, 156, 72, 255}, {90, 123, 73, 255}, {155, 92, 71, 255}, {132, 110, 80, 255}, {116, 115, 154, 255}}};
constexpr std::array labels{"Ready for roots", "In plants", "In creatures", "In fallen matter",
                            "In soil microbes"};
}
void user_interface::soil_panel(world_state& world, sengine::drawing::rect r) {
    const float x = r.x + 28, y = r.y, w = r.width - 56;
    text("The living soil", x, y + 22, 32, ink, true);
    wrapped("Leaves fall. Small lives feed. The soil begins again.", x, y + 68, w, 17, faded);
    const auto sample = world.matter_sample();
    const double total = std::accumulate(sample.nitrogen.begin(), sample.nitrogen.end(), 0.);
    text("Where the nitrogen rests", x, y + 142, 23, ink, true);
    float cursor = x;
    for (int i = 0; i < 5; ++i) {
        const float length = total > 0 ? w * sample.nitrogen[i] / total : 0;
        draw_rectangle_rec({cursor, y + 181, length, 12}, colors[i]);
        cursor += length;
        draw_circle({x + 5, y + 220 + i * 28}, 4, colors[i]);
        text(labels[i], x + 17, y + 212 + i * 28, 15, faded);
        const auto value = std::format("{:.0f} mg", sample.nitrogen[i]);
        text(value, x + w - measure_text_ex(body_, value.c_str(), 15, 0).x, y + 212 + i * 28, 15, ink);
    }
    wrapped("Roots can use the gold share. The rest is held in living tissue or waiting to break down.", x,
            y + 373, w, 16, ink);
    if (button({x, y + 463, w, 40}, "Take a pinch of compost", tool == garden_tool::compost)) {
        tool = garden_tool::compost;
        close_panel();
    }
    wrapped("Rich compost supplies nitrogen as microbes digest it. Click a soil patch to add 0.5 g.", x,
            y + 518, w, 15, faded);
    if (button({x, y + 594, w, 40}, "Take some woody mulch", tool == garden_tool::mulch)) {
        tool = garden_tool::mulch;
        close_panel();
    }
    wrapped("Wood feeds decomposers, but they borrow nitrogen from roots. Try it beside rich compost and "
            "watch what changes.",
            x, y + 650, w, 15, faded);
    wrapped(std::format("Today, soil life released {:.1f} mg of nitrogen and borrowed {:.1f} mg. Moisture "
                        "and warmth set the pace.",
                        world.today().mineralized_nitrogen, world.today().immobilized_nitrogen),
            x, y + 749, w, 16, ink);
    if (button({x, y + 857, w, 36}, "Follow the cycle in the journal")) {
        panel_ = panel_kind::journal;
        scroll_ = 0;
    }
}
float user_interface::matter_journal(const world_state& world, float x, float y, float w, bool measure_only) {
    const panel_text content(*this, measure_only);
    content.text("Nothing simply disappears", x, y, 26, ink, true);
    const float description_end = content.wrapped(
        "Nitrogen moves through roots, neighbours and the soil. Each colour shows a measured store, in "
        "milligrams.",
        x, y + 40, w, 16, faded);
    const matter_timeline timeline(world);
    const auto& samples = timeline.samples();
    sengine::drawing::rect plot{x + 44, description_end + 24, w - 48, 115};
    const double ceiling = timeline.ceiling();
    if (!measure_only) {
        for (int line = 0; line < 3; ++line) {
            float py = plot.y + plot.height * line / 2;
            draw_line_ex({plot.x, py}, {plot.x + plot.width, py}, 1, rgb(200, 190, 160));
        }
        content.text(std::format("{:.0f}", ceiling), x, plot.y - 4, 12, faded);
        content.text("0", x + 26, plot.y + plot.height - 6, 12, faded);
        for (std::size_t i = 1; i < samples.size(); ++i) {
            const float xa = plot.x + timeline.fraction(samples[i - 1].day) * plot.width;
            const float xb = plot.x + timeline.fraction(samples[i].day) * plot.width;
            double low_a = 0, low_b = 0;
            for (int pool = 0; pool < 5; ++pool) {
                double high_a = low_a + samples[i - 1].nitrogen[pool],
                       high_b = low_b + samples[i].nitrogen[pool];
                const sengine::drawing::point2 a{xa, plot.y + plot.height * float(1 - low_a / ceiling)},
                    b{xa, plot.y + plot.height * float(1 - high_a / ceiling)},
                    c{xb, plot.y + plot.height * float(1 - high_b / ceiling)},
                    d{xb, plot.y + plot.height * float(1 - low_b / ceiling)};
                draw_triangle(a, c, b, fade(colors[pool], .65f));
                draw_triangle(a, d, c, fade(colors[pool], .65f));
                draw_line_ex(b, c, 1, colors[pool]);
                low_a = high_a;
                low_b = high_b;
            }
        }
    }
    const bool hover = check_collision_point_rec(mouse(), plot) && check_collision_point_rec(mouse(), clip_);
    const auto& chosen =
        samples[hover ? timeline.nearest((mouse().x - plot.x) / plot.width) : samples.size() - 1];
    if (hover && !measure_only) {
        const float px = plot.x + timeline.fraction(chosen.day) * plot.width;
        draw_line_ex({px, plot.y}, {px, plot.y + plot.height}, 1, ink);
    }
    content.text(std::format("Day {:.2f}", samples.front().day + 1), plot.x, plot.y + plot.height + 9, 13,
                 faded);
    const auto end = std::format("{:.2f} now", samples.back().day + 1);
    content.text(end, plot.x + plot.width - measure_text_ex(body_, end.c_str(), 13, 0).x,
                 plot.y + plot.height + 9, 13, faded);
    float row = plot.y + plot.height + 45;
    const int columns = w < 500 ? 1 : 2;
    for (int i = 0; i < 5; ++i) {
        const float cx = x + (i % columns) * w / columns, cy = row + (i / columns) * 26;
        if (!measure_only)
            draw_circle({cx + 4, cy + 8}, 4, colors[i]);
        content.text(std::format("{}: {:.1f} mg", labels[i], chosen.nitrogen[i]), cx + 16, cy, 14, faded);
    }
    row += ((5 + columns - 1) / columns) * 26 + 15;
    const std::string live_caption =
        "Values show the garden now. Hover the plot to read an earlier observation.";
    const auto sampled_caption =
        std::format("Observed on day {:.2f}. Values come from that sample, not an interpolated estimate.",
                    chosen.day + 1);
    const float caption_end = std::max(this->wrapped(live_caption, x, row, w, 14, faded, true),
                                       this->wrapped(sampled_caption, x, row, w, 14, faded, true));
    content.wrapped(hover ? sampled_caption : live_caption, x, row, w, 14, faded);
    row = caption_end + 24;
    const auto& ledger = world.matter_ledger();
    return content.wrapped(
               std::format("Since records began: {:.2f} g carbon captured from air; {:.2f} g returned by "
                           "respiration. Added plants, creatures and compost supplied {:.2f} g nitrogen.",
                           ledger.fixed_carbon / 1000, ledger.respired_carbon / 1000,
                           ledger.introduced.nitrogen / 1000),
               x, row, w, 15, ink) +
           40;
}
}
