#include "carry_pose.hpp"
#include <algorithm>
#include <cmath>

namespace terrarium {
carry_pose compute_carry_pose(const sengine::camera_pose& player, const garden_design& design) {
    const float radius = float(design.radius());
    const sengine::point forward{std::sin(player.yaw), 0, -std::cos(player.yaw)},
        right{std::cos(player.yaw), 0, std::sin(player.yaw)};
    carry_pose pose;
    pose.base = {player.eye.x + forward.x * .47f,
                 player.eye.y - .23f - std::max(0.f, float(design.height()) - .25f) * .35f,
                 player.eye.z + forward.z * .47f};
    for (unsigned i = 0; i < pose.wrists.size(); ++i) {
        const float side = i ? 1.f : -1.f;
        pose.wrists[i] = {pose.base.x + right.x * side * radius * .82f - forward.x * radius * .4f,
                          pose.base.y - .025f,
                          pose.base.z + right.z * side * radius * .82f - forward.z * radius * .4f};
    }
    return pose;
}
}
