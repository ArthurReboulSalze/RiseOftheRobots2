#pragma once

#include "SDL.h"
#include <string>

class UiFont {
public:
    UiFont(SDL_Renderer* renderer, const std::string& assets_dir);
    ~UiFont();
    UiFont(const UiFont&) = delete;
    UiFont& operator=(const UiFont&) = delete;

    void draw(const std::string& text, int x, int y, SDL_Color color,
              int cell_w = 16, int cell_h = 20, bool centered = false) const;

private:
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
};

// The original CHRSET banks: fixed 6, 12 and 24-pixel cells, ASCII 32..127.
// Kept separate from the provisional menu font so existing non-combat screens
// retain their layout while the combat HUD and pause menu use source artwork.
class GameFont {
public:
    GameFont(SDL_Renderer* renderer, const std::string& assets_dir);
    ~GameFont();
    GameFont(const GameFont&) = delete;
    GameFont& operator=(const GameFont&) = delete;
    void draw(const std::string& text, int x, int y, int size,
              SDL_Color color, bool centered = false) const;
private:
    SDL_Renderer* renderer_;
    SDL_Texture* textures_[3]{};
};

void draw_combat_hud(SDL_Renderer* renderer, const GameFont& font,
                     const std::string& left_name, const std::string& right_name,
                     int left_health, int right_health, int left_super, int right_super,
                     int seconds, bool left_flash, bool right_flash,
                     int left_power_mask, int right_power_mask);
