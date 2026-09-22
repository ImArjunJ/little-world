#include "native_audio.hpp"
namespace terrarium {
native_audio::native_audio(bool enabled) : stream_(enabled, {ambient_mixer::sample_rate, 2}) {}
void native_audio::update(audio_environment environment, audio_settings settings, bool focused) {
    if (!stream_)
        return;
    if (!focused) {
        stream_.clear();
        return;
    }
    constexpr int block_samples = std::tuple_size_v<decltype(samples_)>;
    int queued = stream_.queued_samples();
    if (queued < 0)
        return;
    for (int i = 0; i < 3 && queued < block_samples * 2; ++i) {
        mixer_.render(samples_, environment, settings);
        if (!stream_.write(samples_))
            break;
        queued += block_samples;
    }
}
}
