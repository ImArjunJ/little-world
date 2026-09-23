#pragma once
#include "greenhouse.hpp"
#include "player_controller.hpp"
#include "sengine/native_hud.hpp"
#include "sengine/window.hpp"
#include "tending_preview.hpp"
#include <filesystem>
#include <memory>

namespace terrarium::render {
struct garden_scene_state;
enum class render_quality { low, high };

class garden_scene {
  public:
    garden_scene(sengine::window& window, const std::filesystem::path& scene, render_quality quality);
    ~garden_scene();
    garden_scene(const garden_scene&) = delete;
    garden_scene& operator=(const garden_scene&) = delete;
    sengine::native_hud& hud();
    void configure(int detail, bool occlusion, bool soft_focus, bool inspecting, float focus_distance);
    void present(const greenhouse&, const terrarium::camera_pose& player, double seconds,
                 std::optional<tending_preview> preview = {});
    bool frame(const terrarium::camera_pose& camera, unsigned width, unsigned height,
               const std::filesystem::path& capture = {}, bool capture_interface = true);

  private:
    std::unique_ptr<garden_scene_state> state_;
};
}
