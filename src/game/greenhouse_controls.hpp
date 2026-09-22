#pragma once
#include "greenhouse.hpp"
#include "sengine/camera_path.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <utility>
namespace terrarium {
class greenhouse_controls {
  public:
    greenhouse_controls(greenhouse& game, sengine::explorer& explorer, const sengine::landscape& land)
        : game_(game), explorer_(explorer), land_(land) {}
    void event(const SDL_Event&, bool captured);
    void advance(double seconds, bool reduced_motion = false);
    void pan(float right, float forward);
    sengine::camera_pose camera() const;
    void viewport(float aspect) { aspect_ = aspect; }
    std::optional<vec2> soil_point(float x, float y, float width, float height) const;
    sengine::point world_point(vec2, float above_soil = 0) const;
    void focus(vec2 point, bool close = true) {
        target_focus_ = point;
        if (close)
            target_zoom_ = .4f;
    }
    void reset_camera() {
        target_focus_ = {};
        target_zoom_ = 1.25f;
        elevation_ = .48f;
    }
    void orbit(float angle, float elevation, float zoom) {
        orbit_ = angle;
        elevation_ = std::clamp(elevation, .15f, 1.52f);
        zoom_ = target_zoom_ = std::clamp(zoom, .16f, 2.4f);
    }
    void rotate_camera(float radians) { orbit_ += radians; }
    void enter_editor(const std::string&);
    void open_journal();
    std::string take_notice() { return std::exchange(notice_, {}); }
    const sengine::landscape& landscape() const { return land_; }

  private:
    greenhouse& game_;
    sengine::explorer& explorer_;
    const sengine::landscape& land_;
    sengine::camera_pose return_camera_{};
    vec2 focus_{}, target_focus_{};
    float orbit_{}, elevation_{.48f}, zoom_{1.25f};
    float target_zoom_{1.25f};
    std::string notice_;
    float aspect_{1.6f};
    mutable sengine::camera_path travel_;
    mutable std::optional<sengine::point> travel_eye_;
    mutable float travel_padding_{};
    float camera_padding() const;
    sengine::camera_pose editor_camera() const;
};
}
