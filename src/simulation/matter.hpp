#pragma once
#include <array>
namespace terrarium {

struct matter_amount {
    double carbon{}, nitrogen{};
};
struct matter_account {
    matter_amount initial{}, introduced{};
    double fixed_carbon{}, respired_carbon{};
};
struct matter_reading {
    double day{};

    std::array<double, 5> nitrogen{};
    double carbon{};
};
enum class amendment { compost, wood };
inline constexpr double plant_carbon = 450;
inline constexpr double plant_nitrogen = 20;
inline constexpr double microbial_cn = 8;
}
