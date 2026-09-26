#include "settings.h"
#include "json.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>

const char* filter_name(DisplayFilter filter) {
    static const char* names[] = {"ORIGINAL PIXELS", "BILINEAR", "SCALE2X", "SCALE3X", "XBR SMOOTH", "CRT SOFT"};
    const int index = static_cast<int>(filter);
    return index >= 0 && index < static_cast<int>(DisplayFilter::Count) ? names[index] : names[0];
}

void Settings::load(const std::string& path) {
    *this = Settings{};
    std::ifstream file(path);
    if (!file) return;
    try {
        const auto json = nlohmann::json::parse(file);
        if (!json.is_object()) return;
        auto volume = [&](const char* key) {
            return json.contains(key) && json[key].is_number_integer()
                   ? std::clamp(json[key].get<int>(), 0, 100) : 100;
        };
        music_volume = volume("music_volume");
        game_volume = volume("game_volume");
        if (json.contains("easy_finishings") && json["easy_finishings"].is_boolean())
            easy_finishings = json["easy_finishings"].get<bool>();
        if (json.contains("filter") && json["filter"].is_number_integer()) {
            const int value = json["filter"].get<int>();
            if (value >= 0 && value < static_cast<int>(DisplayFilter::Count)) filter = static_cast<DisplayFilter>(value);
        }
    } catch (const nlohmann::json::exception&) { *this = Settings{}; }
}

bool Settings::save(const std::string& path) const {
    std::error_code error;
    const auto parent=std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    std::ofstream file(path);
    if (!file) return false;
    file << nlohmann::json{{"music_volume",music_volume},{"game_volume",game_volume},
                          {"easy_finishings",easy_finishings},{"filter",static_cast<int>(filter)}}.dump(2) << '\n';
    return bool(file);
}
