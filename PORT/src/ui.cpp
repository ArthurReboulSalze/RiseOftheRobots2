#include "ui.h"
#include "SDL_image.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

UiFont::UiFont(SDL_Renderer* renderer, const std::string& assets_dir)
    : renderer_(renderer), texture_(nullptr) {
    const std::string path = assets_dir + "/ui/font.png";
    texture_ = IMG_LoadTexture(renderer_, path.c_str());
    if (!texture_) throw std::runtime_error("UI font not found: " + path);
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
}

UiFont::~UiFont() {
    SDL_DestroyTexture(texture_);
}

void UiFont::draw(const std::string& text, int x, int y, SDL_Color color,
                  int cell_w, int cell_h, bool centered) const {
    if (centered) x -= static_cast<int>(text.size()) * cell_w / 2;
    SDL_SetTextureColorMod(texture_, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture_, color.a);
    for (unsigned char ch : text) {
        if (ch >= 32 && ch <= 126) {
            const int index = ch - 32;
            SDL_Rect src{(index % 16) * 16, (index / 16) * 20, 16, 20};
            SDL_Rect dst{x, y, cell_w, cell_h};
            SDL_RenderCopy(renderer_, texture_, &src, &dst);
        }
        x += cell_w;
    }
}

GameFont::GameFont(SDL_Renderer* renderer, const std::string& assets_dir)
    : renderer_(renderer) {
    for (int i = 0; i < 3; ++i) {
        const std::string path = assets_dir + "/ui/charset" + std::to_string(i + 1) + ".png";
        textures_[i] = IMG_LoadTexture(renderer_, path.c_str());
        if (!textures_[i]) {
            for (auto* texture : textures_) SDL_DestroyTexture(texture);
            for (auto& texture : textures_) texture = nullptr;
            throw std::runtime_error("Original CHRSET font not found: " + path +
                                     "; import the game again");
        }
        SDL_SetTextureBlendMode(textures_[i], SDL_BLENDMODE_BLEND);
    }
}

GameFont::~GameFont() {
    for (auto* texture : textures_) SDL_DestroyTexture(texture);
}

void GameFont::draw(const std::string& text, int x, int y, int size,
                    SDL_Color color, bool centered) const {
    const int bank = size == 6 ? 0 : size == 12 ? 1 : size == 24 ? 2 : -1;
    if (bank < 0) throw std::invalid_argument("Unsupported CHRSET cell size");
    SDL_Texture* texture = textures_[bank];
    if (centered) x -= int(text.size()) * size / 2;
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);
    for (unsigned char ch : text) {
        if (ch >= 32 && ch <= 127) {
            const int index = ch - 32;
            SDL_Rect src{(index % 16) * size, (index / 16) * size, size, size};
            SDL_Rect dst{x, y, size, size};
            SDL_RenderCopy(renderer_, texture, &src, &dst);
        }
        x += size;
    }
}

void draw_combat_hud(SDL_Renderer* renderer, const GameFont& font,
                     const std::string& left_name, const std::string& right_name,
                     int left_health, int right_health, int left_super, int right_super,
                     int seconds, bool left_flash, bool right_flash,
                     int left_power_mask, int right_power_mask) {
    auto fill = [&](int x, int y, int w, int h, SDL_Color color) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Rect rect{x, y, w, h};
        SDL_RenderFillRect(renderer, &rect);
    };
    constexpr SDL_Color blue{160, 183, 255, 255};
    constexpr SDL_Color orange{255, 73, 26, 255};
    constexpr SDL_Color dark{4, 5, 13, 255};
    constexpr SDL_Color frame{163, 165, 180, 255};
    constexpr SDL_Color green{45, 235, 24, 255};
    font.draw(left_name, 40, 1, 12, blue);
    font.draw(right_name, 600 - int(right_name.size()) * 12, 1, 12, blue);
    font.draw("00000000", 200, 1, 12, blue);
    font.draw("00000000", 344, 1, 12, blue);
    char timer[3];
    std::snprintf(timer, sizeof(timer), "%02d", std::clamp(seconds, 0, 99));
    font.draw(timer, 320, 0, 24, orange, true);

    const int left = std::clamp(left_health, 0, 120) * 254 / 120;
    const int right = std::clamp(right_health, 0, 120) * 254 / 120;
    for (const int x : {39, 343}) {
        fill(x, 15, 258, 10, dark);
        fill(x, 15, 258, 1, frame);
        fill(x, 24, 258, 1, frame);
    }
    fill(40, 17, left, 6, left_flash ? orange : green);
    fill(599 - right, 17, right, 6, right_flash ? orange : green);
    // FUN_16d4e displays each owned power-mask bit as CHRSET character 0x76+i.
    int left_count=0,right_count=0;
    for (int index=0;index<6;++index) {
        if (left_power_mask & (1<<index))
            font.draw(std::string(1, char(118+index)),39+left_count++*24,26,24,
                      SDL_Color{255,255,255,255});
        if (right_power_mask & (1<<index))
            font.draw(std::string(1, char(118+index)),577-right_count++*24,26,24,
                      SDL_Color{255,255,255,255});
    }

    // Original super indicators occupy narrow strips at the lower corners.
    for (const int x : {39, 537}) {
        fill(x, 382, 66, 8, dark);
        fill(x, 382, 66, 1, frame);
        fill(x, 389, 66, 1, frame);
    }
    fill(40, 384, std::clamp(left_super, 0, 24) * 63 / 24, 4,
         SDL_Color{253, 232, 91, 255});
    const int right_meter = std::clamp(right_super, 0, 24) * 63 / 24;
    fill(601 - right_meter, 384, right_meter, 4, SDL_Color{253, 232, 91, 255});
}
