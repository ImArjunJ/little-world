#pragma once
#include "ambient_mixer.hpp"
#include "ecosystem.hpp"
#include "greenhouse_controls.hpp"
#include "save_library.hpp"
#include "sengine/drawing.hpp"
#include "tending_preview.hpp"
#include <array>
#include <filesystem>
#include <string>
namespace terrarium {
class explorer;
struct landscape;
}
namespace terrarium {
class greenhouse;
enum class screen_kind {
    habitat,
    greenhouse,
    home,
    gardens,
    new_garden,
    construction,
    delete_garden,
    options,
    setup,
    welcome,
    photo
};
enum class ui_request {
    none,
    save,
    restore,
    continue_game,
    greenhouse,
    open,
    create,
    rename,
    duplicate,
    delete_garden,
    home,
    quit,
    finish_setup,
    finish_welcome,
    photo,
    export_photo,
    finish_photo
};
enum class panel_kind { none, weather, creature_state, plant_state, journal, soil_cell };
enum class ui_control {
    pause,
    weather,
    soil_cell,
    journal,
    return_to_world,
    overview,
    observe,
    photograph,
    follow,
    favourite,
    name,
    save,
    load
};
class user_interface {
  public:
    user_interface();
    ~user_interface() = default;
    void draw(world_state& world, greenhouse_controls& camera, double backlog);
    void sync_scale();
    float scale() const { return scale_; }
    sengine::drawing::point2 to_screen(sengine::drawing::point2 p) const {
        return {p.x * scale_, (p.y + entrance_) * scale_};
    }
    void show(screen_kind screen);
    screen_kind screen() const { return screen_; }
    screen_kind setup_origin() const { return setup_from_; }
    screen_kind welcome_origin() const { return welcome_from_; }
    void draw_photo();
    void draw_greenhouse(greenhouse&, const terrarium::explorer&, const terrarium::landscape&);
    void draw_greenhouse_journal(greenhouse&);
    void new_garden() {
        renaming_ = false;
        draft_design = {};
        draft_starter = starter::woodland;
        construction_step_ = 0;
        draft_name = "A pocket of green";
        show(screen_kind::new_garden);
    }
    void draw_front(const std::vector<garden_entry>& entries);
    bool over_garden(sengine::drawing::point2 point) const;
    sengine::drawing::rect tool_bounds(garden_tool tool) const;
    sengine::drawing::rect control_bounds(ui_control control) const;
    void reveal_plant(entity_id id) {
        selected_plant = id;
        selected = followed = 0;
        panel_ = panel_kind::plant_state;
        scroll_ = 0;
        editing_ = false;
    }
    void reveal_family() {
        selected_plant = 0;
        panel_ = panel_kind::creature_state;
        scroll_ = 0;
    }
    void close_panel() {
        panel_ = panel_kind::none;
        editing_ = false;
    }
    void toast(std::string message);
    bool typing() const { return editing_ || screen_ == screen_kind::new_garden; }
    panel_kind panel() const { return panel_; }

  public:
    garden_design draft_design;
    starter draft_starter{starter::woodland};

    ui_request request{ui_request::none};
    std::string request_id, garden_name, draft_name{"A pocket of green"};
    float interface_size{1};
    bool automatic_dpi{true}, onboarding_seen{}, ambient_occlusion{true}, lens_blur{};
    bool setup_seen{}, borderless{};
    int performance{1};

    garden_tool tool{garden_tool::inspect};
    entity_id selected{}, followed{}, selected_plant{};
    int environment_view{};
    double speed{1};
    bool paused{}, reduced_motion{}, observation{};
    audio_settings audio;

  private:
    class panel_text {
      public:
        panel_text(user_interface& ui, bool measure_only) : ui_(ui), measure_only_(measure_only) {}
        void text(const std::string&, float x, float y, float size, sengine::drawing::color,
                  bool display = false) const;
        float wrapped(const std::string&, float x, float y, float width, float size,
                      sengine::drawing::color) const;

      private:
        user_interface& ui_;
        bool measure_only_;
    };

