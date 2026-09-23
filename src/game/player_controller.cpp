#include "player_controller.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <type_traits>

namespace terrarium {
namespace {
class binary_reader {
  public:
    explicit binary_reader(const std::filesystem::path& path) : stream_(path, std::ios::binary) {}
    template <class value> void read(value& destination) {
        static_assert(std::is_trivially_copyable_v<value>);
        read_bytes(reinterpret_cast<char*>(&destination), sizeof(value));
    }
    void read_bytes(char* destination, std::size_t size) { stream_.read(destination, size); }
    explicit operator bool() const { return bool(stream_); }

  private:
    std::ifstream stream_;
};
float interpolate_pose(float from, float to, float fraction) {
    return from + (to - from) * fraction;
}
}
inline constexpr double explorer_step = 1.0 / 120.0;
landscape load_landscape(const std::filesystem::path& path) {
    binary_reader file(path);
    char magic[8]{};
    file.read_bytes(magic, 8);
    if (std::memcmp(magic, "LWLAND1\0", 8))
        throw std::runtime_error("Invalid landscape header");
    landscape land;
    file.read(land.side);
    file.read(land.origin);
    file.read(land.spacing);
    if (!file || land.side < 2 || land.side > 2049 || !std::isfinite(land.origin) ||
        !std::isfinite(land.spacing) || land.spacing <= 0)
        throw std::runtime_error("Invalid landscape dimensions");
    land.heights.resize(size_t(land.side) * land.side);
    file.read_bytes(reinterpret_cast<char*>(land.heights.data()), land.heights.size() * sizeof(float));
    uint32_t count{};
    file.read(count);
    if (!file || count > 100000)
        throw std::runtime_error("Invalid landscape collision data");
    land.obstacles.resize(count);
    for (auto& box : land.obstacles) {
        file.read(box.low.x);
        file.read(box.low.y);
        file.read(box.low.z);
        file.read(box.high.x);
        file.read(box.high.y);
        file.read(box.high.z);
        if (!std::isfinite(box.low.x) || !std::isfinite(box.low.y) || !std::isfinite(box.low.z) ||
            !std::isfinite(box.high.x) || !std::isfinite(box.high.y) || !std::isfinite(box.high.z) ||
            box.low.x > box.high.x || box.low.y > box.high.y || box.low.z > box.high.z)
            throw std::runtime_error("Invalid landscape collider");
    }
    if (!file ||
        std::any_of(land.heights.begin(), land.heights.end(), [](float h) { return !std::isfinite(h); }))
        throw std::runtime_error("Truncated or invalid landscape");
    return land;
}
float terrain_height(const landscape& terrain, float x, float z) {
    float gx = std::clamp((x - terrain.origin) / terrain.spacing, 0.f, float(terrain.side - 1));
    float gz = std::clamp((z - terrain.origin) / terrain.spacing, 0.f, float(terrain.side - 1));
    unsigned ix = std::min(unsigned(gx), terrain.side - 2), iz = std::min(unsigned(gz), terrain.side - 2);
    float u = gx - ix, v = gz - iz;
    const auto row = std::size_t(iz) * terrain.side + ix;
    const float near_left = terrain.heights[row], near_right = terrain.heights[row + 1];
    const float far_left = terrain.heights[row + terrain.side],
                far_right = terrain.heights[row + terrain.side + 1];
    if (u + v <= 1)
        return near_left + (near_right - near_left) * u + (far_left - near_left) * v;
    return far_right + (far_left - far_right) * (1 - u) + (near_right - far_right) * (1 - v);
}
bool walkable(const landscape& terrain, point p, float radius, float body_height) {
    const float edge = terrain.origin + (terrain.side - 1) * terrain.spacing;
    if (!std::isfinite(p.x) || !std::isfinite(p.z) || p.x < terrain.origin + radius ||
        p.z < terrain.origin + radius || p.x > edge - radius || p.z > edge - radius)
        return false;
    const float ground = terrain_height(terrain, p.x, p.z);
    for (const auto& box : terrain.obstacles) {
        if (box.low.y >= p.y + body_height || box.high.y <= p.y + .025f)
            continue;
        float dx = p.x - std::clamp(p.x, box.low.x, box.high.x);
        float dz = p.z - std::clamp(p.z, box.low.z, box.high.z);
        if (dx * dx + dz * dz < radius * radius)
            return false;
    }
    const float slope_x =
        (terrain_height(terrain, p.x + .2f, p.z) - terrain_height(terrain, p.x - .2f, p.z)) / .4f;
    const float slope_z =
        (terrain_height(terrain, p.x, p.z + .2f) - terrain_height(terrain, p.x, p.z - .2f)) / .4f;
    return p.y > ground + .15f || std::hypot(slope_x, slope_z) < .9f;
}
namespace {
bool overlaps(point p, const box& box) {
    const float dx = p.x - std::clamp(p.x, box.low.x, box.high.x),
                dz = p.z - std::clamp(p.z, box.low.z, box.high.z);
    return dx * dx + dz * dz < .23f * .23f;
}
}
float support_height(const landscape& terrain, point p, float highest) {
    float result = terrain_height(terrain, p.x, p.z);
    for (const auto& box : terrain.obstacles)
        if (box.high.y <= highest && overlaps(p, box))
            result = std::max(result, box.high.y);
    return result;
}
float ceiling_height(const landscape& terrain, point p, float lowest) {
    float result = 10000;
    for (const auto& box : terrain.obstacles)
        if (box.low.y >= lowest && overlaps(p, box))
            result = std::min(result, box.low.y);
    return result;
}
explorer::explorer(const landscape& terrain, point spawn, float yaw, float pitch) : terrain_(terrain) {
    if (terrain.side < 2 || terrain.heights.size() != std::size_t(terrain.side) * terrain.side)
        throw std::invalid_argument("Explorer needs a valid landscape");
    if (!relocate(spawn, yaw, pitch))
        throw std::runtime_error("Explorer spawn is obstructed");
}
void explorer::look(float yaw, float pitch) {
    if (!std::isfinite(yaw) || !std::isfinite(pitch))
        return;
    yaw_ = std::remainder(yaw_ + yaw, 2 * std::numbers::pi_v<float>);
    pitch_ = std::clamp(pitch_ + pitch, -1.4f, 1.35f);
}
bool explorer::relocate(point feet, float yaw, float pitch) {
    if (!std::isfinite(feet.x) || !std::isfinite(feet.z))
        return false;
    feet.y = terrain_height(terrain_, feet.x, feet.z);
    if (!walkable(terrain_, feet) || !std::isfinite(yaw) || !std::isfinite(pitch))
        return false;
    position_ = {feet.x, terrain_height(terrain_, feet.x, feet.z), feet.z};
    previous_position_ = position_;
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -1.4f, 1.35f);
    velocity_.y = 0;
    grounded_ = true;
    landing_ = landing_velocity_ = landing_impact_ = 0;
    stop();
    return true;
}
void explorer::stop() {
    velocity_.x = velocity_.z = 0;
    pending_ = 0;
    previous_position_ = position_;
    bob_ = previous_bob_ = 0;
    footsteps_ = 0;
    jump_buffer_ = 0;
}
void explorer::advance(double seconds, explorer_input input) {
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(input.forward) ||
        !std::isfinite(input.right))
        return;
    if (input.jump_pressed)
        jump_buffer_ = .14f;

    pending_ += std::min(seconds, .1);
    while (pending_ + 1e-10 >= explorer_step) {
        integrate(input);
        pending_ -= explorer_step;
    }
    pending_ = std::max(0., pending_);
}
void explorer::update_jump(explorer_input input) {
    const float dt = float(explorer_step);
    coyote_ = grounded_ ? .10f : std::max(0.f, coyote_ - dt);
    jump_buffer_ = std::max(0.f, jump_buffer_ - dt);
    if (jump_buffer_ > 0 && coyote_ > 0 && !input.crouching) {
        velocity_.y = 5.6f;
        grounded_ = false;
        coyote_ = jump_buffer_ = 0;
        stride_distance_ = 0;
    }
}
void explorer::accelerate(explorer_input input) {
    const float dt = float(explorer_step);
    float f = std::clamp(input.forward, -1.f, 1.f), r = std::clamp(input.right, -1.f, 1.f);
    const float length = std::max(1.f, std::hypot(f, r));
    const float speed = input.crouching ? 1.0f : input.running ? 4.2f : 2.1f;
    point wish{(std::sin(yaw_) * f + std::cos(yaw_) * r) * speed / length, 0,
               (-std::cos(yaw_) * f + std::sin(yaw_) * r) * speed / length};
    float dx = wish.x - velocity_.x, dz = wish.z - velocity_.z;
    const float delta = std::hypot(dx, dz),
                limit = dt * (grounded_ ? (f == 0 && r == 0 ? 30.f : 18.f) : (f == 0 && r == 0 ? 0.f : 4.5f));
    if (delta > 0) {
        velocity_.x += dx * std::min(1.f, limit / delta);
        velocity_.z += dz * std::min(1.f, limit / delta);
    }
}
void explorer::move_horizontal() {
    for (int axis = 0; axis < 2; ++axis) {
        auto p = position_;
        if (axis == 0)
            p.x += velocity_.x * explorer_step;
        else
            p.z += velocity_.z * explorer_step;
        float support = support_height(terrain_, p, position_.y + .22f);
        if (grounded_ && std::abs(support - position_.y) < .22f)
            p.y = support;
        if (walkable(terrain_, p, .23f, eye_height_ + .17f)) {
            position_ = p;
        } else if (axis == 0)
            velocity_.x = 0;
        else
            velocity_.z = 0;
    }
}
void explorer::move_vertical() {
    const float dt = float(explorer_step);
    const float floor =
        support_height(terrain_, position_, (grounded_ ? position_.y : previous_position_.y) + .025f);
    if (!grounded_ || position_.y > floor + .025f) {
        grounded_ = false;
        velocity_.y -= 16.f * dt;
        const float top = ceiling_height(terrain_, position_, position_.y + eye_height_ + .16f);
        position_.y += velocity_.y * dt;
        if (position_.y + eye_height_ + .17f > top && velocity_.y > 0) {
            position_.y = top - eye_height_ - .17f;
            velocity_.y = 0;
        }
        if (position_.y <= floor) {
            const float impact = std::max(0.f, -velocity_.y);
            position_.y = floor;
            velocity_.y = 0;
            grounded_ = true;
            if (impact > 1.5f) {
                landing_velocity_ = -std::min(.8f, impact * .10f);
                landing_impact_ = impact;
            }
        }
    } else {
        position_.y = floor;
        velocity_.y = 0;
    }
}
void explorer::update_gait(explorer_input input) {
    const float dt = float(explorer_step);
    landing_velocity_ += (-150.f * landing_ - 22.f * landing_velocity_) * dt;
    landing_ += landing_velocity_ * dt;
    run_blend_ += (((input.running || !grounded_) && speed() > 2.5f ? 1.f : 0.f) - run_blend_) *
                  (1 - std::exp(-8 * dt));
    const float travelled =
        std::hypot(position_.x - previous_position_.x, position_.z - previous_position_.z);
    const float stride = input.running ? 1.12f : .76f;
    if (grounded_) {
        distance_ += travelled / stride;
        stride_distance_ += travelled;
    }
    if (grounded_ && stride_distance_ >= stride) {
        stride_distance_ = std::fmod(stride_distance_, stride);
        ++footsteps_;
    }
    const float blend = 1 - std::exp(-18 * float(explorer_step));
    bob_ += ((grounded_ ? std::min(1.f, travelled / dt / 2.1f) : 0.f) - bob_) * blend;
    float desired_eye = input.crouching ? 1.10f : 1.62f;
    if (desired_eye > eye_height_ && !walkable(terrain_, position_, .23f, 1.79f))
        desired_eye = 1.10f;
    eye_height_ += (desired_eye - eye_height_) * (1 - std::exp(-12 * dt));
}
void explorer::integrate(explorer_input input) {
    previous_position_ = position_;
    previous_eye_ = eye_height_;
    previous_bob_ = bob_;
    update_jump(input);
    accelerate(input);
    move_horizontal();
    move_vertical();
    update_gait(input);
}
camera_pose explorer::camera(bool motion) const {
    const float t = float(pending_ / explorer_step);
    point eye{interpolate_pose(previous_position_.x, position_.x, t),
              interpolate_pose(previous_position_.y, position_.y, t) +
                  interpolate_pose(previous_eye_, eye_height_, t),
              interpolate_pose(previous_position_.z, position_.z, t)};

    eye.x += std::sin(yaw_) * .20f;
    eye.z -= std::cos(yaw_) * .20f;
    const float phase = distance_ * std::numbers::pi_v<float>;
    const float gait = interpolate_pose(previous_bob_, bob_, t);
    if (motion) {
        eye.y += (.018f + .009f * run_blend_) * gait * std::cos(phase * 2) + landing_;
        const float sway = .010f * gait * std::sin(phase);
        eye.x += std::cos(yaw_) * sway;
        eye.z += std::sin(yaw_) * sway;
    }
    point feet{interpolate_pose(previous_position_.x, position_.x, t),
               interpolate_pose(previous_position_.y, position_.y, t),
               interpolate_pose(previous_position_.z, position_.z, t)};
    return {
        eye,       {std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), -std::cos(yaw_) * std::cos(pitch_)},
        feet,      yaw_,
        phase,     gait,
        landing_,  motion ? 65.f + 3.f * run_blend_ : 65.f,
        grounded_, speed()};
}
unsigned explorer::take_footsteps() {
    auto result = footsteps_;
    footsteps_ = 0;
    return result;
}
float explorer::take_landing() {
    auto result = landing_impact_;
    landing_impact_ = 0;
    return result;
}
}
