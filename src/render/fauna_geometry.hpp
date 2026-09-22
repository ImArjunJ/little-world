#pragma once
#include "ecosystem.hpp"
#include "sengine/scene.hpp"
#include <array>
#include <filesystem>
namespace terrarium::render {
struct fauna_geometry {
    std::array<sengine::mesh_id, 3> meshes;
    std::array<sengine::material_id, 3> materials;
};
fauna_geometry load_fauna(sengine::scene&, const std::filesystem::path& directory);
}
