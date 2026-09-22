#include "render/substrate.hpp"
#include "habitat_surface.hpp"
namespace terrarium::render {
struct substrate_meshes::impl {
    sengine::scene& scene;
    struct part {
        sengine::scene_node entity;
        bool visible{};
    };
    struct garden {
        std::array<part, 3> parts;
        sengine::material_id soil, basin, water;
    };
    std::array<sengine::mesh_id, 3> meshes;
    std::array<garden, 10> gardens;
    explicit impl(sengine::scene& scene) : scene(scene) {}
};
substrate_meshes::substrate_meshes(sengine::scene& scene, sengine::material_id soil,
                                   const std::filesystem::path& water_material)
    : impl_(std::make_unique<impl>(scene)) {
    auto& p = *impl_;
    const auto water = load_material(scene, water_material);
    for (unsigned part = 0; part < p.meshes.size(); ++part) {
        auto source = presentation::make_surface_mesh(presentation::surface_part(part));
        sengine::mesh_data mesh;
        mesh.volume = {{0, -.5f, 0}, {1, .5f, 1}};
        mesh.reverse_bitangent = true;
        mesh.indices = std::move(source.indices);
        for (const auto& v : source.vertices) {
            sengine::float3 normal{v.normal.x, v.normal.y, v.normal.z};
            mesh.vertices.push_back({.position = {v.position.x, v.position.y, v.position.z},
                                     .normal = normal,
                                     .tangent = normalize(cross(sengine::float3{0, 0, -1}, normal)),
                                     .uv = {v.uv.x, v.uv.y}});
        }
        p.meshes[part] = upload_mesh(scene, mesh);
    }
    for (auto& garden : p.gardens) {
        garden.soil = duplicate_material(scene, soil, "Surface earth");
        garden.basin = duplicate_material(scene, soil, "Basin earth");
        garden.water = duplicate_material(scene, water);
        std::array materials{garden.soil, garden.basin, garden.water};
        for (unsigned i = 0; i < garden.parts.size(); ++i)
            garden.parts[i].entity = create_mesh(scene, p.meshes[i], materials[i], i != 2);
    }
}
substrate_meshes::~substrate_meshes() = default;
void substrate_meshes::update(unsigned placement, const world_state* world, const sengine::mat4& jar) {
    auto& p = *impl_;
    auto& g = p.gardens.at(placement);
    auto& scene = p.scene;
    for (unsigned i = 0; i < g.parts.size(); ++i) {
        auto& part = g.parts[i];
        bool visible = world && (i != 2 || world->pond_level() > .0001f);
        if (visible != part.visible) {
            part.visible = visible;
            if (visible)
                sengine::visible(scene, part.entity, true);
            else
                sengine::visible(scene, part.entity, false);
        }
    }
    if (!world)
        return;
    const auto& design = world->design();
    float height = float(design.height());
    float top = float(design.soil_depth + design.drainage_depth) / height;
    float depth = presentation::basin_depth(design);
    const auto ground = jar * sengine::translation(sengine::float3{0, top, 0}) *
                        sengine::scaling(sengine::float3{1, depth / height, 1});
    for (unsigned i = 0; i < 2; ++i)
        set_transform(scene, g.parts[i].entity, ground);
    float radius = std::max(.0001f, presentation::water_radius(*world));
    set_transform(
        scene, g.parts[2].entity,
        jar *
            sengine::translation(sengine::float3{world_state::pond_center.x * presentation::habitat_scale,
                                                 top + presentation::water_offset(*world) / height,
                                                 world_state::pond_center.y * presentation::habitat_scale}) *
            sengine::scaling(
                sengine::float3{world_state::pond_radii.x * presentation::habitat_scale * radius, 1,
                                world_state::pond_radii.y * presentation::habitat_scale * radius}));
    set_parameter(scene, g.water, "depth", depth + presentation::water_offset(*world));
    float wetness = float(world->moisture());
    float shade = std::lerp(.95f, .66f, wetness);
    set_color(scene, g.soil, "baseColorFactor", sengine::float4{shade, shade * .96f, shade * .89f, 1});
    set_color(scene, g.basin, "baseColorFactor", sengine::float4{shade, shade * .96f, shade * .89f, 1});
    set_parameter(scene, g.basin, "roughnessFactor", .88f);
}
}
