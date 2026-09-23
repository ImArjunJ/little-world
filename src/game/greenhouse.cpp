#include "greenhouse.hpp"
#include "carry_pose.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>

namespace terrarium {
const std::vector<garden_spot>& greenhouse::spots() {
    static const std::vector<garden_spot> value{
        {{-.04f, .980f, -.02f}, "Workbench / left", .65f},
        {{.79f, .980f, -.02f}, "Workbench / middle", .65f},
        {{1.62f, .980f, -.02f}, "Workbench / right", .65f},
        {{-2.8f, .974f, -3.06f}, "Sun bench / left", 1},
        {{-1.87f, .974f, -3.06f}, "Sun bench / middle", 1},
        {{-.94f, .974f, -3.06f}, "Sun bench / right", 1},
        {{-4.f, 1.234f, 1.99f}, "Observatory shelf / lower left", .35f},
        {{-4.f, 1.234f, 2.81f}, "Observatory shelf / lower right", .35f},
        {{-4.f, 1.964f, 1.99f}, "Observatory shelf / upper left", .5f},
        {{-4.f, 1.964f, 2.81f}, "Observatory shelf / upper right", .5f}};
    return value;
}
const char* greenhouse::location(int p) {
    return p >= 0 && p < int(spots().size()) ? spots()[p].name.c_str() : "In storage";
}
greenhouse::greenhouse(std::filesystem::path root) : library_(std::move(root)) {
    library_.import_legacy(error_);
    for (const auto& entry : library_.list()) {
        greenhouse_garden g;
        g.id = entry.id;
        g.name = entry.name;
        std::string failure;
        if (library_.load(g.id, g.world, failure))
            gardens_.push_back(std::move(g));
        else
            error_ = "Could not load " + entry.name + ": " + failure;
    }
    load_layout();
}
void greenhouse::load_layout() {
    std::ifstream in(library_.root() / "greenhouse.layout");
    std::string magic, id;
    int version, p;
    float angle;
    std::set<int> used;
    if (in >> magic >> version && magic == "LITTLE_WORLD_GREENHOUSE" && version == 1)
        for (unsigned n = 0; n < 10000 && in >> std::quoted(id) >> p >> angle; ++n)
            for (auto& g : gardens_)
                if (g.id == id && p >= 0 && p < int(spots().size()) && std::isfinite(angle) &&
                    !used.contains(p)) {
                    g.place = p;
                    g.rotation = angle;
                    used.insert(p);
                    break;
                }
    for (auto& g : gardens_) {
        if (g.place < 0)
            for (int i = 0; i < int(spots().size()); ++i)
                if (!used.contains(i)) {
                    g.place = i;
                    used.insert(i);
                    break;
                }
        if (g.place >= 0)
            g.world.climate.room_exposure = spots()[g.place].exposure;
    }
}
const greenhouse_garden* greenhouse::find(const std::string& id) const {
    for (const auto& g : gardens_)
        if (g.id == id)
            return &g;
    return nullptr;
}
const greenhouse_garden* greenhouse::at(int place) const {
    if (place < 0)
        return nullptr;
    for (const auto& g : gardens_)
        if (g.place == place)
            return &g;
    return nullptr;
}
greenhouse_garden* greenhouse::mutable_active() {
    for (auto& g : gardens_)
        if (g.id == selected_)
            return &g;
    return nullptr;
}
bool greenhouse::holding() const {
    return mode_ == greenhouse_mode::carry || mode_ == greenhouse_mode::lifting ||
           mode_ == greenhouse_mode::lowering;
}
bool greenhouse::transition() const {
    return mode_ == greenhouse_mode::lifting || mode_ == greenhouse_mode::lowering ||
           mode_ == greenhouse_mode::enter_editor || mode_ == greenhouse_mode::leave_editor;
}
void greenhouse::begin(greenhouse_mode mode) {
    mode_ = mode;
    progress_ = 0;
}
void greenhouse::advance(double seconds, bool simulate) {
    if (!std::isfinite(seconds) || seconds < 0)
        return;
    if (transition()) {
        progress_ = std::min(1.f, progress_ + float(seconds) / .65f);
        if (progress_ >= 1)
            switch (mode_) {
            case greenhouse_mode::lifting:
                mode_ = greenhouse_mode::carry;
                break;
            case greenhouse_mode::lowering:
                if (auto* g = mutable_active()) {
                    g->place = destination_;
                    g->world.climate.room_exposure = spots()[destination_].exposure;
                    g->dirty = true;
                }
                mode_ = greenhouse_mode::explore;
                selected_.clear();
                save();
                break;
            case greenhouse_mode::enter_editor:
                mode_ = greenhouse_mode::editor;
                break;
            case greenhouse_mode::leave_editor:
                mode_ = greenhouse_mode::explore;
                selected_.clear();
                save();
                break;
            default:
                break;
            }
    }
    if (!simulate)
        return;
    int budget = std::max(8, 512 / int(std::max<size_t>(1, gardens_.size())));
    for (auto& g : gardens_) {
        int ticks = g.clock.accrue(seconds, speed_, budget);
        if (ticks) {
            g.world.advance(ticks);
            g.dirty = true;
        }
    }
    autosave_ += seconds;
    if (autosave_ >= 30) {
        save();
        autosave_ = 0;
    }
}
bool greenhouse::create(const std::string& name, garden_design design, starter starter) {
    if (mode_ != greenhouse_mode::journal)
        return false;
    int p = 0;
    for (; p < int(spots().size()) && at(p); ++p) {
    }
    if (p == int(spots().size())) {
        error_ = "Every placement is occupied. Free a spot before creating another garden.";
        return false;
    }
    greenhouse_garden g;
    g.world = world_state(uint32_t(gardens_.size() * 7919 + 1402), false);
    if (!g.world.construct(design)) {
        error_ = "This vessel design is invalid.";
        return false;
    }
    plant_starter(g.world, starter);
    g.name = name;
    g.place = p;
    g.world.climate.room_exposure = spots()[p].exposure;
    if (!library_.create(name, g.world, g.id, error_))
        return false;
    gardens_.push_back(std::move(g));
    return save();
}
bool greenhouse::rename(const std::string& id, const std::string& name) {
    if (mode_ != greenhouse_mode::journal)
        return false;
    for (auto& g : gardens_)
        if (g.id == id) {
            if (!library_.rename(id, name, error_))
                return false;
            g.name = name;
            return true;
        }
    return false;
}
bool greenhouse::remove(const std::string& id) {
    if (mode_ != greenhouse_mode::journal || !find(id))
        return false;
    if (!library_.remove(id, error_))
        return false;
    std::erase_if(gardens_, [id](const auto& g) { return g.id == id; });
    if (tracked_ == id)
        tracked_.clear();
    return save();
}
world_state* greenhouse::editing_world() {
    if (mode_ != greenhouse_mode::editor)
        return nullptr;
    auto* g = mutable_active();
    if (!g)
        return nullptr;
    g->dirty = true;
    return &g->world;
}
void greenhouse::reset_editor_clock() {
    if (mode_ == greenhouse_mode::editor)
        if (auto* g = mutable_active())
            g->clock.reset();
}
bool greenhouse::duplicate(const std::string& id) {
    if (mode_ != greenhouse_mode::journal)
        return false;
    const auto* original = find(id);
    if (!original)
        return false;
    greenhouse_garden copy;
    copy.world = original->world;
    copy.name = original->name + " / a new beginning";
    for (int i = 0; i < int(spots().size()); ++i)
        if (!at(i)) {
            copy.place = i;
            break;
        }
    if (copy.place >= 0)
        copy.world.climate.room_exposure = spots()[copy.place].exposure;
    if (!library_.create(copy.name, copy.world, copy.id, error_))
        return false;
    gardens_.push_back(std::move(copy));
    return save();
}
bool greenhouse::save() {
    try {
        for (auto& g : gardens_)
            if (g.dirty) {
                if (!library_.save(g.id, g.name, g.world, error_))
                    return false;
                g.dirty = false;
            }
        auto temp = library_.root() / "greenhouse.layout.tmp", dest = library_.root() / "greenhouse.layout";
        std::ofstream out(temp);
        out << "LITTLE_WORLD_GREENHOUSE 1\n" << std::setprecision(9);
        for (auto& g : gardens_)
            out << std::quoted(g.id) << ' ' << g.place << ' ' << g.rotation << '\n';
        out.close();
        if (!out)
            throw std::runtime_error("Cannot write greenhouse arrangement.");
        std::filesystem::rename(temp, dest);
        error_.clear();
        return true;
    } catch (const std::exception& e) {
        error_ = e.what();
        return false;
    }
}
bool greenhouse::lift(const std::string& id) {
    if (mode_ != greenhouse_mode::explore)
        return false;
    auto* g = find(id);
    if (!g || g->place < 0)
        return false;
    selected_ = id;
    begin(greenhouse_mode::lifting);
    return true;
}
bool greenhouse::place(int p) {
    if (mode_ != greenhouse_mode::carry || p < 0 || p >= int(spots().size()))
        return false;
    auto* occupant = at(p);
    if (occupant && occupant->id != selected_)
        return false;
    destination_ = p;
    begin(greenhouse_mode::lowering);
    return true;
}
bool greenhouse::edit(const std::string& id) {
    if (mode_ != greenhouse_mode::explore)
        return false;
    auto* g = find(id);
    if (!g || g->place < 0)
        return false;
    selected_ = id;
    begin(greenhouse_mode::enter_editor);
    return true;
}
bool greenhouse::leave() {
    if (mode_ == greenhouse_mode::editor) {
        begin(greenhouse_mode::leave_editor);
        return true;
    }
    if (mode_ == greenhouse_mode::journal) {
        mode_ = greenhouse_mode::explore;
        return true;
    }
    return false;
}
bool greenhouse::open_journal() {
    if (mode_ != greenhouse_mode::explore)
        return false;
    mode_ = greenhouse_mode::journal;
    return true;
}
bool greenhouse::track(const std::string& id) {
    if (mode_ != greenhouse_mode::journal || !find(id) || find(id)->place < 0)
        return false;
    tracked_ = id;
    mode_ = greenhouse_mode::explore;
    return true;
}
bool greenhouse::rotate(float radians) {
    if (mode_ != greenhouse_mode::carry || !std::isfinite(radians))
        return false;
    if (auto* g = mutable_active()) {
        g->rotation = std::remainder(g->rotation + radians, 6.2831853f);
        g->dirty = true;
        return true;
    }
    return false;
}
bool greenhouse::rain() {
    if (mode_ != greenhouse_mode::editor)
        return false;
    auto* g = mutable_active();
    g->world.rain();
    g->dirty = true;
    return true;
}
bool greenhouse::plant(plant_kind kind, vec2 p) {
    if (mode_ != greenhouse_mode::editor)
        return false;
    auto* g = mutable_active();
    bool ok = g->world.add_plant(kind, p);
    g->dirty |= ok;
    return ok;
}
bool greenhouse::introduce(species_kind species, vec2 point) {
    if (mode_ != greenhouse_mode::editor || int(species) < 0 || int(species) > 2)
        return false;
    auto* g = mutable_active();
    bool ok = g->world.add_creature(species, point) != 0;
    g->dirty |= ok;
    return ok;
}
bool greenhouse::water(vec2 p) {
    if (mode_ != greenhouse_mode::editor)
        return false;
    auto* g = mutable_active();
    g->world.water(p);
    g->dirty = true;
    return true;
}
bool greenhouse::climate(float t, float light, float vent) {
    if (mode_ != greenhouse_mode::editor || !std::isfinite(t) || !std::isfinite(light) ||
        !std::isfinite(vent))
        return false;
    auto* g = mutable_active();
    g->world.climate.temperature = std::clamp(t, 0.f, 40.f);
    g->world.climate.sunlight = std::clamp(light, 0.f, 1.5f);
    g->world.climate.opening = std::clamp(vent, 0.f, 1.f);
    g->dirty = true;
    return true;
}
bool greenhouse::set_speed(double v) {
    if ((mode_ != greenhouse_mode::editor && mode_ != greenhouse_mode::journal) || !std::isfinite(v) ||
        v < 0 || v > 240)
        return false;
    speed_ = v;
    return true;
}
float greenhouse::carry_blend() const {
    float t = mode_ == greenhouse_mode::carry ? 1.f : 0.f;
    if (mode_ == greenhouse_mode::lifting)
        t = std::clamp((progress_ - .28f) / .72f, 0.f, 1.f);
    if (mode_ == greenhouse_mode::lowering)
        t = 1 - std::clamp(progress_ / .72f, 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
bool greenhouse::carry_clear(const terrarium::camera_pose& pose, const terrarium::landscape& land) const {
    if (!holding() || !active())
        return true;
    const auto base = compute_carry_pose(pose, active()->world.design()).base;
    float x = base.x, y = base.y, z = base.z;
    float radius = float(active()->world.design().radius()),
          height = float(active()->world.design().height());
    for (const auto& b : land.obstacles) {
        if (y + height <= b.low.y + .005f || y >= b.high.y - .005f)
            continue;
        float dx = x - std::clamp(x, b.low.x, b.high.x), dz = z - std::clamp(z, b.low.z, b.high.z);
        if (dx * dx + dz * dz < radius * radius)
            return false;
    }
    return true;
}
terrarium::explorer_input greenhouse::locomotion(terrarium::explorer_input input) const {
    if (!can_walk())
        return {};
    if (mode_ == greenhouse_mode::carry) {
        float length = std::max(1.f, std::hypot(input.forward, input.right));
        input.forward *= .52f / length;
        input.right *= .52f / length;
        input.running = false;
        input.crouching = false;
        input.jump_pressed = false;
    }
    return input;
}
int greenhouse::target(const terrarium::camera_pose& camera, const terrarium::landscape& land,
                       bool empty) const {
    float best = 2.6f;
    int result = -1;
    for (int i = 0; i < int(spots().size()); ++i) {
        const auto* g = at(i);
        if (empty ? g && g->id != selected_ : !g)
            continue;
        auto p = spots()[i].position;
        float r = g ? float(g->world.design().radius()) : .23f;
        float h = empty ? .08f : g ? float(g->world.design().height()) : .35f;
        if (auto hit =
                sengine::intersect(camera.eye, camera.direction,
                                   {{p.x - r, p.y + .006f, p.z - r}, {p.x + r, p.y + h, p.z + r}}, best)) {
            bool blocked = false;
            for (auto& box : land.obstacles)
                if (auto wall = sengine::intersect(camera.eye, camera.direction, box, *hit - .02f)) {
                    blocked = true;
                    break;
                }
            if (!blocked) {
                best = *hit;
                result = i;
            }
        }
    }
    return result;
}
}
