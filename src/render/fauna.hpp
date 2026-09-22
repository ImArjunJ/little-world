#pragma once
#include "animal_motion.hpp"
#include "sengine/scene.hpp"
#include <filesystem>
#include <memory>
namespace terrarium::render {
class fauna_meshes {
  public:
    fauna_meshes(sengine::scene&, sengine::scene_asset shells, const std::filesystem::path& directory);
    ~fauna_meshes();
    void place(species_kind, const sengine::mat4&, const presentation::animal_weights&);
    void finish();

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
}
