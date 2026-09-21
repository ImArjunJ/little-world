#pragma once
#include "ecosystem.hpp"
#include <filesystem>
#include <memory>
#include <utils/Entity.h>

namespace filament {
class Engine;
}
namespace terrarium::render {
class fauna_geometry {
  public:
    fauna_geometry(filament::Engine&, const std::filesystem::path& directory);
    ~fauna_geometry();
    void build(utils::Entity, species_kind);

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
}
