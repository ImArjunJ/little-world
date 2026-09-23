#pragma once
#include "sengine/application.hpp"

namespace terrarium {
class application {
  public:
    void run();

  private:
    sengine::application host_{{.title = "Little World — greenhouse", .width = 1440, .height = 900}};
};
}
