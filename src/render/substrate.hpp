#pragma once
#include "ecosystem.hpp"
#include "sengine/scene.hpp"
#include <filesystem>
#include <memory>
namespace terrarium::render {
class substrate_meshes {
  public:
    substrate_meshes(sengine::scene&, sengine::material_id soil, const std::filesystem::path& water_material);
    ~substrate_meshes();
    void update(unsigned placement, const world_state*, const sengine::mat4& jar);

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
}
