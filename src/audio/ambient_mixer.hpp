#pragma once
#include <array>
#include <cstdint>
#include <span>
namespace terrarium {
struct audio_settings {
    bool enabled{true};
    float master{.45f}, forest{.55f}, water{.40f}, rain{.55f};
};
struct audio_environment {
    float pond{};
    bool raining{};
};

class ambient_mixer {
  public:
    static constexpr int sample_rate = 48000;
    void render(std::span<float> stereo, audio_environment environment, audio_settings settings);
    std::uint64_t frames() const { return sample_; }

  private:
    std::uint32_t random_{0x9e3779b9};
    std::uint64_t sample_{};
    float brown_{}, pink_{}, gain_{}, rain_gain_{}, forest_gain_{}, water_gain_{}, drop_{},
        drop_frequency_{1800};
    double drop_phase_{}, bird_phase_{};
    float noise();
};
}
