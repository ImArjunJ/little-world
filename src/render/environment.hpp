#pragma once
#include <memory>

namespace filament {
class Engine;
class Scene;
}

namespace terrarium::render {
class environment_lighting {
  public:
    environment_lighting(filament::Engine&, filament::Scene&);
    ~environment_lighting();
    environment_lighting(const environment_lighting&) = delete;
    environment_lighting& operator=(const environment_lighting&) = delete;

  private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
}
