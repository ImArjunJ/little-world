#pragma once
#include "greenhouse_controls.hpp"
#include "sengine/native_canvas.hpp"
#include "ui.hpp"
namespace terrarium {
class native_frontend {
  public:
    native_frontend(greenhouse&, greenhouse_controls&, sengine::explorer&, std::filesystem::path profile);
    sengine::native_canvas canvas;
    user_interface ui;
    void start();
    void event(const sengine::input_event&, bool captured);
    void draw(sengine::native_hud&, int width, int height, float dpi, double delta, sengine::hud_input,
              bool focused);
    bool walking() const { return ui.screen() == screen_kind::greenhouse && game_.can_walk(); }
    bool living() const {
        return ui.screen() == screen_kind::greenhouse || ui.screen() == screen_kind::habitat;
    }
    bool typing() const { return ui.typing(); }
    void update_editor_input(double seconds);
    bool save_preferences();
    std::optional<tending_preview> preview() const;
    bool quit{};
    std::filesystem::path photograph;

  private:
    greenhouse& game_;
    greenhouse_controls& controls_;
    sengine::explorer& explorer_;
    std::filesystem::path root_;
    std::string editor_id_, preferences_written_;
    double preferences_delay_{};
    bool initial_setup_{};
    void load_preferences();
    std::string preferences() const;
    void requests();
    void return_to_greenhouse();
    void tend(world_state&, double);
    void draw_preview(sengine::native_hud&);
    void draw_environment(const world_state&, sengine::native_hud&);
    entity_id pick_creature(const world_state&, sengine::drawing::point2) const;
    entity_id pick_plant(const world_state&, sengine::drawing::point2) const;
    sengine::drawing::point2 screen_point(vec2, float elevation = 0) const;
};
}
