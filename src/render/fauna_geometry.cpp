#include "render/fauna_geometry.hpp"
#include "render/buffer_upload.hpp"
#include <algorithm>
#include <bit>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/MorphTargetBuffer.h>
#include <filament/RenderableManager.h>
#include <filament/VertexBuffer.h>
#include <fstream>
#include <math/mat3.h>
#include <math/norm.h>
#include <stdexcept>

namespace terrarium::render {
using namespace filament;
namespace {
struct vertex {
    math::float3 position;
    math::quatf tangent;
    math::float4 color;
};
template <class element> void read(std::istream& input, element* values, size_t count = 1) {
    static_assert(std::endian::native == std::endian::little);
    if (!input.read(reinterpret_cast<char*>(values), sizeof(element) * count))
        throw std::runtime_error("Truncated animal geometry");
}
math::quatf tangent_frame(math::float3 normal) {
    normal = normalize(normal);
    const auto reference = std::abs(normal.y) < .98f ? math::float3{0, 1, 0} : math::float3{1, 0, 0};
    const auto tangent = normalize(cross(reference, normal));
    return math::mat3f::packTangentFrame({tangent, cross(normal, tangent), normal});
}
}
struct fauna_geometry::impl {
    Engine& engine;
    Material* material{};
    struct mesh {
        VertexBuffer* vertices{};
        IndexBuffer* indices{};
        MorphTargetBuffer* motion{};
        MaterialInstance* material{};
        Box bounds;
    };
    std::array<mesh, 3> meshes;
    explicit impl(Engine& e) : engine(e) {}
    ~impl() {
        for (auto& mesh : meshes) {
            engine.destroy(mesh.vertices);
            engine.destroy(mesh.indices);
            engine.destroy(mesh.motion);
            engine.destroy(mesh.material);
        }
        engine.destroy(material);
    }
};
fauna_geometry::fauna_geometry(Engine& engine, const std::filesystem::path& directory)
    : impl_(std::make_unique<impl>(engine)) {
    auto& p = *impl_;
    std::ifstream material(directory / "fauna.filamat", std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(material)), {});
    if (bytes.empty())
        throw std::runtime_error("Animal material unavailable");
    p.material = Material::Builder().package(bytes.data(), bytes.size()).build(engine);
    if (!p.material)
        throw std::runtime_error("Animal material invalid");
    std::ifstream input(directory / "fauna.bin", std::ios::binary);
    std::array<char, 4> magic{};
    read(input, magic.data(), magic.size());
    if (magic != std::array{'F', 'N', 'A', '2'})
        throw std::runtime_error("Unsupported animal geometry");
    for (auto& mesh : p.meshes) {
        std::uint32_t count, index_count;
        float roughness;
        math::float3 low, high;
        read(input, &count);
        read(input, &index_count);
        read(input, &roughness);
        read(input, &low);
        read(input, &high);
        if (!count || count > 300000 || !index_count || index_count > 1800000 || index_count % 3 ||
            !(roughness >= 0 && roughness <= 1))
            throw std::runtime_error("Invalid animal geometry header");
        std::vector<std::uint32_t> indices(index_count);
        read(input, indices.data(), index_count);
        if (std::ranges::any_of(indices, [count](auto index) { return index >= count; }))
            throw std::runtime_error("Animal geometry index outside vertex buffer");
        std::vector<math::float3> positions(count), normals(count), colors(count);
        read(input, positions.data(), count);
        read(input, normals.data(), count);
        read(input, colors.data(), count);
        std::vector<vertex> vertices(count);
        for (unsigned i = 0; i < count; ++i)
            vertices[i] = {positions[i], tangent_frame(normals[i]), {colors[i], 1}};
        mesh.bounds = {(low + high) * .5f, (high - low) * .5f + math::float3{.1f}};
        mesh.vertices = VertexBuffer::Builder()
                            .vertexCount(count)
                            .bufferCount(1)
                            .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT3,
                                       offsetof(vertex, position), sizeof(vertex))
                            .attribute(VertexAttribute::TANGENTS, 0, VertexBuffer::AttributeType::FLOAT4,
                                       offsetof(vertex, tangent), sizeof(vertex))
                            .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::FLOAT4,
                                       offsetof(vertex, color), sizeof(vertex))
                            .build(engine);
        mesh.vertices->setBufferAt(engine, 0, upload(std::move(vertices)));
        mesh.indices = IndexBuffer::Builder()
                           .indexCount(index_count)
                           .bufferType(IndexBuffer::IndexType::UINT)
                           .build(engine);
        mesh.indices->setBuffer(engine, upload(std::move(indices)));
        mesh.motion = MorphTargetBuffer::Builder().count(4).vertexCount(count).build(engine);
        for (unsigned target = 0; target < 4; ++target) {
            std::vector<math::float3> positions(count), changes(count);
            std::vector<math::short4> tangents(count);
            read(input, positions.data(), count);
            read(input, changes.data(), count);
            for (unsigned i = 0; i < count; ++i) {
                const auto q = tangent_frame(normals[i] + changes[i]);
                tangents[i] = math::packSnorm16(math::float4{q.x, q.y, q.z, q.w});
            }
            mesh.motion->setPositionsAt(engine, target, positions.data(), count);
            mesh.motion->setTangentsAt(engine, target, tangents.data(), count);
        }
        mesh.material = p.material->createInstance();
        mesh.material->setParameter("roughness", roughness);
    }
    if (input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Unexpected trailing animal geometry");
}
fauna_geometry::~fauna_geometry() = default;
void fauna_geometry::build(utils::Entity entity, species_kind species) {
    auto& p = *impl_;
    const auto& mesh = p.meshes[unsigned(species)];
    RenderableManager::Builder(1)
        .boundingBox(mesh.bounds)
        .material(0, mesh.material)
        .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, mesh.vertices, mesh.indices)
        .morphing(mesh.motion)
        .morphing(0, 0, 0)
        .castShadows(true)
        .receiveShadows(true)
        .build(p.engine, entity);
}
}
