#include "garden_scene_state.hpp"
namespace terrarium::render {
using namespace sengine;
namespace {
const char* locomotion_clip(const terrarium::camera_pose& player) {
    if (!player.grounded)
        return "Jump_Loop";
    if (player.eye.y - player.feet.y < 1.40f)
        return player.speed > .15f ? "Crouch_Fwd_Loop" : "Crouch_Idle_Loop";
    if (player.speed > 4.f)
        return "Sprint_Loop";
    if (player.speed > 3.1f)
        return "Jog_Fwd_Loop";
    return player.speed > .15f ? "Walk_Loop" : "Idle_Loop";
}
}
gardener::gardener(scene& resources, const std::filesystem::path& path, model_instance& wardrobe,
                   const std::array<model_node, 2>& boots)
    : body_(model(resources, path)), wardrobe_(wardrobe) {
    for (const auto& entry : body_.nodes()) {
        if (!entry.name.empty() &&
            !joints_.emplace(entry.name, joint{entry.id, body_.local_transform(entry.id)}).second)
            throw std::runtime_error("Duplicate gardener joint: " + entry.name);
        if (entry.name == "First person hidden head" || entry.name == "Eyebrows" || entry.name == "Eyes" ||
            entry.name == "Hair_Buns")
            if (body_.renderable(entry.id))
                body_.layers(entry.id, 0x02);
    }
    for (auto name : {"Idle_Loop", "Walk_Loop", "Jog_Fwd_Loop", "Sprint_Loop", "Crouch_Idle_Loop",
                      "Crouch_Fwd_Loop", "Jump_Loop"})
        clips_.emplace(name, body_.clip(name));
    for (unsigned i = 0; i < boots_.size(); ++i) {
        const auto foot = world(i ? "foot_r" : "foot_l");
        boots_[i] = {boots[i], inverse(foot) * translation(float3{foot[3].x, 0, foot[3].z}) *
                                   wardrobe_.local_transform(boots[i])};
    }
    body_.visible(true);
    body_.synchronize();
}
model_node gardener::node(const std::string& name) const {
    return joints_.at(name).node;
}
mat4 gardener::world(const std::string& name) const {
    return body_.world_transform(node(name));
}
float3 gardener::position(const std::string& name) const {
    return world(name)[3].xyz();
}
void gardener::rotate_world(const std::string& name, float3 axis, float radians) {
    const auto transform = world(name);
    const auto position = transform[3].xyz();
    body_.world_transform(node(name), translation(position) * rotation(radians, axis) *
                                          translation(-position) * transform);
}
void gardener::aim(const std::string& name, const std::string& child, sengine::float3 target) {
    auto a = position(name);
    auto from = normalize(position(child) - a), to = normalize(target - a);
    auto axis = cross(from, to);
    float l = length(axis);
    if (l > .00001f)
        rotate_world(name, axis / l, std::acos(std::clamp(dot(from, to), -1.f, 1.f)));
}
void gardener::reach(const std::string& upper, const std::string& lower, const std::string& end,
                     sengine::float3 target, sengine::float3 pole) {
    auto a = position(upper);
    float l1 = length(position(lower) - a), l2 = length(position(end) - position(lower));
    auto delta = target - a;
    float len = std::clamp(length(delta), std::abs(l1 - l2) + .001f, l1 + l2 - .001f);
    auto dir = normalize(delta);
    auto side = normalize(pole - a - dir * dot(pole - a, dir));
    float along = (l1 * l1 - l2 * l2 + len * len) / (2 * len);
    auto middle = a + dir * along + side * std::sqrt(std::max(0.f, l1 * l1 - along * along));
    aim(upper, lower, middle);
    aim(lower, end, a + dir * len);
}
void gardener::locomotion(const terrarium::camera_pose& player, double seconds) {
    auto selected = clips_.at(locomotion_clip(player));
    if (!active_clip_ || selected != active_clip_) {
        previous_clip_ = active_clip_.value_or(selected);
        previous_clip_time_ = clip_time_;
        blend_time_ = active_clip_ ? 0.f : 1.f;
        active_clip_ = selected;
        clip_time_ = 0;
    }
    const float duration = float(body_.clips()[*active_clip_].duration);
    clip_time_ = player.grounded && player.speed > .15f
                     ? std::fmod(player.stride / (2 * 3.14159265f), 1.f) * duration
                     : std::fmod(clip_time_ + float(seconds), duration);
    body_.animate(*active_clip_, clip_time_);
    blend_time_ += float(seconds);
    if (blend_time_ < .18f)
        body_.blend(previous_clip_, previous_clip_time_, 1 - ease(blend_time_ / .18f));
}
sengine::float3 gardener::place_body(const terrarium::camera_pose& player) {
    auto body_position = point(player.feet);
    const auto body_rotation = sengine::rotation(3.14159265f - player.yaw, sengine::float3{0, 1, 0});
    body_.transform(sengine::translation(body_position) * body_rotation);
    if (player.grounded) {
        const float lowest_ankle = std::min(position("foot_l").y, position("foot_r").y);
        body_position.y += player.feet.y + .07f - lowest_ankle;
        body_.transform(sengine::translation(body_position) * body_rotation);
    }

    return body_position;
}
void gardener::relax_arms(float support) {
    if (support > 0) {
        for (auto& [name, joint] : joints_) {
            if (!(name.starts_with("upperarm_") || name.starts_with("lowerarm_") ||
                  name.starts_with("hand_") || name.starts_with("index_") || name.starts_with("middle_") ||
                  name.starts_with("ring_") || name.starts_with("pinky_") || name.starts_with("thumb_")))
                continue;
            auto instance = joint.node;
            auto local = body_.local_transform(instance);
            auto rotation = slerp(rotation_of(local), rotation_of(joint.bind), support);
            auto blended = sengine::rotation(rotation);
            blended[3] = local[3];
            body_.local_transform(instance, blended);
        }
    }
}
void gardener::orient_wrist(const std::string& side, float sign, float support, sengine::float3 forward,
                            sengine::float3 right) {
    auto toward = support > .01f ? forward * .06f - right * sign * .09f : sengine::float3{0, -.12f, 0};
    aim("hand_" + side, "middle_01_" + side, position("hand_" + side) + toward);
    const auto wrist = position("hand_" + side);
    const auto finger_axis = normalize(position("middle_01_" + side) - wrist);
    auto palm =
        normalize(cross(position("index_01_" + side) - wrist, position("pinky_01_" + side) - wrist)) * sign;
    palm = normalize(palm - finger_axis * dot(palm, finger_axis));
    auto up = normalize(sengine::float3{0, 1, 0} - finger_axis * finger_axis.y);
    const float roll = std::atan2(dot(finger_axis, cross(palm, up)), dot(palm, up));
    rotate_world("hand_" + side, finger_axis, roll * support);
}
void gardener::curl_fingers(const std::string& side, float support) {
    for (auto f : {"index", "middle", "ring", "pinky"})
        for (auto segment : {"01", "02", "03"}) {
            std::string name = std::string(f) + "_" + segment + "_" + side;
            auto inst = node(name);
            auto local = body_.local_transform(inst);
            body_.local_transform(inst, local * sengine::rotation(support * .08f, sengine::float3{1, 0, 0}));
        }
}
void gardener::animate(const terrarium::camera_pose& player, double seconds, const carrying_pose& holding) {
    locomotion(player, seconds);
    const auto body_position = place_body(player);
    const std::array authored_hands{position("hand_l"), position("hand_r")};
    const auto& carry = holding.pose;
    const auto held = holding.held, grip = holding.grip;
    const float hold = holding.blend, support = holding.support;
    const sengine::float3 forward{std::sin(player.yaw), 0, -std::cos(player.yaw)},
        right{std::cos(player.yaw), 0, std::sin(player.yaw)};
    relax_arms(support);
    float reach_lean =
        (std::clamp((dot(grip - body_position, forward) - .68f) * 1.8f, 0.f, .6f) * (1 - hold) +
         .08f * hold) *
        support;
    if (reach_lean > 0)
        rotate_world("spine_01", right, -reach_lean);
    for (auto side : {std::string("l"), std::string("r")}) {

        float sign = side == "l" ? -1.f : 1.f;
        const unsigned boot_index = side == "l" ? 0 : 1;
        const auto& boot = boots_[boot_index];
        wardrobe_.local_transform(boot.node, world("foot_" + side) * boot.attachment);
        if (support <= 0)
            continue;
        auto shoulder = position("upperarm_" + side);
        auto rest_hand = authored_hands[boot_index];
        auto held_hand = point(carry.wrists[boot_index]) + grip - held;
        auto hand = rest_hand * (1 - support) + held_hand * support;
        reach("upperarm_" + side, "lowerarm_" + side, "hand_" + side, hand,
              shoulder + right * sign * .30f + sengine::float3{0, -.45f, 0} - forward * .05f);
        orient_wrist(side, sign, support, forward, right);
        curl_fingers(side, support);
    }
    body_.synchronize();
}
}
