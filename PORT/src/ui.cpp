#include "ui.h"
#include "SDL_image.h"
#include <stdexcept>

UiFont::UiFont(SDL_Renderer* renderer, const std::string& assets_dir)
    : renderer_(renderer), texture_(nullptr) {
    const std::string path = assets_dir + "/ui/font.png";
    texture_ = IMG_LoadTexture(renderer_, path.c_str());
    if (!texture_) throw std::runtime_error("police UI introuvable: " + path);
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
