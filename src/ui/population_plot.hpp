#pragma once
#include "journal.hpp"
#include "sengine/drawing.hpp"

namespace terrarium {
class population_plot {
  public:
    population_plot(const journal_timeline& timeline, sengine::drawing::rect bounds, int series, int ceiling)
        : timeline_(timeline), bounds_(bounds), series_(series), ceiling_(ceiling) {}
    sengine::drawing::point2 project(const population_sample& sample) const {
        return {bounds_.x + float(timeline_.fraction(sample.day)) * bounds_.width,
                bounds_.y +
                    bounds_.height * (1 - float(journal_timeline::count(sample, series_)) / ceiling_)};
    }

  private:
    const journal_timeline& timeline_;
    sengine::drawing::rect bounds_;
    int series_, ceiling_;
};
}
