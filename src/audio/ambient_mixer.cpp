#include "ambient_mixer.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace terrarium {
namespace {
float unit(float x) {
    return std::isfinite(x) ? std::clamp(x, 0.f, 1.f) : 0.f;
}
void fade(float& value, float target) {
    value += (target - value) * .00035f;
    if (std::abs(target - value) < 1e-6f)
        value = target;
}
}
float ambient_mixer::noise() {
    random_ ^= random_ << 13;
    random_ ^= random_ >> 17;
    random_ ^= random_ << 5;
    return static_cast<float>(random_ >> 8) / 8388608.f - 1;
}
void ambient_mixer::render(std::span<float> stereo, audio_environment environment, audio_settings settings) {
    constexpr double tau = 2 * std::numbers::pi;
    const float desired = settings.enabled ? unit(settings.master) : 0;
    const float forest_level = unit(settings.forest), water_level = unit(settings.water);
    const float rain_level = environment.raining ? unit(settings.rain) : 0;
    const float pond = std::sqrt(unit(environment.pond));
    for (std::size_t i = 0; i + 1 < stereo.size(); i += 2) {
        double t = static_cast<double>(sample_++) / sample_rate;
        float white = noise();
        brown_ = (brown_ + .018f * white) * .995f;
        pink_ = .96f * pink_ + .04f * white;
        fade(gain_, desired);
        fade(forest_gain_, forest_level);
        fade(water_gain_, water_level);
        fade(rain_gain_, rain_level);
        float forest = (brown_ * .45f + pink_ * .16f) * forest_gain_;
        double cycle = std::fmod(t + 1.7, 11.3);
        double frequency = 2400 + 300 * std::sin(cycle * 19);
        bird_phase_ = std::fmod(bird_phase_ + tau * frequency / sample_rate, tau);
        float birds = 0;
        if (cycle < .6) {
            double envelope = std::sin(cycle / .6 * std::numbers::pi);
            birds = static_cast<float>(std::sin(bird_phase_) * envelope * envelope) * .007f * forest_gain_;
        }
        if (noise() > .9995f && drop_ < .01f) {
            drop_ = .08f;
            drop_frequency_ = 1100 + std::abs(noise()) * 1800;
            drop_phase_ = 0;
        }
        drop_phase_ = std::fmod(drop_phase_ + tau * drop_frequency_ / sample_rate, tau);
        drop_ *= .9982f;
        float water =
            (static_cast<float>(std::sin(drop_phase_)) * drop_ + pink_ * .055f) * water_gain_ * pond;
        float rain = (white * .035f + pink_ * .32f + brown_ * .15f) * rain_gain_;
        float mono = (forest + water + rain + birds) * gain_;
        float side = static_cast<float>(std::sin(t * .17)) * .18f;
        stereo[i] = std::clamp(mono * (1 + side), -.8f, .8f);
        stereo[i + 1] = std::clamp(mono * (1 - side), -.8f, .8f);
    }
    if (stereo.size() % 2)
        stereo.back() = 0;
}
}
