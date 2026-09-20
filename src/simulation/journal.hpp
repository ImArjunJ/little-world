#pragma once
#include "ecosystem.hpp"
#include <vector>
namespace terrarium {

class journal_timeline {
  public:
    explicit journal_timeline(const world_state& world);
    journal_timeline(const std::deque<population_sample>& history, population_sample current);
    const std::vector<population_sample>& samples() const { return samples_; }
    double first_day() const { return samples_.front().day; }
    double last_day() const { return samples_.back().day; }
    double fraction(double day) const;
    std::size_t nearest(double fraction) const;
    int ceiling(int series) const;
    static int count(const population_sample& sample, int series);

  private:
    std::vector<population_sample> samples_;
};
class matter_timeline {
  public:
    explicit matter_timeline(const world_state& world);
    matter_timeline(const std::deque<matter_reading>& history, matter_reading current);
    const std::vector<matter_reading>& samples() const { return samples_; }
    double fraction(double day) const;
    std::size_t nearest(double fraction) const;
    double ceiling() const;

  private:
    std::vector<matter_reading> samples_;
};
}
