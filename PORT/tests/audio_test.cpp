#define SDL_MAIN_HANDLED
#include "audio.h"
#include "SDL.h"
#include "SDL_mixer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static std::vector<uint8_t> tone(int rate, double frequency) {
    std::vector<uint8_t> pcm(rate);
    for (int i = 0; i < rate; ++i) {
        pcm[i] = uint8_t(std::lround(128 + 60 * std::sin(2 * 3.141592653589793 * frequency * i / rate)));
    }
    return pcm;
}

static double amplitude(const std::vector<int16_t>& pcm, int rate, double frequency) {
    double real = 0, imaginary = 0, weights = 0;
    for (size_t i = 64; i + 64 < pcm.size(); ++i) {
        const double weight = 0.5 - 0.5 * std::cos(2 * 3.141592653589793 * i / pcm.size());
        const double phase = 2 * 3.141592653589793 * frequency * i / rate;
        real += pcm[i] * weight * std::cos(phase);
        imaginary += pcm[i] * weight * std::sin(phase);
        weights += weight;
    }
    return 2 * std::hypot(real, imaginary) / weights;
}

int main(int argc, char** argv) {
    std::vector<uint8_t> silence(11025, 128);
    auto stereo = resample_pcm_u8(silence.data(), silence.size(), 11025, 44100, 2);
    require(stereo.size() == 44100 * 2, "Wrong output duration/channel count");
    require(std::all_of(stereo.begin(), stereo.end(), [](int16_t s) { return s == 0; }),
            "Unsigned PCM silence must remain silent");

    auto source = tone(11025, 1000);
    auto mono = resample_pcm_u8(source.data(), source.size(), 11025, 44100, 1);
    const double fundamental = amplitude(mono, 44100, 1000);
    require(std::abs(fundamental / (60 * 256) - 1) < 0.01, "Tone level/pitch changed");
    require(amplitude(mono, 44100, 10025) / fundamental < 0.002,
            "Resampling leaves an audible spectral image");

    stereo = resample_pcm_u8(source.data(), source.size(), 11025, 48000, 2, 0.25);
    require(stereo.size() == 48000 * 2, "48 kHz device conversion has wrong duration");
    for (size_t i = 0; i < stereo.size(); i += 2) {
        require(stereo[i] == stereo[i + 1], "Mono source must reach both output channels equally");
    }
    auto quiet = resample_pcm_u8(source.data(), source.size(), 11025, 44100, 1, 0.25);
    require(std::abs(amplitude(quiet, 44100, 1000) / fundamental - 0.25) < 0.001,
            "Original SOS gain was not preserved");
    source = tone(44100, 14000);
    mono = resample_pcm_u8(source.data(), source.size(), 44100, 11025, 1);
    require(amplitude(mono, 11025, 2975) < 60 * 256 * 0.01, "Downsampling aliases high frequencies");
    require(resample_pcm_u8(nullptr, 0, 11025, 44100, 2).empty(), "Empty PCM should be accepted");
    bool rejected = false;
    try { resample_pcm_u8(nullptr, 1, 11025, 44100, 2); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Missing PCM input was accepted");

    // Exercise the actual SDL_mixer chunk ownership/conversion with a synthetic WAV.
    SDL_SetMainReady();
    require(SDL_Init(SDL_INIT_AUDIO) == 0, SDL_GetError());
    require(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 512) == 0, Mix_GetError());
    const auto path = std::filesystem::temp_directory_path() /
        ("rotr2-audio-test-" + std::to_string(SDL_GetPerformanceCounter()) + ".wav");
    source = tone(11025, 1000);
    {
        std::ofstream file(path, std::ios::binary);
        auto word = [&](uint32_t value, int bytes) {
            for (int i = 0; i < bytes; ++i) file.put(char((value >> (8 * i)) & 255));
        };
        file.write("RIFF", 4); word(36 + uint32_t(source.size()) + 1, 4); file.write("WAVEfmt ", 8);
        word(16, 4); word(1, 2); word(1, 2); word(11025, 4); word(11025, 4);
        word(1, 2); word(8, 2); file.write("data", 4); word(uint32_t(source.size()), 4);
        file.write(reinterpret_cast<const char*>(source.data()), source.size()); file.put(0);
        require(bool(file), "Unable to create the synthetic WAV");
    }
    Mix_Chunk* chunk = load_effect_wav(path.string().c_str(), 0.25);
    require(chunk && chunk->allocated && chunk->alen == 44100 * 2 * sizeof(int16_t),
            "Actual WAV conversion failed or the mixer does not own its buffer");
    require(Mix_PlayChannel(-1, chunk, 0) >= 0, Mix_GetError());
    Mix_HaltChannel(-1);
    Mix_FreeChunk(chunk);
    std::filesystem::remove(path);
    for (int i = 1; i < argc; ++i) {
        chunk = load_effect_wav(argv[i], double(0x2000) / 0x7fff);
        require(chunk && chunk->alen, argv[i]);
        require(Mix_PlayChannel(-1, chunk, 0) >= 0, Mix_GetError());
        Mix_HaltChannel(-1);
        Mix_FreeChunk(chunk);
    }
    if (argc > 1) std::printf("Loaded and queued %d private effect WAVs\n", argc - 1);
    Mix_CloseAudio(); SDL_Quit();
    std::puts("Audio: rate, gain, spectral images, stereo and chunk playback passed");
}
