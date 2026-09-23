#include "drawing_color.hpp"
#include "ui.hpp"
#include <cmath>
namespace terrarium {
namespace {
using namespace sengine::drawing;
constexpr color brass{197, 170, 111, 255}, dark{40, 50, 30, 240};
class icon_painter {
  public:
    icon_painter(point2 position, float size, color ink) : p(position), u(size / 32), c(ink) {}
    void draw(int id) const {
        switch (id) {
        case 0:
            inspect();
            break;
        case 1:
            fern();
            break;
        case 2:
            clover();
            break;
        case 3:
            flower();
            break;
        case 4:
            insect(id);
            break;
        case 6:
            insect(id);
            break;
        case 5:
            snail();
            break;
        case 7:
            water();
            break;
        case 8:
            sun();
            break;
        case 9:
            journal();
            break;
        case 10:
            more();
            break;
        case 11:
            home();
            break;
        case 12:
            observe();
            break;
        case 13:
            pause();
            break;
        case 14:
            play();
            break;
        case 15:
            favourite();
            break;
        case 16:
            pencil();
            break;
        case 17:
            target();
            break;
        case 18:
            close();
            break;
        case 19:
            camera();
            break;
        default:
            break;
        }
    }

  private:
    point2 project(float x, float y) const { return {p.x + x * u, p.y + y * u}; }
    void line(float x, float y, float a, float b, float thick = 1.5f) const {
        draw_line_ex(project(x, y), project(a, b), thick * u, c);
    }
    void circle(float x, float y, float radius) const { draw_circle_lines(project(x, y), radius * u, c); }
    void inspect() const {

        circle(-3, -3, 8);
        line(3, 3, 12, 12, 2.3f);
        line(-8, -5, -4, -8, 1);
    }
    void fern() const {

        line(-7, 12, 7, -13);
        for (int i = 0; i < 6; ++i) {
            float x = -5 + i * 2, y = 8 - i * 3.5f;
            line(x, y, x - 8 + i * .7f, y - 4);
            line(x, y, x + 7 - i * .65f, y - 1);
        }
    }
    void clover() const {

        line(0, 3, -4, 13);
        for (int i = 0; i < 3; ++i) {
            float a = i * 2 * pi / 3 - pi / 2;
            draw_circle(project(std::cos(a) * 5, std::sin(a) * 5), 5 * u, c);
        }
        draw_circle(project(0, 0), 1.5f * u, dark);
    }
    void flower() const {

        line(0, -2, 0, 13);
        line(0, 8, -7, 4);
        line(0, 5, 6, 2);
        for (int i = 0; i < 6; ++i) {
            float a = i * 2 * pi / 6;
            draw_circle(project(std::cos(a) * 6, -5 + std::sin(a) * 6), 3.2f * u, c);
        }
        draw_circle(project(0, -5), 3 * u, brass);
    }
    void insect(int id) const {

        draw_ellipse(static_cast<int>(p.x), static_cast<int>(p.y + u), 7 * u, 10 * u,
                     id == 6 ? rgb(187, 108, 80) : c);
        circle(0, -10, 3);
        for (int i = 0; i < 3; ++i) {
            float y = -5 + i * 5;
            line(-6, y, -11, y - 3);
            line(6, y, 11, y - 3);
        }
        line(0, -6, 0, 9, 1);
        if (id == 6)
            for (float side : {-1.f, 1.f})
                for (int j = 0; j < 2; ++j)
                    draw_circle(project(side * 3, -2 + j * 6), 1.5f * u, dark);
    }
    void snail() const {

        line(-12, 10, 11, 10, 2);
        line(11, 10, 12, 2);
        line(12, 2, 9, -3);
        line(12, 2, 15, -3);
        circle(-2, 1, 8);
        for (int i = 0; i < 40; ++i) {
            float a = i * .19f, b = (i + 1) * .19f, r = 1 + i * .12f;
            draw_line_ex(project(-2 + std::cos(a) * r, 1 + std::sin(a) * r),
                         project(-2 + std::cos(b) * (r + .12f), 1 + std::sin(b) * (r + .12f)), u, c);
        }
    }
    void water() const {

        draw_triangle(project(0, -13), project(-7, 1), project(7, 1), c);
        draw_circle(project(0, 2), 7 * u, c);
        line(-3, 0, -3, 4, 1);
    }
    void sun() const {

        circle(0, 0, 6);
        for (int i = 0; i < 8; ++i) {
            float a = i * pi / 4;
            line(std::cos(a) * 9, std::sin(a) * 9, std::cos(a) * 13, std::sin(a) * 13);
        }
    }
    void journal() const {

        line(0, -9, 0, 12);
        line(-12, -10, -12, 9);
        line(12, -10, 12, 9);
        line(-12, -10, -2, -8);
        line(12, -10, 2, -8);
        line(-12, 9, 0, 12);
        line(12, 9, 0, 12);
        for (int j = 0; j < 3; ++j) {
            line(-9, -5 + j * 4, -3, -4 + j * 4, 1);
            line(3, -4 + j * 4, 9, -5 + j * 4, 1);
        }
    }
    void more() const {

        for (int i = 0; i < 3; ++i)
            draw_circle(project(-8 + i * 8, 0), 2 * u, c);
    }
    void home() const {

        line(-12, 0, 0, -11);
        line(0, -11, 12, 0);
        line(-8, -2, -8, 11);
        line(8, -2, 8, 11);
        line(-8, 11, 8, 11);
        line(-2, 11, -2, 4);
        line(3, 11, 3, 4);
    }
    void observe() const {

        draw_ellipse_lines(static_cast<int>(p.x), static_cast<int>(p.y), 13 * u, 7 * u, c);
        circle(0, 0, 4);
    }
    void pause() const {

        line(-4, -9, -4, 9, 3);
        line(4, -9, 4, 9, 3);
    }
    void play() const { draw_triangle(project(-5, -9), project(-5, 9), project(9, 0), c); }
    void favourite() const {

        for (int i = 0; i < 10; ++i) {
            float a = i * pi / 5 - pi / 2, b = (i + 1) * pi / 5 - pi / 2, r = i % 2 ? 5 : 12,
                  t = i % 2 ? 12 : 5;
            line(std::cos(a) * r, std::sin(a) * r, std::cos(b) * t, std::sin(b) * t);
        }
    }
    void pencil() const {

        line(-9, 10, 8, -10, 3);
        line(-10, 12, -7, 11);
        line(-8, 14, 10, 14, 1);
    }
    void target() const {

        circle(0, 0, 8);
        line(-13, 0, -5, 0);
        line(5, 0, 13, 0);
        line(0, -13, 0, -5);
        line(0, 5, 0, 13);
    }
    void close() const {

        line(-7, -7, 7, 7);
        line(-7, 7, 7, -7);
    }
    void camera() const {

        line(-12, -7, -12, 10);
        line(-12, 10, 12, 10);
        line(12, 10, 12, -7);
        line(12, -7, 5, -7);
        line(5, -7, 3, -11);
        line(3, -11, -4, -11);
        line(-4, -11, -6, -7);
        line(-6, -7, -12, -7);
        circle(0, 1, 5);
    }

  private:
    point2 p;
    float u;
    color c;
};
}
void user_interface::icon(int id, sengine::drawing::point2 position, float size,
                          sengine::drawing::color ink) {
    icon_painter(position, size, ink).draw(id);
}
}
