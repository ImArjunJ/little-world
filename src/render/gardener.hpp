#pragma once
#include "player_controller.hpp"
#include "sengine/model.hpp"
#include <map>

namespace terrarium::render {
struct carrying_pose;
class gardener {
  public:
    gardener(sengine::scene&, const std::filesystem::path&, sengine::model_instance& wardrobe,
             const std::array<sengine::model_node, 2>& boots);
    void animate(const terrarium::camera_pose&, double seconds, const carrying_pose&);

  private:
    struct joint {
        sengine::model_node node;
        sengine::mat4 bind;
    };
    struct boot {
        sengine::model_node node;
        sengine::mat4 attachment;
    };
    sengine::model_node node(const std::string&) const;
    sengine::mat4 world(const std::string&) const;
    sengine::float3 position(const std::string&) const;
    void rotate_world(const std::string&, sengine::float3 axis, float radians);
    void aim(const std::string&, const std::string& child, sengine::float3 target);
    void reach(const std::string& upper, const std::string& lower, const std::string& end,
               sengine::float3 target, sengine::float3 pole);
    void locomotion(const terrarium::camera_pose&, double seconds);
    sengine::float3 place_body(const terrarium::camera_pose&);
    void relax_arms(float support);
    void orient_wrist(const std::string& side, float sign, float support, sengine::float3 forward,
                      sengine::float3 right);
    void curl_fingers(const std::string& side, float support);

  private:
    sengine::model_instance body_;
    sengine::model_instance& wardrobe_;
    std::map<std::string, joint> joints_;
    std::map<std::string, std::size_t> clips_;
    std::array<boot, 2> boots_;
    std::optional<std::size_t> active_clip_;
    std::size_t previous_clip_{};
    float clip_time_{}, previous_clip_time_{}, blend_time_{1};
};
}
