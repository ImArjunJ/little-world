#pragma once
#include "ambient_mixer.hpp"
#include <SDL3/SDL.h>
#include <array>
namespace terrarium {
class native_audio {
  public:
    explicit native_audio(bool enabled);
    ~native_audio();
    void update(audio_environment, audio_settings, bool focused);

  private:
    SDL_AudioStream* stream_{};
    ambient_mixer mixer_;
    std::array<float, 2048> samples_{};
};
}
