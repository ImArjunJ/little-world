#pragma once
#include "ecosystem.hpp"
#include "sengine/drawing.hpp"
namespace terrarium {
void draw_creature_portrait(const creature_state&, sengine::drawing::rect);
void draw_plant_portrait(const plant_state&, sengine::drawing::rect);
}
