#pragma once
#include "settings.h"
#include <cstdint>
#include <vector>

// Opaque, composited ARGB8888 pixels. Border samples clamp to the image edge.
int filter_scale(DisplayFilter filter);
std::vector<uint32_t> filter_pixels(const uint32_t* source, int width, int height, DisplayFilter filter);
