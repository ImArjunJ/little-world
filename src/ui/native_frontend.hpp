#pragma once
#include "greenhouse_controls.hpp"
#include "sengine/native_canvas.hpp"
#include "ui.hpp"
namespace terrarium {
struct frontend_settings {
    audio_settings audio;
    int performance;
    bool reduced_motion, ambient_occlusion, lens_blur;
};
class native_frontend {
  public:
    native_frontend(sengine::window&, sengine::native_hud&, greenhouse&, greenhouse_controls&,
                    terrarium::explorer&, std::filesystem::path profile);
    void begin_events();
    void apply_window_preferences();
    frontend_settings settings() const;
    bool quit_requested() const { return quit_; }
    const std::filesystem::path& photograph() const { return photograph_; }
    void finish_photograph(std::string message);
    void notify(std::string message);
    void toggle_reduced_motion();
    void event(const sengine::input_event&, bool captured);
    void draw(int width, int height, float dpi, double delta, sengine::hud_input, bool focused);
    bool walking() const { return ui_.screen() == screen_kind::greenhouse && game_.can_walk(); }
    bool living() const {
        return ui_.screen() == screen_kind::greenhouse || ui_.screen() == screen_kind::habitat;
    }
    bool typing() const { return ui_.typing(); }
    bool save_preferences();
    std::optional<tending_preview> preview() const;

  private:
    void start();
    void update_editor_input(double seconds);
    void load_preferences();
    std::string preferences() const;
    void requests();
    void return_to_greenhouse();
    bool tending_shortcuts(world_state& world);
    void tending_camera(world_state& world, double dt);
    void tending_pointer(world_state& world, double dt);
    void garden_request(ui_request action);
    void tend(world_state&, double);
    void draw_preview(sengine::native_hud&);
    void draw_environment(const world_state&, sengine::native_hud&);
    entity_id pick_creature(const world_state&, sengine::drawing::point2) const;
    entity_id pick_plant(const world_state&, sengine::drawing::point2) const;
    sengine::drawing::point2 screen_point(vec2, float elevation = 0) const;

  private:
    sengine::window& display_;
    sengine::native_hud& hud_;
    sengine::native_canvas canvas_;
    user_interface ui_;
    bool quit_{}, fullscreen_{}, text_input_{};
    std::filesystem::path photograph_;
    greenhouse& game_;
    greenhouse_controls& controls_;
    terrarium::explorer& explorer_;
    std::filesystem::path root_;
    std::string editor_id_, preferences_written_;
    double preferences_delay_{};
    bool initial_setup_{};
};
}
