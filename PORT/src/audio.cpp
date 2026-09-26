#include "audio.h"
#include "SDL.h"
#include "SDL_mixer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>

std::vector<int16_t> resample_pcm_u8(const uint8_t* samples, size_t count,
                                   int source_rate, int output_rate,
                                   int channels, double gain) {
    if (source_rate < 1000 || source_rate > 192000 ||
        output_rate < 1000 || output_rate > 192000 ||
        (channels != 1 && channels != 2) || !std::isfinite(gain) || gain < 0 || gain > 1) {
        throw std::invalid_argument("Invalid PCM resampling parameters");
    }
    if (count == 0) return {};
    if (!samples) throw std::invalid_argument("Missing PCM samples");
    if (count > std::numeric_limits<uint32_t>::max()) {
        throw std::length_error("PCM input is too large");
    }
    const uint64_t frames = (uint64_t(count) * output_rate + source_rate - 1) / source_rate;
    if (frames * channels > std::numeric_limits<uint32_t>::max() / sizeof(int16_t)) {
        throw std::length_error("PCM output is too large");
    }

    constexpr int radius = 16, taps = radius * 2;
    constexpr double pi = 3.14159265358979323846;
    const int divisor = std::gcd(source_rate, output_rate);
    const int phases = output_rate / divisor;
    const double cutoff = std::min(1.0, double(output_rate) / source_rate);
    std::vector<std::array<double, taps>> kernels(phases);
    for (int phase = 0; phase < phases; ++phase) {
        double sum = 0;
        for (int tap = 0; tap < taps; ++tap) {
            const double x = double(tap - radius + 1) - double(phase) / phases;
            const double t = pi * x * cutoff;
            const double sinc = std::abs(t) < 1e-12 ? 1.0 : std::sin(t) / t;
            const double window = std::abs(x) >= radius ? 0.0 :
                0.42 + 0.5 * std::cos(pi * x / radius) + 0.08 * std::cos(2 * pi * x / radius);
            sum += kernels[phase][tap] = cutoff * sinc * window;
        }
        for (double& weight : kernels[phase]) weight /= sum;
    }

    std::vector<int16_t> pcm(size_t(frames) * channels);
    for (uint64_t frame = 0; frame < frames; ++frame) {
        const uint64_t position = frame * source_rate;
        const int64_t center = int64_t(position / output_rate);
        const auto& kernel = kernels[size_t((position % output_rate) / divisor)];
        double value = 0;
        for (int tap = 0; tap < taps; ++tap) {
            const int64_t index = center + tap - radius + 1;
            if (index >= 0 && uint64_t(index) < count) {
                value += (int(samples[size_t(index)]) - 128) * kernel[tap];
            }
        }
        const auto sample = int16_t(std::clamp(std::lround(value * 256 * gain), -32768L, 32767L));
        for (int channel = 0; channel < channels; ++channel) {
            pcm[size_t(frame) * channels + channel] = sample;
        }
    }
    return pcm;
}

Mix_Chunk* load_effect_wav(const char* path, double gain) {
    SDL_AudioSpec source{};
    Uint8* bytes = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(path, &source, &bytes, &length)) return nullptr;
    std::unique_ptr<Uint8, decltype(&SDL_FreeWAV)> wav(bytes, SDL_FreeWAV);
    int output_rate = 0, channels = 0;
    Uint16 format = 0;
    if (!Mix_QuerySpec(&output_rate, &format, &channels)) return nullptr;
    if (source.format != AUDIO_U8 || source.channels != 1 || format != AUDIO_S16SYS ||
        (channels != 1 && channels != 2)) {
        Mix_Chunk* chunk = Mix_LoadWAV(path);
        if (chunk) Mix_VolumeChunk(chunk, int(std::lround(MIX_MAX_VOLUME * gain)));
        return chunk;
    }
    // Apply gain before conversion, leaving headroom for reconstructed peaks.
    const auto pcm = resample_pcm_u8(bytes, length, source.freq, output_rate, channels, gain);
    if (pcm.empty()) return nullptr;
    const size_t size = pcm.size() * sizeof(int16_t);
    auto* buffer = static_cast<Uint8*>(SDL_malloc(size));
    if (!buffer) return nullptr;
    std::memcpy(buffer, pcm.data(), size);
    Mix_Chunk* chunk = Mix_QuickLoad_RAW(buffer, Uint32(size));
    if (!chunk) { SDL_free(buffer); return nullptr; }
    chunk->allocated = 1; // Mix_FreeChunk owns the SDL-allocated PCM buffer.
    return chunk;
}
