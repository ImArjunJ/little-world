#pragma once
#include "footstep_audio.hpp"
#include "greenhouse_controls.hpp"
#include "native_audio.hpp"
#include "native_frontend.hpp"
#include "render/garden_scene.hpp"
#include "sengine/application.hpp"
#include "sengine/pointer_capture.hpp"

namespace terrarium {
struct frame_input {
    bool jump_pressed{};
    sengine::hud_input pointer;
};

class greenhouse_session final : public sengine::runtime_scene {
  public:
    explicit greenhouse_session(sengine::application&);
    void save();
    void begin_frame() override;
    bool event(const sengine::input_event&) override;
    bool close_requested() override;
    void update(const sengine::runtime_frame&) override;
    void render(const sengine::runtime_frame&) override;

  private:
    void sync_pointer();
    void window_event(const sengine::input_event&);
    void walking_shortcut(frame_input&, const sengine::input_event&);
    void look_around(const sengine::input_event&);
    void walk(bool jump, double elapsed, bool focused);
    void play_footsteps();
    void advance_world(const frame_input&, double elapsed, bool focused);
    void apply_window_preferences(bool focused);
    void draw_interface(frame_input, const sengine::window_metrics&, double elapsed, bool focused);
    void update_ambience(bool focused);
    terrarium::camera_pose view_camera(const terrarium::camera_pose&, float aspect);
    void configure_view(const terrarium::camera_pose&);
    void present_frame(const terrarium::camera_pose&, const sengine::window_metrics&);
    void draw_frame(frame_input, const sengine::window_metrics&, double elapsed, bool focused);

  private:
    sengine::application& host_;
    sengine::window& display;
    std::filesystem::path data{sengine::window::executable_directory() / "data"};
    render::garden_scene renderer{display, data / "scene/greenhouse.gltf", render::render_quality::high};
    terrarium::landscape landscape{terrarium::load_landscape(data / "scene/landscape.bin")};
    terrarium::explorer explorer{landscape, {2.95f, 0, 2.6f}, -.48f, -.17f};
    footstep_audio footsteps{true, data / "audio/footsteps"};
    std::filesystem::path profile{sengine::user_data_directory("little-world")};
    greenhouse game{profile};
    greenhouse_controls controls{game, explorer, landscape};
    native_frontend frontend{display, renderer.hud(), game, controls, explorer, profile};
    native_audio ambience{true};
    sengine::pointer_capture pointer;
    frame_input frame_input_;
    bool focused_{};
};
}
