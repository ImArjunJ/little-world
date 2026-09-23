#pragma once
#include "player_controller.hpp"
#include "save_library.hpp"
#include <optional>
#include <simulates/clock.hpp>

namespace terrarium {
enum class greenhouse_mode { explore, lifting, carry, lowering, enter_editor, editor, leave_editor, journal };
struct greenhouse_garden {
    std::string id, name;
    world_state world{1, false};
    simulates::clock clock{world_state::fixed_step};
    int place{-1};
    float rotation{};
    bool dirty{};
};
struct garden_spot {
    sengine::point position;
    std::string name;
    float exposure;
};
class greenhouse {
  public:
    explicit greenhouse(std::filesystem::path root);
    void advance(double seconds, bool simulate = true);
    bool create(const std::string& name, garden_design design = {}, starter starter = starter::woodland);
    bool rename(const std::string& id, const std::string& name);
    bool remove(const std::string& id);
    bool save();
    bool duplicate(const std::string& id);
    world_state* editing_world();
    std::filesystem::path active_path() const {
        return selected_.empty() ? std::filesystem::path{} : library_.path(selected_);
    }
    void reset_editor_clock();
    bool lift(const std::string& id);
    bool place(int index);
    bool edit(const std::string& id);
    bool leave();
    bool open_journal();
    bool track(const std::string& id);
    bool rotate(float radians);
    bool rain();
    bool plant(plant_kind kind, vec2 point);
    bool water(vec2 point);
    bool introduce(species_kind species, vec2 point);
    bool climate(float temperature, float sunlight, float vent);
    bool set_speed(double value);
    greenhouse_mode mode() const { return mode_; }
    double speed() const { return speed_; }
    int destination() const { return destination_; }
    float progress() const { return progress_; }
    const std::string& selected() const { return selected_; }
    const std::string& tracked() const { return tracked_; }
    const std::string& error() const { return error_; }
    const std::vector<greenhouse_garden>& gardens() const { return gardens_; }
    const greenhouse_garden* find(const std::string& id) const;
    const greenhouse_garden* at(int place) const;
    const greenhouse_garden* active() const { return find(selected_); }
    bool holding() const;
    float carry_blend() const;
    bool transition() const;
    bool can_walk() const { return mode_ == greenhouse_mode::explore || mode_ == greenhouse_mode::carry; }
    bool carry_clear(const terrarium::camera_pose&, const terrarium::landscape&) const;
    terrarium::explorer_input locomotion(terrarium::explorer_input input) const;
    int target(const terrarium::camera_pose&, const terrarium::landscape&, bool empty) const;
    static const std::vector<garden_spot>& spots();
    static const char* location(int place);

  private:
    greenhouse_garden* mutable_active();
    void begin(greenhouse_mode mode);
    void load_layout();

  private:
    save_library library_;
    std::vector<greenhouse_garden> gardens_;
    greenhouse_mode mode_{greenhouse_mode::explore};
    std::string selected_, tracked_, error_;
    double speed_{1}, autosave_{};
    float progress_{};
    int destination_{-1};
};
}
