#include "portraits.hpp"
#include "drawing_color.hpp"
#include <cmath>
namespace terrarium {
using namespace sengine::drawing;
void draw_creature_portrait(const creature_state& c, sengine::drawing::rect r) {
    float x = r.x + r.width * .5f, y = r.y + r.height * .55f, u = r.width / 180;
    sengine::drawing::color dark = rgb(64, 68, 43), green = rgb(118, 143, 76), shell = rgb(181, 135, 74);
    if (c.species == species_kind::snail) {
        draw_ellipse(x, y + 25 * u, 65 * u, 14 * u, green);
        draw_circle(x - 10 * u, y - 8 * u, 38 * u, shell);
        for (int i = 1; i < 95; ++i) {
            float a = i * .14f, b = (i + 1) * .14f, ra = 2 * std::exp(a * .17f) * u,
                  rb = 2 * std::exp(b * .17f) * u;
            draw_line_ex({x - 10 * u + std::cos(a) * ra, y - 8 * u + std::sin(a) * ra},
                         {x - 10 * u + std::cos(b) * rb, y - 8 * u + std::sin(b) * rb}, 2 * u, dark);
        }
        for (int sign : {-1, 1}) {
            draw_line_ex({x + 48 * u, y + 23 * u}, {x + (56 + sign * 8) * u, y - 10 * u}, 3 * u, green);
            draw_circle(x + (56 + sign * 8) * u, y - 10 * u, 3 * u, dark);
        }
    } else {
        for (int side : {-1, 1})
            for (int leg = 0; leg < 3; ++leg)
                draw_line_ex({x + side * 22 * u, y + (leg - 1) * 19 * u},
                             {x + side * 48 * u, y + (leg - 1) * 30 * u}, 3 * u, dark);
        draw_ellipse(x, y, 32 * u, 44 * u, c.species == species_kind::ladybird ? rgb(164, 67, 43) : green);
        draw_circle(x, y - 39 * u, 16 * u, dark);
        for (int side : {-1, 1})
            draw_line_ex({x + side * 9 * u, y - 47 * u}, {x + side * 21 * u, y - 62 * u}, 2 * u, dark);
        if (c.species == species_kind::ladybird) {
            draw_line_ex({x, y - 25 * u}, {x, y + 41 * u}, 2 * u, dark);
            for (int side : {-1, 1})
                for (int dot = 0; dot < 3; ++dot)
                    draw_circle(x + side * (dot == 1 ? 20 : 12) * u, y + (dot - 1) * 23 * u, 6 * u, dark);
        }
    }
}
void draw_plant_portrait(const plant_state& p, sengine::drawing::rect r) {
    float x = r.x + r.width * .5f, y = r.y + r.height * .87f, u = r.height / 156;
    sengine::drawing::color stem = rgb(68, 99, 57), leaf = rgb(115, 139, 70);
    draw_line_ex({x, y}, {x, y - 110 * u}, 3 * u, stem);
    for (int side : {-1, 1})
        for (int i = 0; i < 5; ++i) {
            float yy = y - (22 + i * 18) * u;
            draw_line_ex({x, yy}, {x + side * (32 - i * 3) * u, yy - 14 * u}, 2 * u, stem);
            draw_ellipse(x + side * (27 - i * 2) * u, yy - 12 * u, (p.kind == plant_kind::fern ? 20 : 14) * u,
                         8 * u, leaf);
        }
    if (p.kind == plant_kind::flower) {
        for (int i = 0; i < 7; ++i) {
            float a = i * 2 * pi / 7;
            draw_circle(x + std::cos(a) * 14 * u, y - 114 * u + std::sin(a) * 14 * u, 10 * u,
                        rgb(229, 200, 141));
        }
        draw_circle(x, y - 114 * u, 7 * u, rgb(153, 111, 52));
    }
}
}
