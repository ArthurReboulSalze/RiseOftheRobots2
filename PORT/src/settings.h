#pragma once
#include <string>

enum class DisplayFilter { Nearest, Bilinear, Scale2x, Scale3x, Xbr, Crt, Count };
const char* filter_name(DisplayFilter filter);

struct Settings {
    int music_volume = 100;
    int game_volume = 100;
    bool easy_finishings = false;
    DisplayFilter filter = DisplayFilter::Nearest;
    void load(const std::string& path);
    bool save(const std::string& path) const;
};
