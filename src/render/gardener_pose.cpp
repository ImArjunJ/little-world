#include "garden_scene_state.hpp"
namespace terrarium::render {
using namespace sengine;
namespace {
class character_rig {
  public:
    explicit character_rig(garden_scene_state& state) : state(state), scene(state.resources) {}
    sengine::scene_node entity(const std::string& name) const { return state.joints.at(name).entity; }
    sengine::mat4 world(const std::string& name) const { return world_transform(scene, entity(name)); }
    sengine::float3 position(const std::string& name) const { return world(name)[3].xyz(); }
    void rotate_world(const std::string& name, sengine::float3 axis, float radians) {
        auto inst = entity(name);
        auto w = world_transform(scene, inst);
        auto pos = w[3].xyz();
        auto parent = sengine::parent(scene, inst);
        auto pw = parent ? world_transform(scene, parent) : sengine::mat4{};
        set_transform(scene, inst,
                      inverse(pw) * sengine::translation(pos) * sengine::rotation(radians, axis) *
                          sengine::translation(-pos) * w);
    }
    void aim(const std::string& name, const std::string& child, sengine::float3 target) {
        auto a = position(name);
        auto from = normalize(position(child) - a), to = normalize(target - a);
        auto axis = cross(from, to);
        float l = length(axis);
        if (l > .00001f)
            rotate_world(name, axis / l, std::acos(std::clamp(dot(from, to), -1.f, 1.f)));
    }
    void reach(const std::string& upper, const std::string& lower, const std::string& end,
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

  private:
    garden_scene_state& state;
    sengine::scene& scene;
};
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
void animate_locomotion(garden_scene_state& state, const terrarium::camera_pose& player, double seconds) {
    auto& scene = state.resources;
    for (auto& [name, joint] : state.joints)
        set_transform(scene, joint.entity, joint.bind);
    auto selected = state.clips.at(locomotion_clip(player));
    if (!state.clip_started || selected != state.active_clip) {
        state.previous_clip = state.active_clip;
        state.previous_clip_time = state.clip_time;
        state.active_clip = selected;
        state.blend_time = state.clip_started ? 0.f : 1.f;
        state.clip_started = true;
        state.clip_time = 0;
    }
    const float duration = animation_duration(scene, state.character, state.active_clip);
    state.clip_time = player.grounded && player.speed > .15f
                          ? std::fmod(player.stride / (2 * 3.14159265f), 1.f) * duration
                          : std::fmod(state.clip_time + float(seconds), duration);
    animate(scene, state.character, state.active_clip, state.clip_time);
    state.blend_time += float(seconds);
    if (state.blend_time < .18f)
        blend_animation(scene, state.character, state.previous_clip, state.previous_clip_time,
                        ease(state.blend_time / .18f));
}
sengine::float3 place_body(garden_scene_state& state, const character_rig& rig,
                           const terrarium::camera_pose& player) {
    auto& scene = state.resources;
    auto body_root = root(scene, state.character);
    auto body_position = point(player.feet);
    const auto body_rotation = sengine::rotation(3.14159265f - player.yaw, sengine::float3{0, 1, 0});
    set_transform(scene, body_root, sengine::translation(body_position) * body_rotation);
    if (player.grounded) {
        const float lowest_ankle = std::min(rig.position("foot_l").y, rig.position("foot_r").y);
        body_position.y += player.feet.y + .07f - lowest_ankle;
        set_transform(scene, body_root, sengine::translation(body_position) * body_rotation);
    }

    return body_position;
}
void relax_arms(garden_scene_state& state, float support) {
    auto& scene = state.resources;
    if (support > 0) {
        for (auto& [name, joint] : state.joints) {
            if (!(name.starts_with("upperarm_") || name.starts_with("lowerarm_") ||
                  name.starts_with("hand_") || name.starts_with("index_") || name.starts_with("middle_") ||
                  name.starts_with("ring_") || name.starts_with("pinky_") || name.starts_with("thumb_")))
                continue;
            auto instance = joint.entity;
            auto local = local_transform(scene, instance);
            auto rotation = slerp(rotation_of(local), rotation_of(joint.bind), support);
            auto blended = sengine::rotation(rotation);
            blended[3] = local[3];
            set_transform(scene, instance, blended);
        }
    }
}
void orient_wrist(character_rig& rig, const std::string& side, float sign, float support,
                  sengine::float3 forward, sengine::float3 right) {
    auto toward = support > .01f ? forward * .06f - right * sign * .09f : sengine::float3{0, -.12f, 0};
    rig.aim("hand_" + side, "middle_01_" + side, rig.position("hand_" + side) + toward);
    const auto wrist = rig.position("hand_" + side);
    const auto finger_axis = normalize(rig.position("middle_01_" + side) - wrist);
    auto palm =
        normalize(cross(rig.position("index_01_" + side) - wrist, rig.position("pinky_01_" + side) - wrist)) *
        sign;
    palm = normalize(palm - finger_axis * dot(palm, finger_axis));
    auto up = normalize(sengine::float3{0, 1, 0} - finger_axis * finger_axis.y);
    const float roll = std::atan2(dot(finger_axis, cross(palm, up)), dot(palm, up));
    rig.rotate_world("hand_" + side, finger_axis, roll * support);
}
void curl_fingers(sengine::scene& scene, const character_rig& rig, const std::string& side, float support) {
    for (auto f : {"index", "middle", "ring", "pinky"})
        for (auto segment : {"01", "02", "03"}) {
            std::string name = std::string(f) + "_" + segment + "_" + side;
            auto inst = rig.entity(name);
            auto local = local_transform(scene, inst);
            set_transform(scene, inst, local * sengine::rotation(support * .08f, sengine::float3{1, 0, 0}));
        }
}
}
void animate_gardener(garden_scene_state& state, const terrarium::camera_pose& player, double seconds,
                      const carrying_pose& holding) {
    auto& scene = state.resources;
    character_rig rig(state);
    animate_locomotion(state, player, seconds);
    const auto body_position = place_body(state, rig, player);
    const std::array authored_hands{rig.position("hand_l"), rig.position("hand_r")};
    const auto& carry = holding.pose;
    const auto held = holding.held, grip = holding.grip;
    const float hold = holding.blend, support = holding.support;
    const sengine::float3 forward{std::sin(player.yaw), 0, -std::cos(player.yaw)},
        right{std::cos(player.yaw), 0, std::sin(player.yaw)};
    relax_arms(state, support);
    float reach_lean =
        (std::clamp((dot(grip - body_position, forward) - .68f) * 1.8f, 0.f, .6f) * (1 - hold) +
         .08f * hold) *
        support;
    if (reach_lean > 0)
        rig.rotate_world("spine_01", right, -reach_lean);
    for (auto side : {std::string("l"), std::string("r")}) {

        float sign = side == "l" ? -1.f : 1.f;
        const unsigned boot_index = side == "l" ? 0 : 1;
        auto& boot = state.boots[boot_index];
        set_transform(scene, boot.entity, rig.world("foot_" + side) * state.boot_attachments[boot_index]);
        if (support <= 0)
            continue;
        auto shoulder = rig.position("upperarm_" + side);
        auto rest_hand = authored_hands[boot_index];
        auto held_hand = point(carry.wrists[boot_index]) + grip - held;
        auto hand = rest_hand * (1 - support) + held_hand * support;
        rig.reach("upperarm_" + side, "lowerarm_" + side, "hand_" + side, hand,
                  shoulder + right * sign * .30f + sengine::float3{0, -.45f, 0} - forward * .05f);
        orient_wrist(rig, side, sign, support, forward, right);
        curl_fingers(scene, rig, side, support);
    }
    update_bones(scene, state.character);
}
}
