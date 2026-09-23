#pragma once
#include "vessel.hpp"
namespace terrarium {
enum class vessel_form { legacy, pocket, bowl, tall };
enum class soil_mix { forest, loam, sand };
enum class starter { empty, woodland, meadow };
struct garden_design {
    vessel_form form{vessel_form::bowl};
    soil_mix mix{soil_mix::forest};
    double soil_depth{.06}, drainage_depth{.025};

  public:
    bool valid() const;
    double radius() const;
    double height() const;
    double area() const;
    double porosity() const;
    double field_capacity() const;
    double drainage_rate() const;
    double soil_water_capacity() const;
    double reservoir_capacity() const;
    physics::vessel_parameters parameters() const;
};
const char* vessel_name(vessel_form);
const char* soil_name(soil_mix);
class world_state;
void plant_starter(world_state&, starter);
}
