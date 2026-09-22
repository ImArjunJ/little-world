#include "greenhouse_controls.hpp"
#include "habitat_surface.hpp"
#include <algorithm>
#include <numbers>
namespace terrarium {
namespace {
sengine::point turn(sengine::point a, sengine::point b, float t) {
    float yaw = std::atan2(a.x, -a.z);
    yaw += std::remainder(std::atan2(b.x, -b.z) - yaw, 2 * std::numbers::pi_v<float>) * t;
    float pitch = std::lerp(std::asin(std::clamp(a.y, -1.f, 1.f)), std::asin(std::clamp(b.y, -1.f, 1.f)), t);
    return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}
}
void greenhouse_controls::enter_editor(const std::string& id) {
    if (game_.edit(id)) {
        notice_.clear();
        return_camera_ = sengine::camera(explorer_, false);
        travel_eye_.reset();
        auto jar = greenhouse::spots()[game_.active()->place].position;
        orbit_ = std::atan2(return_camera_.eye.x - jar.x, return_camera_.eye.z - jar.z);
        zoom_ = target_zoom_ = 1.25f;
        elevation_ = .48f;
        focus_ = target_focus_ = {};
        sengine::stop(explorer_);
    }
}
void greenhouse_controls::advance(double seconds, bool reduced_motion) {
    if (game_.mode() != greenhouse_mode::editor)
        return;
    float blend = reduced_motion ? 1.f : float(-std::expm1(-12 * std::clamp(seconds, 0., .1)));
    focus_.x += (target_focus_.x - focus_.x) * blend;
    focus_.y += (target_focus_.y - focus_.y) * blend;
    zoom_ += (target_zoom_ - zoom_) * blend;
}
void greenhouse_controls::pan(float right, float forward) {
    if (game_.mode() != greenhouse_mode::editor)
        return;
    float angle = orbit_ + game_.active()->rotation;
    target_focus_.x += std::cos(angle) * right - std::sin(angle) * forward;
    target_focus_.y -= std::sin(angle) * right + std::cos(angle) * forward;
    float distance = std::hypot(target_focus_.x, target_focus_.y);
    if (distance > world_state::radius) {
        target_focus_.x *= world_state::radius / distance;
        target_focus_.y *= world_state::radius / distance;
    }
}
void greenhouse_controls::open_journal() {
    if (game_.open_journal()) {
        sengine::stop(explorer_);
    }
}
void greenhouse_controls::event(const sengine::input_event& e, bool captured) {
    auto mode = game_.mode();
    if (e.type == sengine::event_type::key_down && !e.key.repeat) {
        auto key = e.key.code;
        if (key == sengine::key_code::escape) {
            game_.leave();
            return;
        }
        if (mode == greenhouse_mode::carry) {
            if (key == sengine::key_code::e && captured) {
                int p = game_.target(sengine::camera(explorer_, false), land_, true);
                if (p >= 0) {
                    game_.place(p);
                    sengine::stop(explorer_);
                }
            }
            return;
        }
        if (mode == greenhouse_mode::explore) {
            if (key == sengine::key_code::j) {
                open_journal();
                return;
            }
            if (captured && (key == sengine::key_code::e || key == sengine::key_code::f)) {
                int place = game_.target(sengine::camera(explorer_, false), land_, false);
                if (auto* g = game_.at(place)) {
                    auto id = g->id;
                    if (key == sengine::key_code::e && explorer_.grounded)
                        enter_editor(id);
                    else if (explorer_.grounded) {
                        auto p = greenhouse::spots()[place].position;
                        auto feet = explorer_.position;
                        if (std::hypot(p.x - feet.x, p.z - feet.z) <= .98f) {
                            game_.lift(id);
                            notice_.clear();
                            sengine::stop(explorer_);
                        } else
                            notice_ = "Step closer to lift this garden.";
                    }
                }
            }
        }
    }
    if (e.type == sengine::event_type::mouse_wheel) {
        if (mode == greenhouse_mode::carry)
            game_.rotate(e.wheel.y * .16f);
        else if (mode == greenhouse_mode::editor)
            target_zoom_ = std::clamp(target_zoom_ - e.wheel.y * .06f, .16f, 2.4f);
    }
    if (e.type == sengine::event_type::mouse_motion && mode == greenhouse_mode::editor &&
        (e.motion.buttons & sengine::middle_button))
        pan(-e.motion.dx * .012f * zoom_, e.motion.dy * .012f * zoom_);
    if (e.type == sengine::event_type::mouse_motion && mode == greenhouse_mode::editor &&
        (e.motion.buttons & sengine::right_button)) {
        orbit_ -= e.motion.dx * .005f;
        elevation_ = std::clamp(elevation_ + e.motion.dy * .004f, .15f, 1.52f);
    }
}
float greenhouse_controls::camera_padding() const {
    float tangent = std::tan(std::max(65.f, return_camera_.fov) * std::numbers::pi_v<float> / 360);
    return std::max(.015f, .006f * std::sqrt(1 + tangent * tangent * (1 + aspect_ * aspect_)));
}
sengine::camera_pose greenhouse_controls::editor_camera() const {
    auto result = return_camera_;
    auto* g = game_.active();
    if (!g || g->place < 0)
        return result;
    float h = float(g->world.design().height());
    float r = float(g->world.design().radius());
    float d = zoom_ * std::max({.62f, h * 2.2f, r * 2.8f}) / std::min(1.f, aspect_ / 1.20f);
    sengine::point target = world_point(focus_, h * .16f);
    result.eye = {target.x + std::sin(orbit_) * std::cos(elevation_) * d, target.y + std::sin(elevation_) * d,
                  target.z + std::cos(orbit_) * std::cos(elevation_) * d};
    sengine::point offset_from_jar{result.eye.x - target.x, result.eye.y - target.y, result.eye.z - target.z};
    sengine::point ray{offset_from_jar.x / d, offset_from_jar.y / d, offset_from_jar.z / d};
    const float padding = camera_padding();
    float clearance = d;
    auto avoid = [&](sengine::box obstacle) {
        obstacle.low = {obstacle.low.x - padding, obstacle.low.y - padding, obstacle.low.z - padding};
        obstacle.high = {obstacle.high.x + padding, obstacle.high.y + padding, obstacle.high.z + padding};
        auto hit = sengine::intersect(target, ray, obstacle, clearance);
        if (hit)
            clearance = std::max(.001f, *hit - .001f);
    };
    for (auto obstacle : land_.obstacles)
        avoid(obstacle);
    const auto feet = return_camera_.feet;
    avoid({{feet.x - .3f, feet.y, feet.z - .3f}, {feet.x + .3f, return_camera_.eye.y + .2f, feet.z + .3f}});
    result.eye = {target.x + ray.x * clearance, target.y + ray.y * clearance, target.z + ray.z * clearance};
    auto delta = sengine::point{target.x - result.eye.x, target.y - result.eye.y, target.z - result.eye.z};
    float len = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    result.direction = {delta.x / len, delta.y / len, delta.z / len};
    result.fov =
        std::min(65.f, 360 / std::numbers::pi_v<float> *
                           std::atan(std::tan(43.f * std::numbers::pi_v<float> / 360) * d / clearance));
    return result;
}
sengine::camera_pose greenhouse_controls::camera() const {
    if (game_.mode() == greenhouse_mode::editor)
        return editor_camera();
    if (game_.mode() == greenhouse_mode::enter_editor || game_.mode() == greenhouse_mode::leave_editor) {
        float t = game_.progress();
        t = t * t * (3 - 2 * t);
        if (game_.mode() == greenhouse_mode::leave_editor)
            t = 1 - t;
        auto to = editor_camera(), result = return_camera_;
        float padding = camera_padding();
        if (!travel_eye_ || travel_eye_->x != to.eye.x || travel_eye_->y != to.eye.y ||
            travel_eye_->z != to.eye.z || travel_padding_ != padding) {
            travel_ = sengine::plan_path(return_camera_.eye, to.eye, land_.obstacles, padding);
            travel_eye_ = to.eye;
            travel_padding_ = padding;
        }
        result.eye = sengine::sample_path(travel_, t);
        result.direction = turn(return_camera_.direction, to.direction, t);
        result.fov = return_camera_.fov + (to.fov - return_camera_.fov) * t;
        return result;
    }
    return sengine::camera(explorer_);
}
sengine::point greenhouse_controls::world_point(vec2 point, float above_soil) const {
    auto* g = game_.active();
    if (!g || g->place < 0)
        return {};
    auto p = greenhouse::spots()[g->place].position;
    float scale = float(g->world.design().radius()) * .8f / world_state::radius;
    float c = std::cos(g->rotation), sn = std::sin(g->rotation);
    return {p.x + (point.x * c + point.y * sn) * scale,
            p.y + float(g->world.design().soil_depth + g->world.design().drainage_depth) +
                presentation::surface_offset(g->world, point) + above_soil,
            p.z + (-point.x * sn + point.y * c) * scale};
}
std::optional<vec2> greenhouse_controls::soil_point(float x, float y, float width, float height) const {
    if (game_.mode() != greenhouse_mode::editor)
        return std::nullopt;
    auto camera = editor_camera();
    auto* g = game_.active();
    auto origin = camera.eye, dir = camera.direction;
    float rl = std::hypot(dir.x, dir.z);
    sengine::point right{-dir.z / rl, 0, dir.x / rl},
        up{-dir.y * right.z, dir.x * right.z - dir.z * right.x, dir.y * right.x};
    float tangent = std::tan(camera.fov * .5f * std::numbers::pi_v<float> / 180),
          sx = (2 * x / width - 1) * width / height * tangent, sy = (1 - 2 * y / height) * tangent;
    sengine::point ray{dir.x + right.x * sx + up.x * sy, dir.y + up.y * sy, dir.z + right.z * sx + up.z * sy};
    auto p = greenhouse::spots()[g->place].position;
    float soil = p.y + float(g->world.design().soil_depth + g->world.design().drainage_depth);
    float c = std::cos(g->rotation), sn = std::sin(g->rotation);
    auto local = [&](sengine::point v) {
        return sengine::point{v.x * c - v.z * sn, v.y, v.x * sn + v.z * c};
    };
    return presentation::intersect_surface(g->world, local({origin.x - p.x, origin.y - soil, origin.z - p.z}),
                                           local(ray));
}
}
