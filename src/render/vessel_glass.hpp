#pragma once
#include <filesystem>
#include <memory>
namespace filament {
class Engine;
class MaterialInstance;
}
namespace terrarium::render {
class vessel_glass {
  public:
    vessel_glass(filament::Engine&, const std::filesystem::path& material);
    ~vessel_glass();
    filament::MaterialInstance* instance() const;

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
}
