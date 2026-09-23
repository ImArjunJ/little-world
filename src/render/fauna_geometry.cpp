#include "render/fauna_geometry.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <stdexcept>
namespace terrarium::render {
namespace {
template <class element> void read(std::istream& input, element* values, std::size_t count = 1) {
    static_assert(std::endian::native == std::endian::little);
    if (!input.read(reinterpret_cast<char*>(values), sizeof(element) * count))
        throw std::runtime_error("Truncated animal geometry");
}
}
fauna_geometry load_fauna(sengine::scene& scene, const std::filesystem::path& directory) {
    fauna_geometry result;
    const auto material = load_material(scene, directory / "fauna.filamat");
    std::ifstream input(directory / "fauna.bin", std::ios::binary);
    std::array<char, 4> magic{};
    read(input, magic.data(), magic.size());
    if (magic != std::array{'F', 'N', 'A', '2'})
        throw std::runtime_error("Unsupported animal geometry");
    for (unsigned kind = 0; kind < result.meshes.size(); ++kind) {
        std::uint32_t count, index_count;
        float roughness;
        sengine::float3 low, high;
        read(input, &count);
        read(input, &index_count);
        read(input, &roughness);
        read(input, &low);
        read(input, &high);
        if (!count || count > 300000 || !index_count || index_count > 1800000 || index_count % 3 ||
            !(roughness >= 0 && roughness <= 1))
            throw std::runtime_error("Invalid animal geometry header");
        sengine::mesh_data mesh;
        mesh.indices.resize(index_count);
        read(input, mesh.indices.data(), index_count);
        if (std::ranges::any_of(mesh.indices, [count](auto i) { return i >= count; }))
            throw std::runtime_error("Animal geometry index outside vertex buffer");
        std::vector<sengine::float3> positions(count), normals(count), colors(count);
        read(input, positions.data(), count);
        read(input, normals.data(), count);
        read(input, colors.data(), count);
        for (unsigned i = 0; i < count; ++i)
            mesh.vertices.push_back({.position = positions[i],
                                     .normal = normals[i],
                                     .tangent = {},
                                     .uv = {},
                                     .color = {colors[i], 1}});
        mesh.volume = {(low + high) * .5f, (high - low) * .5f + sengine::float3{.1f}};
        for (unsigned target = 0; target < 4; ++target) {
            sengine::morph_target morph;
            morph.positions.resize(count);
            morph.normals.resize(count);
            read(input, morph.positions.data(), count);
            read(input, morph.normals.data(), count);
            for (unsigned i = 0; i < count; ++i)
                morph.normals[i] = normals[i] + morph.normals[i];
            mesh.morphs.push_back(std::move(morph));
        }
        result.meshes[kind] = upload_mesh(scene, mesh);
        result.materials[kind] = duplicate_material(scene, material);
        set_parameter(scene, result.materials[kind], "roughness", roughness);
    }
    if (input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Unexpected trailing animal geometry");
    return result;
}
}
