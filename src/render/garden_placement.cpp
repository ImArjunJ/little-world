#include "garden_scene_state.hpp"
#include "habitat_surface.hpp"
namespace terrarium::render {
using namespace sengine;
namespace {
void update_plants(garden_scene_state& state, std::size_t i, const greenhouse_garden& garden, float radius,
                   float height, float soil_top) {
    auto& model = *state.gardens;
    auto& plot = state.plots[i];
    if (garden.world.plants().size() > plot.plants.size())
        throw std::runtime_error("Garden exceeds its imported plant capacity");
    for (size_t j = 0; j < std::max(plot.visible_plants, garden.world.plants().size()); ++j) {
        auto& plant_nodes = plot.plants[j];
        const auto& n = plant_nodes.root;
        auto inst = n.node;
        if (j >= garden.world.plants().size()) {
            for (auto& f : plant_nodes.foliage)
                model.show(f, false);
            continue;
        }
        const auto& plant = garden.world.plants()[j];
        for (unsigned k = 0; k < 3; ++k) {
            auto& f = plant_nodes.foliage[k];
            model.show(f, k == unsigned(plant.kind));
        }
        float growth = std::clamp(world_state::growth_scale(plant), .15f, 1.f);
        const float x = plant.position.x / world_state::radius * .80f;
        const float z = plant.position.y / world_state::radius * .80f;
        const float clearance = std::max(.015f, .88f - std::hypot(x, z)) * radius;
        const float desired_height = (1 - soil_top) * height * (.30f + .42f * growth);
        const float plant_height =
            std::min(desired_height, clearance / state.plant_radius[unsigned(plant.kind)]);
        model.local_transform(
            inst, sengine::translation(sengine::float3{x, soil_top + .004f, z}) *
                      sengine::rotation(float(plant.shape % 628) * .01f, sengine::float3{0, 1, 0}) *
                      sengine::scaling(sengine::float3{plant_height / radius, plant_height / height,
                                                       plant_height / radius}) *
                      n.bind);
    }
    plot.visible_plants = garden.world.plants().size();
}
void update_animals(garden_scene_state& state, std::size_t i, const greenhouse_garden& garden,
                    const mat4& transform, float radius, float height, float soil_top) {
    for (const auto& bug : garden.world.creatures()) {
        const float size = creature_length(bug);
        const auto pose =
            transform *
            sengine::translation(sengine::float3{
                bug.position.x / world_state::radius * .8f,
                soil_top + (presentation::bed_offset(garden.world, bug.position) + .001f) / height,
                bug.position.y / world_state::radius * .8f}) *
            sengine::scaling(sengine::float3{size / radius, size / height, size / radius}) *
            sengine::rotation(-bug.heading, sengine::float3{0, 1, 0});
        state.fauna->place(bug.species, pose, state.plots[i].motion.weights(bug.id));
    }
}
}
void update_gardens(garden_scene_state& state, const greenhouse& game, carrying_pose& carry) {
    auto& model = *state.gardens;
    const auto* active = game.active();
    const auto held = carry.held;
    const float hold = carry.blend;
    auto& grip = carry.grip;
    for (size_t i = 0; i < state.plots.size(); ++i) {
        auto& plot = state.plots[i];
        const greenhouse_garden* garden = nullptr;
        for (const auto& candidate : game.gardens())
            if (candidate.place == int(i)) {
                garden = &candidate;
                break;
            }
        auto mark = point(greenhouse::spots()[i].position);
        mark.y += .009f;
        model.local_transform(plot.marker, sengine::translation(game.mode() == greenhouse_mode::carry &&
                                                                        (!garden || garden == active)
                                                                    ? mark
                                                                    : sengine::float3{0, -100, 0}));
        model.show(plot.root, garden != nullptr);
        if (!garden) {
            state.substrate->update(i, nullptr, {});
            for (std::size_t j = 0; j < plot.visible_plants; ++j)
                for (auto node : plot.plants[j].foliage)
                    model.show(node, false);
            plot.visible_plants = 0;
            plot.motion = {};
            continue;
        }
        float radius = float(garden->world.design().radius()),
              height = float(garden->world.design().height());
        float soil_top =
            float(garden->world.design().soil_depth + garden->world.design().drainage_depth) / height;
        float drain = float(garden->world.design().drainage_depth) / height,
              soil = float(garden->world.design().soil_depth) / height;
        model.local_transform(plot.vessels[1],
                              sengine::translation(sengine::float3{0, drain - .08f * soil / .155f, 0}) *
                                  sengine::scaling(sengine::float3{1, soil / .155f, 1}));
        model.local_transform(plot.vessels[2], sengine::scaling(sengine::float3{1, drain / .08f, 1}));
        auto location = point(greenhouse::spots()[i].position);
        if (garden == active && game.holding()) {
            auto anchor = location;
            if (game.mode() == greenhouse_mode::lowering)
                anchor = point(greenhouse::spots()[game.destination()].position);
            location = anchor * (1 - hold) + held * hold;
            grip = location;
        }
        auto transform = sengine::translation(location) *
                         sengine::rotation(garden->rotation, sengine::float3{0, 1, 0}) *
                         sengine::scaling(sengine::float3{radius, height, radius});
        if (transform != plot.transform) {
            model.local_transform(plot.root, transform);
            plot.transform = transform;
        }
        state.substrate->update(i, &garden->world, transform);
        plot.motion.update(garden->world, garden->id);
        update_plants(state, i, *garden, radius, height, soil_top);
        update_animals(state, i, *garden, transform, radius, height, soil_top);
    }
}
void update_preview(garden_scene_state& state, const greenhouse& game,
                    const std::optional<tending_preview>& preview) {
    auto& model = *state.gardens;
    const auto* active = game.active();
    for (unsigned kind = 0; kind < state.previews.size(); ++kind) {
        auto& n = state.previews[kind];
        const bool visible = preview && active && active->place >= 0 &&
                             game.mode() == greenhouse_mode::editor && int(preview->tool) == int(kind) + 1;
        model.show(n.node, visible);
        if (!visible)
            continue;
        const auto& design = active->world.design();
        const float radius = float(design.radius()), height = float(design.height());
        const float soil_top = float(design.soil_depth + design.drainage_depth) / height;
        const float x = preview->position.x / world_state::radius * .8f;
        const float z = preview->position.y / world_state::radius * .8f;
        float size;
        if (kind < 3) {
            plant_state plant{};
            plant.kind = plant_kind(kind);
            plant.biomass = .45f;
            const float growth = std::clamp(world_state::growth_scale(plant), .15f, 1.f);
            const float clearance = std::max(.015f, .88f - std::hypot(x, z)) * radius;
            size = std::min((1 - soil_top) * height * (.30f + .42f * growth),
                            clearance / state.plant_radius[kind]);
        } else {
            creature_state creature{};
            creature.species = species_kind(kind - 3);
            creature.age = maturity_age(creature.species);
            size = creature_length(creature);
        }
        model.local_transform(
            n.node,
            state.plots[active->place].transform *
                sengine::translation(sengine::float3{
                    x,
                    soil_top +
                        (kind < 3
                             ? presentation::surface_offset(active->world, preview->position) / height + .004f
                             : (presentation::bed_offset(active->world, preview->position) + .001f) / height),
                    z}) *
                sengine::scaling(sengine::float3{size / radius, size / height, size / radius}) * n.bind);
    }
}
}
