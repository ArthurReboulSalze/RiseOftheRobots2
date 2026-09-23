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
