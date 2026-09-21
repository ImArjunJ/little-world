#pragma once
#include "greenhouse.hpp"
#include "sengine/explorer.hpp"
#include "sengine/native_hud.hpp"
#include "tending_preview.hpp"
#include <filesystem>
#include <memory>

namespace terrarium::render {
enum class render_quality { low, high };

class filament_scene {
  public:
    filament_scene(void* native_window, const std::filesystem::path& scene, render_quality quality,
                   void* shared_context = nullptr);
    ~filament_scene();
    filament_scene(const filament_scene&) = delete;
    filament_scene& operator=(const filament_scene&) = delete;
    sengine::native_hud& hud();
    void configure(int detail, bool occlusion, bool soft_focus, bool inspecting, float focus_distance);
    void present(const greenhouse&, const sengine::camera_pose& player, double seconds,
                 std::optional<tending_preview> preview = {});
    bool frame(const sengine::camera_pose& camera, unsigned width, unsigned height,
               const std::filesystem::path& capture = {}, bool capture_interface = true);

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
}
