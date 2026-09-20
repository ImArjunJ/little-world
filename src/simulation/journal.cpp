#include "journal.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace terrarium {
namespace {
bool valid(const population_sample& sample) {
    if (!std::isfinite(sample.day) || sample.day < 0 || sample.plants < 0 ||
        sample.plants > static_cast<int>(world_state::max_plants))
        return false;
    int total = 0;
    for (int count : sample.population) {
        if (count < 0 || count > static_cast<int>(world_state::max_creatures))
            return false;
        total += count;
    }
    return total <= static_cast<int>(world_state::max_creatures);
}
}
journal_timeline::journal_timeline(const world_state& world)
    : journal_timeline(world.history(),
                       {world.day(), world.populations(), static_cast<int>(world.plants().size())}) {}
journal_timeline::journal_timeline(const std::deque<population_sample>& history, population_sample current) {
    if (!valid(current))
        throw std::invalid_argument("Invalid current journal census");
    for (const auto& sample : history) {
        if (!valid(sample) || sample.day > current.day ||
            (!samples_.empty() && sample.day <= samples_.back().day))
            throw std::invalid_argument("Invalid journal chronology or census");
        samples_.push_back(sample);
    }
    if (!samples_.empty() && samples_.back().day == current.day)
        samples_.back() = current;
    else
        samples_.push_back(current);
}
double journal_timeline::fraction(double day) const {
    if (last_day() == first_day())
        return 1;
    return std::clamp((day - first_day()) / (last_day() - first_day()), 0., 1.);
}
std::size_t journal_timeline::nearest(double position) const {
    if (!std::isfinite(position))
        position = 1;
    const double day = first_day() + std::clamp(position, 0., 1.) * (last_day() - first_day());
    auto next =
        std::lower_bound(samples_.begin(), samples_.end(), day,
                         [](const population_sample& sample, double value) { return sample.day < value; });
    if (next == samples_.begin())
        return 0;
    if (next == samples_.end())
        return samples_.size() - 1;
    auto index = static_cast<std::size_t>(next - samples_.begin());
    return day - samples_[index - 1].day < next->day - day ? index - 1 : index;
}
int journal_timeline::count(const population_sample& sample, int series) {
    if (series < 0 || series > 3)
        throw std::out_of_range("Journal series");
    return series == 3 ? sample.plants : sample.population[series];
}
int journal_timeline::ceiling(int series) const {
    int maximum = 1;
    for (const auto& sample : samples_)
        maximum = std::max(maximum, count(sample, series));
    int decade = 1;
    while (maximum > 10 * decade)
        decade *= 10;
    for (int step : {1, 2, 5, 10})
        if (step * decade >= maximum)
            return step * decade;
    return maximum;
}
matter_timeline::matter_timeline(const world_state& world)
    : matter_timeline(world.matter_history(), world.matter_sample()) {}
matter_timeline::matter_timeline(const std::deque<matter_reading>& history, matter_reading current) {
    auto valid = [](const matter_reading& sample) {
        if (!std::isfinite(sample.day) || sample.day < 0 || !std::isfinite(sample.carbon) ||
            sample.carbon < 0)
            return false;
        for (double value : sample.nitrogen)
            if (!std::isfinite(value) || value < 0)
                return false;
        return true;
    };
    if (!valid(current))
        throw std::invalid_argument("Invalid current resource observation");
    for (const auto& sample : history) {
        if (!valid(sample) || sample.day > current.day ||
            (!samples_.empty() && sample.day <= samples_.back().day))
            throw std::invalid_argument("Invalid resource history");
        samples_.push_back(sample);
    }
    if (!samples_.empty() && samples_.back().day == current.day)
        samples_.back() = current;
    else
        samples_.push_back(current);
}
double matter_timeline::fraction(double day) const {
    const double first = samples_.front().day, last = samples_.back().day;
    return first == last ? 1 : std::clamp((day - first) / (last - first), 0., 1.);
}
std::size_t matter_timeline::nearest(double position) const {
    if (!std::isfinite(position))
        position = 1;
    const double day =
        samples_.front().day + std::clamp(position, 0., 1.) * (samples_.back().day - samples_.front().day);
    auto next = std::lower_bound(samples_.begin(), samples_.end(), day,
                                 [](const matter_reading& s, double d) { return s.day < d; });
    if (next == samples_.begin())
        return 0;
    if (next == samples_.end())
        return samples_.size() - 1;
    const auto index = static_cast<std::size_t>(next - samples_.begin());
    return day - samples_[index - 1].day < next->day - day ? index - 1 : index;
}
double matter_timeline::ceiling() const {
    double maximum = 1;
    for (const auto& sample : samples_) {
        double total = 0;
        for (double value : sample.nitrogen)
            total += value;
        maximum = std::max(maximum, total);
    }
    const double unit = std::pow(10., std::floor(std::log10(maximum)));
    for (double step : {1., 2., 5., 10.})
        if (unit * step >= maximum)
            return unit * step;
    return maximum;
}
}
