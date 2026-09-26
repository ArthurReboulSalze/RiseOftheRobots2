#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct Mix_Chunk;

// Windowed-sinc conversion of original unsigned PCM. No source file is changed.
std::vector<int16_t> resample_pcm_u8(const uint8_t* samples, size_t count,
                                   int source_rate, int output_rate,
                                   int channels, double gain = 1.0);

// The returned chunk belongs to the caller and is released with Mix_FreeChunk.
Mix_Chunk* load_effect_wav(const char* path, double gain = 1.0);
