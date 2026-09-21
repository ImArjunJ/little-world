#include "render/substrate.hpp"
#include "habitat_surface.hpp"
#include "render/buffer_upload.hpp"
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <fstream>
#include <math/mat3.h>
#include <stdexcept>
#include <utils/EntityManager.h>

namespace terrarium::render {
using namespace filament;
namespace {
struct vertex {
    math::float3 position;
    math::quatf tangent;
    math::float2 uv;
    math::float4 color{1};
};
}
struct substrate_meshes::impl {
    Engine& engine;
    Scene& scene;
    struct mesh {
        VertexBuffer* vertices{};
        IndexBuffer* indices{};
    };
    struct part {
        utils::Entity entity;
        bool visible{};
    };
    struct garden {
        std::array<part, 3> parts;
        MaterialInstance* soil{};
        MaterialInstance* basin{};
        MaterialInstance* water{};
    };
    std::array<mesh, 3> meshes;
    std::array<garden, 10> gardens;
    Material* water{};
    impl(Engine& e, Scene& s) : engine(e), scene(s) {}
    ~impl() {
        for (auto& g : gardens) {
            for (auto& p : g.parts) {
                scene.remove(p.entity);
                engine.destroy(p.entity);
                utils::EntityManager::get().destroy(p.entity);
            }
            engine.destroy(g.soil);
            engine.destroy(g.basin);
            engine.destroy(g.water);
        }
        for (auto& mesh : meshes) {
            engine.destroy(mesh.vertices);
            engine.destroy(mesh.indices);
        }
        engine.destroy(water);
    }
};
substrate_meshes::substrate_meshes(Engine& engine, Scene& scene, const MaterialInstance& soil,
                                   const std::filesystem::path& water_material)
    : impl_(std::make_unique<impl>(engine, scene)) {
    auto& p = *impl_;
    std::ifstream input(water_material, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
    if (bytes.empty())
        throw std::runtime_error("Water material unavailable");
    p.water = Material::Builder().package(bytes.data(), bytes.size()).build(engine);
    if (!p.water)
        throw std::runtime_error("Water material invalid");
    for (unsigned part = 0; part < p.meshes.size(); ++part) {
        auto source = presentation::make_surface_mesh(presentation::surface_part(part));
        auto& mesh = p.meshes[part];
        std::vector<vertex> vertices;
        for (const auto& v : source.vertices) {
            math::float3 normal{v.normal.x, v.normal.y, v.normal.z};
            auto tangent = normalize(cross(math::float3{0, 0, -1}, normal));
            auto bitangent = cross(tangent, normal);
            vertices.push_back({{v.position.x, v.position.y, v.position.z},
                                math::mat3f::packTangentFrame({tangent, bitangent, normal}),
                                {v.uv.x, v.uv.y}});
        }
        mesh.vertices = VertexBuffer::Builder()
                            .vertexCount(vertices.size())
                            .bufferCount(1)
                            .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT3,
                                       offsetof(vertex, position), sizeof(vertex))
                            .attribute(VertexAttribute::TANGENTS, 0, VertexBuffer::AttributeType::FLOAT4,
                                       offsetof(vertex, tangent), sizeof(vertex))
                            .attribute(VertexAttribute::UV0, 0, VertexBuffer::AttributeType::FLOAT2,
                                       offsetof(vertex, uv), sizeof(vertex))
                            .attribute(VertexAttribute::UV1, 0, VertexBuffer::AttributeType::FLOAT2,
                                       offsetof(vertex, uv), sizeof(vertex))
                            .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::FLOAT4,
                                       offsetof(vertex, color), sizeof(vertex))
                            .build(engine);
        mesh.indices = IndexBuffer::Builder()
                           .indexCount(source.indices.size())
                           .bufferType(IndexBuffer::IndexType::UINT)
                           .build(engine);
        mesh.vertices->setBufferAt(engine, 0, upload(std::move(vertices)));
        mesh.indices->setBuffer(engine, upload(std::move(source.indices)));
    }
    for (auto& g : p.gardens) {
        g.soil = MaterialInstance::duplicate(&soil, "Surface earth");
        g.basin = MaterialInstance::duplicate(&soil, "Basin earth");
        g.water = p.water->createInstance();
        std::array<MaterialInstance*, 3> materials{g.soil, g.basin, g.water};
        for (unsigned i = 0; i < g.parts.size(); ++i) {
            auto& part = g.parts[i];
            part.entity = utils::EntityManager::get().create();
            engine.getTransformManager().create(part.entity);
            RenderableManager::Builder(1)
                .boundingBox({{0, -.5f, 0}, {1, .5f, 1}})
                .material(0, materials[i])
                .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, p.meshes[i].vertices,
                          p.meshes[i].indices)
                .castShadows(i != 2)
                .receiveShadows(true)
                .build(engine, part.entity);
        }
    }
}
substrate_meshes::~substrate_meshes() = default;
void substrate_meshes::update(unsigned placement, const world_state* world, const math::mat4f& jar) {
    auto& p = *impl_;
    auto& g = p.gardens.at(placement);
    auto& transforms = p.engine.getTransformManager();
    for (unsigned i = 0; i < g.parts.size(); ++i) {
        auto& part = g.parts[i];
        bool visible = world && (i != 2 || world->pond_level() > .0001f);
        if (visible != part.visible) {
            part.visible = visible;
            if (visible)
                p.scene.addEntity(part.entity);
            else
                p.scene.remove(part.entity);
        }
    }
    if (!world)
        return;
    const auto& design = world->design();
    float height = float(design.height());
    float top = float(design.soil_depth + design.drainage_depth) / height;
    float depth = presentation::basin_depth(design);
    const auto ground = jar * math::mat4f::translation(math::float3{0, top, 0}) *
                        math::mat4f::scaling(math::float3{1, depth / height, 1});
    for (unsigned i = 0; i < 2; ++i)
        transforms.setTransform(transforms.getInstance(g.parts[i].entity), ground);
    float radius = std::max(.0001f, presentation::water_radius(*world));
    transforms.setTransform(
        transforms.getInstance(g.parts[2].entity),
        jar *
            math::mat4f::translation(math::float3{world_state::pond_center.x * presentation::habitat_scale,
                                                  top + presentation::water_offset(*world) / height,
                                                  world_state::pond_center.y * presentation::habitat_scale}) *
            math::mat4f::scaling(
                math::float3{world_state::pond_radii.x * presentation::habitat_scale * radius, 1,
                             world_state::pond_radii.y * presentation::habitat_scale * radius}));
    g.water->setParameter("depth", depth + presentation::water_offset(*world));
    float wetness = float(world->moisture());
    float shade = std::lerp(.95f, .66f, wetness);
    g.soil->setParameter("baseColorFactor", RgbaType::sRGB,
                         math::float4{shade, shade * .96f, shade * .89f, 1});
    g.basin->setParameter("baseColorFactor", RgbaType::sRGB,
                          math::float4{shade, shade * .96f, shade * .89f, 1});
    g.basin->setParameter("roughnessFactor", .88f);
}
}
