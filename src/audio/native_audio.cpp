#include "native_audio.hpp"
namespace terrarium {
native_audio::native_audio(bool enabled) {
    if (!enabled || !SDL_InitSubSystem(SDL_INIT_AUDIO))
        return;
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, ambient_mixer::sample_rate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (stream_)
        SDL_ResumeAudioStreamDevice(stream_);
}
native_audio::~native_audio() {
    if (stream_)
        SDL_DestroyAudioStream(stream_);
}
void native_audio::update(audio_environment environment, audio_settings settings, bool focused) {
    if (!stream_)
        return;
    if (!focused) {
        SDL_ClearAudioStream(stream_);
        return;
    }
    constexpr int block_bytes = sizeof(samples_);
    int queued = SDL_GetAudioStreamQueued(stream_);
    if (queued < 0)
        return;
    for (int i = 0; i < 3 && queued < block_bytes * 2; ++i) {
        mixer_.render(samples_, environment, settings);
        if (!SDL_PutAudioStreamData(stream_, samples_.data(), block_bytes))
            break;
        queued += block_bytes;
    }
}
}
