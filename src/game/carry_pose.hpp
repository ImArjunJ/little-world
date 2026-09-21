#pragma once
#include "construction.hpp"
#include "sengine/explorer.hpp"
#include <array>

namespace terrarium {
struct carry_pose {
    sengine::point base;
    std::array<sengine::point, 2> wrists;
};
carry_pose compute_carry_pose(const sengine::camera_pose&, const garden_design&);
}
