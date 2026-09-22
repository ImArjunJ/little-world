#pragma once
#include "sengine/audio.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

namespace terrarium {

class footstep_audio {
  public:
    explicit footstep_audio(bool enabled, const std::filesystem::path& root) : stream_(enabled) {
        if (!stream_)
            return;
        for (int surface = 0; surface < 2; ++surface)
            for (int i = 0; i < 5; ++i) {
                auto path = root / (std::string(surface ? "footstep_wood_00" : "footstep_grass_00") +
                                    std::to_string(i) + ".wav");
                auto clip = sengine::load_audio(path);
                if (clip.format.channels == 2 && clip.format.rate == 48000)
                    samples_[surface][i] = std::move(clip.samples);
            }
    }
    void volume(float gain) { stream_.volume(gain); }
    void clear() { stream_.clear(); }
    void step(bool indoors, float strength = .22f) {
        if (!stream_)
            return;
        if (stream_.queued_samples() > 48000 * 2 * .12f)
            clear();
        sequence_ = (sequence_ + 2) % 5;
        auto data = samples_[indoors ? 1 : 0][sequence_];
        for (float& sample : data)
            sample *= strength * (.94f + sequence_ * .025f);
        if (!data.empty())
            stream_.write(data);
    }

  private:
    sengine::audio_stream stream_;
    std::array<std::array<std::vector<float>, 5>, 2> samples_;
    unsigned sequence_{};
};
}
