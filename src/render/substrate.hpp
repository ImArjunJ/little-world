#pragma once
#include "ecosystem.hpp"
#include <filesystem>
#include <math/mat4.h>
#include <memory>
namespace filament {
class Engine;
class Scene;
class MaterialInstance;
}
namespace terrarium::render {
class substrate_meshes {
  public:
    substrate_meshes(filament::Engine&, filament::Scene&, const filament::MaterialInstance& soil,
                     const std::filesystem::path& water_material);
    ~substrate_meshes();
    void update(unsigned placement, const world_state*, const filament::math::mat4f& jar);

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
}
