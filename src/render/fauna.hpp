#pragma once
#include "animal_motion.hpp"
#include <filesystem>
#include <math/mat4.h>
#include <memory>

namespace filament {
class Engine;
class Scene;
namespace gltfio {
class AssetLoader;
class FilamentAsset;
}
}
namespace terrarium::render {
class fauna_meshes {
  public:
    fauna_meshes(filament::Engine&, filament::Scene&, filament::gltfio::AssetLoader&,
                 filament::gltfio::FilamentAsset*, const std::filesystem::path& directory);
    ~fauna_meshes();
    void place(species_kind, const filament::math::mat4f&, const presentation::animal_weights&);
    void finish();

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
}
