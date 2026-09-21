#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

namespace terrarium {

class footstep_audio {
  public:
    explicit footstep_audio(bool enabled, const std::filesystem::path& root) {
        if (!enabled || !SDL_InitSubSystem(SDL_INIT_AUDIO))
            return;
        for (int surface = 0; surface < 2; ++surface)
            for (int i = 0; i < 5; ++i) {
                auto path = root / (std::string(surface ? "footstep_wood_00" : "footstep_grass_00") +
                                    std::to_string(i) + ".wav");
                SDL_AudioSpec source{};
                Uint8* bytes = nullptr;
                Uint32 length = 0;
                if (!SDL_LoadWAV(path.c_str(), &source, &bytes, &length)) {
                    SDL_Log("Footstep sample unavailable: %s", path.c_str());
                    continue;
                }
                if (source.format == SDL_AUDIO_F32 && source.channels == 2 && source.freq == 48000)
                    samples_[surface][i].assign(reinterpret_cast<float*>(bytes),
                                                reinterpret_cast<float*>(bytes + length));
                SDL_free(bytes);
            }
        SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 48000};
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (stream_)
            SDL_ResumeAudioStreamDevice(stream_);
    }
    ~footstep_audio() {
        if (stream_)
            SDL_DestroyAudioStream(stream_);
    }
    void volume(float gain) {
        if (stream_)
            SDL_SetAudioStreamGain(stream_, gain);
    }
    void clear() {
        if (stream_)
            SDL_ClearAudioStream(stream_);
    }
    void step(bool indoors, float strength = .22f) {
        if (!stream_)
            return;
        if (SDL_GetAudioStreamQueued(stream_) > 48000 * 2 * int(sizeof(float)) * .12f)
            clear();
        sequence_ = (sequence_ + 2) % 5;
        auto data = samples_[indoors ? 1 : 0][sequence_];
        for (float& sample : data)
            sample *= strength * (.94f + sequence_ * .025f);
        if (!data.empty())
            SDL_PutAudioStreamData(stream_, data.data(), int(data.size() * sizeof(float)));
    }

  private:
    SDL_AudioStream* stream_{};
    std::array<std::array<std::vector<float>, 5>, 2> samples_;
    unsigned sequence_{};
};
}
