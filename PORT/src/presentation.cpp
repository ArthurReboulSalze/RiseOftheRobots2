#include "presentation.h"
#include <stdexcept>

Presentation::Presentation(SDL_Renderer* renderer) : renderer_(renderer),pixels_(640*400) {
    scene_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,640,400);
    if (!scene_) throw std::runtime_error(SDL_GetError());
    SDL_SetTextureBlendMode(scene_,SDL_BLENDMODE_NONE);
}
Presentation::~Presentation() {SDL_DestroyTexture(filtered_);SDL_DestroyTexture(scene_);}
void Presentation::begin() {
    if (SDL_SetRenderTarget(renderer_,scene_)<0) throw std::runtime_error(SDL_GetError());
    SDL_RenderSetLogicalSize(renderer_,640,400);
}
void Presentation::present(DisplayFilter filter,bool changed) {
    SDL_Texture* output=scene_;
    if (filter_scale(filter)>1) {
        const int scale=filter_scale(filter);
        if (!filtered_ || scale_!=scale) {
            SDL_DestroyTexture(filtered_);
            filtered_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640*scale,400*scale);
            if (!filtered_) throw std::runtime_error(SDL_GetError());
            SDL_SetTextureBlendMode(filtered_,SDL_BLENDMODE_NONE); scale_=scale; changed=true;
        }
        if (changed || previous_!=filter) {
            if (SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels_.data(),640*4)<0)
                throw std::runtime_error(SDL_GetError());
            const auto filtered=filter_pixels(pixels_.data(),640,400,filter);
            if (SDL_UpdateTexture(filtered_,nullptr,filtered.data(),640*scale*4)<0)
                throw std::runtime_error(SDL_GetError());
        }
        output=filtered_;
    }
    previous_=filter;
    SDL_SetTextureScaleMode(output,filter==DisplayFilter::Bilinear || filter==DisplayFilter::Xbr ||
                            filter==DisplayFilter::Crt ? SDL_ScaleModeLinear:SDL_ScaleModeNearest);
    SDL_SetRenderTarget(renderer_,nullptr);
    SDL_RenderSetLogicalSize(renderer_,640,400);
    SDL_RenderSetIntegerScale(renderer_,filter==DisplayFilter::Nearest ? SDL_TRUE:SDL_FALSE);
    SDL_SetRenderDrawColor(renderer_,0,0,0,255); SDL_RenderClear(renderer_);
    SDL_Rect target{0,0,640,400}; SDL_RenderCopy(renderer_,output,nullptr,&target);
    SDL_RenderPresent(renderer_);
}
