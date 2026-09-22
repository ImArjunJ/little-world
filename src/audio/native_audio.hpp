#pragma once
#include "ambient_mixer.hpp"
#include "sengine/audio.hpp"
#include <array>
namespace terrarium {
class native_audio {
  public:
    explicit native_audio(bool enabled);
    void update(audio_environment, audio_settings, bool focused);

  private:
    sengine::audio_stream stream_;
    ambient_mixer mixer_;
    std::array<float, 2048> samples_{};
};
}
