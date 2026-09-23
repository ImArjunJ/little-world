#pragma once
#include "sengine/geometry.hpp"
#include <cmath>
#include <filesystem>
#include <vector>

namespace terrarium {
using sengine::box;
using sengine::point;
struct landscape {
    unsigned side{};
    float origin{}, spacing{1};
    std::vector<float> heights;
    std::vector<box> obstacles;
};
landscape load_landscape(const std::filesystem::path& path);
float terrain_height(const landscape&, float x, float z);
bool walkable(const landscape&, point feet, float radius = .23f, float body_height = 1.8f);
float support_height(const landscape&, point feet, float highest);
float ceiling_height(const landscape&, point feet, float lowest);

struct explorer_input {
    float forward{}, right{};
    bool running{}, crouching{}, jump_pressed{};
};
struct camera_pose {
    point eye{}, direction{0, 0, -1}, feet{};
    float yaw{}, stride{}, gait{}, landing{}, fov{65};
    bool grounded{true};
    float speed{};
};
class explorer {
  public:
    explicit explorer(const landscape&, point spawn = {}, float yaw = 0, float pitch = 0);
    void look(float yaw, float pitch);
    void advance(double seconds, explorer_input input);
    void stop();
    bool relocate(point feet, float yaw, float pitch);
    camera_pose camera(bool motion = true) const;
    unsigned take_footsteps();
    float take_landing();
    point position() const noexcept { return position_; }
    bool grounded() const noexcept { return grounded_; }
    float speed() const { return std::hypot(velocity_.x, velocity_.z); }

  private:
    void integrate(explorer_input);
    void update_jump(explorer_input);
    void accelerate(explorer_input);
    void move_horizontal();
    void move_vertical();
    void update_gait(explorer_input);

  private:
    const landscape& terrain_;
    point position_{}, previous_position_{}, velocity_{};
    float yaw_{}, pitch_{}, eye_height_{1.62f}, previous_eye_{1.62f};
    float distance_{}, bob_{}, previous_bob_{}, stride_distance_{};
    double pending_{};
    unsigned footsteps_{};
    bool grounded_{true};
    float jump_buffer_{}, coyote_{}, landing_{}, landing_velocity_{}, landing_impact_{}, run_blend_{};
};
}