  private:
    float hover_ease(int id, bool active);
    float width() const;
    float height() const;
    float footer_y() const;
    float time_y() const;
    sengine::drawing::point2 mouse() const;
    sengine::drawing::rect scaled(sengine::drawing::rect r) const;
    void begin_canvas();
    void illustration(sengine::drawing::point2 center, float size, int stage, float time);
    void garden_summary(const greenhouse_garden& garden, float dx, float dy, float dw, bool narrow);
    void draw_home();
    void draw_gardens(sengine::drawing::rect page, float t, const std::vector<garden_entry>& entries);
    void draw_delete_garden(sengine::drawing::rect page);
    void draw_new_garden(sengine::drawing::rect page, float t);
    void draw_welcome(sengine::drawing::rect page, float t);
    void draw_options(sengine::drawing::rect page);
    void front_navigation(screen_kind, sengine::drawing::rect);
    void habitat_heading(world_state& world);
    void panel_button(ui_control, panel_kind, int symbol, const char* hint);
    void panel_controls();
    void tool_controls();
    void time_controls();
    void view_controls(greenhouse_controls& camera, double backlog);
    void active_panel(world_state& world, greenhouse_controls& camera);
    void habitat_status();
    void creature_name_field(world_state& world, const ancestor_record* ancestor, float x, float y, float w);
    void creature_status(world_state& world, greenhouse_controls& camera, const ancestor_record* ancestor,
                         const creature_state* live, float x, float y, float w);
    void creature_family(world_state& world, const ancestor_record* ancestor, float x, float y, float w);
    void vessel_preview(float x, float y, float time);
    void vessel_dimensions(float x, float y);
    void population_plots(const world_state& world, float x, float row, float w, int columns);
    void preferences(sengine::drawing::rect r);
    void setup(sengine::drawing::rect page, float time);
    void construction(sengine::drawing::rect page, float time);
    void name_input(sengine::drawing::rect r);
    void text(const std::string& value, float x, float y, float size, sengine::drawing::color color,
              bool display = false, bool shadow = false);
    float wrapped(const std::string& value, float x, float y, float width, float size,
                  sengine::drawing::color color, bool measure_only = false);
    bool button(sengine::drawing::rect rectangle, const std::string& text, bool active = false);
    bool medallion(sengine::drawing::rect rectangle, int icon, const std::string& hint, bool active = false);
    void medallion_face(sengine::drawing::rect rectangle, int icon, bool active = false, float glow = 0);
    void icon(int icon, sengine::drawing::point2 position, float size, sengine::drawing::color color);
    void paper(sengine::drawing::rect rectangle);
    void slider(sengine::drawing::rect rectangle, const std::string& label, float& value, float low,
                float high, const std::string& unit);
    sengine::drawing::rect panel_bounds() const;
    void plant_panel(world_state& world, greenhouse_controls& camera, sengine::drawing::rect rectangle);
    void creature_panel(world_state& world, greenhouse_controls& camera, sengine::drawing::rect rectangle);
    void weather_panel(world_state& world, sengine::drawing::rect rectangle);
    void soil_panel(world_state& world, sengine::drawing::rect rectangle);
    float matter_journal(const world_state& world, float x, float y, float width, bool measure_only = false);
    float journal_panel(const world_state& world, sengine::drawing::rect rectangle,
                        bool measure_only = false);

  private:
    screen_kind screen_{screen_kind::habitat};
    screen_kind gardens_from_{screen_kind::home}, draft_from_{screen_kind::gardens},
        setup_from_{screen_kind::home}, welcome_from_{screen_kind::greenhouse};
    float scale_{1};
    int shelf_page_{}, welcome_step_{}, setup_step_{}, construction_step_{};
    float entrance_{};
    std::array<float, 256> hover_{};

    double screen_since_{};
    bool renaming_{}, options_from_greenhouse_{};
    std::string delete_name_, greenhouse_selection_;

    sengine::hud_image paper_image_;
    sengine::drawing::font body_{}, display_{};
    panel_kind panel_{panel_kind::none};
    std::string notification_, name_buffer_;
    double notify_until_{};
    int focused_{-1}, widget_{}, widget_count_{}, active_slider_{-1}, child_page_{};
    bool keyboard_focus_{}, editing_{};
    float scroll_{};
    entity_id previous_selected_{};
    sengine::drawing::rect clip_{};
    bool clipped_{};
};
}
