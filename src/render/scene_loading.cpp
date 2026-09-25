#include "garden_scene_state.hpp"
#include <stdexcept>
namespace terrarium::render {
using namespace sengine;
namespace {
void configure_lighting(garden_scene_state& state, render_quality quality) {
    auto& scene = state.resources;
    state.options.temporal_aa = quality == render_quality::high;
    state.options.occlusion = quality == render_quality::high;
    state.options.soft_shadows = quality == render_quality::high;
    state.options.fog = {true, 55, .003f, 320, .035f, {.60f, .72f, .79f}};
    state.graphics.configure(state.options);
    state.graphics.visible_layers(0x01);
    sun_options sun;
    sun.color = {1, .91f, .76f};
    sun.intensity = 28000;
    sun.direction = {.42f, -.46f, .78f};
    sun.shadow_size = quality == render_quality::high ? 2048 : 1024;
    sun.cascades = quality == render_quality::high ? 3 : 2;
    sun.shadow_far = quality == render_quality::high ? 90.f : 45.f;
    state.sun = add_sun(scene, sun);
    set_environment(scene, {});
}
void cast_shadows(model_instance& model, model_node node, bool enabled) {
    if (model.renderable(node))
        model.cast_shadows(node, enabled);
    for (auto child : model.children(node))
        cast_shadows(model, child, enabled);
}
float canopy_radius(const model_instance& model, model_node node) {
    float result{};
    if (model.renderable(node)) {
        const auto bounds = model.bounds(node);
        const auto transform = model.world_transform(node);
        for (int x : {-1, 1})
            for (int y : {-1, 1})
                for (int z : {-1, 1}) {
                    const auto corner =
                        transform *
                        float4(bounds.center + bounds.half_extent * float3{float(x), float(y), float(z)}, 1);
                    result = std::max(result, std::hypot(corner.x, corner.z));
                }
    }
    for (auto child : model.children(node))
        result = std::max(result, canopy_radius(model, child));
    return result;
}
void bind_gardens(garden_scene_state& state, material_id glass) {
    auto& model = *state.gardens;
    state.plots.reserve(greenhouse::spots().size());
    for (std::size_t i = 0; i < greenhouse::spots().size(); ++i) {
        const auto suffix = std::to_string(i);
        garden_plot plot;
        plot.root = model.find("Garden " + suffix);
        plot.marker = model.find("Placement " + suffix);
        model.show(plot.root, false);
        for (unsigned kind = 0; kind < plot.vessels.size(); ++kind) {
            const auto node = model.find("Vessel " + suffix + " " + std::to_string(kind));
            plot.vessels[kind] = node;
            model.cast_shadows(node, false);
            if (kind == 0 || kind == 3)
                model.material(node, glass);
        }
        for (auto node : model.children(plot.root)) {
            if (std::ranges::find(plot.vessels, node) != plot.vessels.end())
                continue;
            const auto variants = model.children(node);
            if (variants.size() != 3)
                throw std::runtime_error("A plant needs three foliage variants");
            plant_nodes plant{{node, model.local_transform(node)}, {variants[0], variants[1], variants[2]}};
            for (auto variant : plant.foliage)
                model.show(variant, false);
            plot.plants.push_back(plant);
        }
        if (plot.plants.empty())
            throw std::runtime_error("Garden has no plant slots");
        state.plots.push_back(std::move(plot));
    }
    for (unsigned kind = 0; kind < state.previews.size(); ++kind) {
        const auto node = model.find("Preview " + std::to_string(kind));
        state.previews[kind] = {node, model.local_transform(node)};
        model.show(node, false);
        cast_shadows(model, node, false);
    }
    for (unsigned kind = 0; kind < state.plant_radius.size(); ++kind) {
        const auto radius = canopy_radius(model, state.plots.front().plants.front().foliage[kind]);
        if (!std::isfinite(radius) || radius <= 0)
            throw std::runtime_error("Imported plant canopy has invalid bounds");
        state.plant_radius[kind] = radius;
    }
}
}
void load_garden_scene(garden_scene_state& state, const std::filesystem::path& path, render_quality quality) {
    auto& scene = state.resources;
    const auto directory = path.parent_path();
    configure_lighting(state, quality);
    state.environment = std::make_unique<model_instance>(model(scene, path));
    for (const auto& node : state.environment->nodes())
        if (node.name.starts_with("Glazing") && state.environment->renderable(node.id))
            state.environment->cast_shadows(node.id, false);
    state.environment->visible(true);
    state.environment->synchronize();
    state.gardens = std::make_unique<model_instance>(model(scene, directory / "gardens.glb"));
    bind_gardens(state, load_material(scene, directory / "glass.filamat"));
    state.substrate = std::make_unique<substrate_meshes>(
        scene, state.gardens->copy_material(state.plots.front().vessels[1]), directory / "water.filamat");
    state.fauna = std::make_unique<fauna_meshes>(scene, directory);
    state.character = std::make_unique<gardener>(
        scene, directory / "gardener.glb", *state.gardens,
        std::array{state.gardens->find("Gardener boot left"), state.gardens->find("Gardener boot right")});
    state.gardens->visible(true);
    state.gardens->synchronize();
    state.hud = std::make_unique<native_hud>(state.graphics, directory / "hud.filamat",
                                             directory.parent_path() / "fonts/Body.ttf",
                                             directory.parent_path() / "fonts/Display.ttf");
}
}
