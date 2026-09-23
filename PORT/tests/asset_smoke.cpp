// Exercise the real C++ loaders and every selectable robot in a private profile.
#include "assets.h"
#include "fighter.h"
#include "frontend.h"
#include "SDL_image.h"
#include <cstdio>
#include <set>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: rotr2_asset_smoke <profile/EXTRACTED>\n");
        return 2;
    }
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) || !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) return 1;
    SDL_Window* window = SDL_CreateWindow("Asset check", 0, 0, 640, 400, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    int result = 0;
    try {
        if (!window || !renderer) throw std::runtime_error(SDL_GetError());
        Assets assets(renderer, argv[1]);
        Frontend frontend(renderer, assets, argv[1]);
        frontend.render(); // intro
        frontend.key(SDLK_RETURN);
        frontend.render(); // title
        frontend.key(SDLK_RETURN);
        const int count = static_cast<int>(assets.load_atlas("VSFACE")->frames.size());
        std::set<char> selected;
        for (int i = 0; i < count; ++i) {
            const char slot = frontend.player(0).slot;
            selected.insert(slot);
            frontend.render(); // both portrait mappings, including bonus slots
            Assets robot_assets(renderer, argv[1]); // free each robot's textures between checks
            Fighter fighter;
            const std::string bank = std::string("RBT") + slot;
            fighter.set_banks(robot_assets.load_atlas(bank), robot_assets.load_mvs(bank));
            fighter.cl2 = robot_assets.load_cl2(std::string("R") + slot);
            fighter.step(0);
            if (!fighter.current_atlas_frame()) throw std::runtime_error("No idle frame: " + bank);
            frontend.key(SDLK_RIGHT);
            frontend.key(SDLK_d);
        }
        if (static_cast<int>(selected.size()) != count) throw std::runtime_error("Roster did not cycle completely");
        if (count == 30 && (!selected.count('2') || !selected.count('3')))
            throw std::runtime_error("Bonus robots unavailable");
        frontend.key(SDLK_RETURN);
        if (!frontend.take_match_request()) throw std::runtime_error("Match not requested");
        std::printf("Intro, title, %d portraits and robot banks loaded successfully.\n", count);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return result;
}
