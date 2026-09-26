#pragma once
#include "filters.h"
#include "SDL.h"

class Presentation {
public:
    explicit Presentation(SDL_Renderer* renderer);
    ~Presentation();
    void begin();
    void present(DisplayFilter filter,bool changed);
private:
    SDL_Renderer* renderer_;
    SDL_Texture* scene_=nullptr;
    SDL_Texture* filtered_=nullptr;
    DisplayFilter previous_=DisplayFilter::Count;
    int scale_=0;
    std::vector<uint32_t> pixels_;
};
