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
            const auto* moves = fighter.mvs;
            for (uint16_t input : {IN_PUNCH, IN_KICK}) {
                Fighter attack;
                attack.set_banks(fighter.atlas, moves);
                int entries = 0;
                for (int tick = 0; tick < 100; ++tick) {
                    const int target = attack.step(input);
                    if (target == 8 || target == 9) ++entries;
                    if (!attack.current_atlas_frame())
                        throw std::runtime_error("Invisible held attack: " + bank);
                }
                if (entries != 1) throw std::runtime_error("Held attack repeated: " + bank);
            }
            bool supports_jump = false;
            for (const auto& transition : moves->moves[0].transitions)
                if (transition.mask == IN_UP) supports_jump = true;
            if (supports_jump) {
                Fighter jump;
                jump.set_banks(fighter.atlas, moves);
                int takeoffs = 0;
                bool was_airborne = false;
                for (int tick = 0; tick < 100; ++tick) {
                    jump.step(IN_UP);
                    if (jump.airborne() && !was_airborne) ++takeoffs;
                    was_airborne = jump.airborne();
                    if (!jump.current_atlas_frame())
                        throw std::runtime_error("Invisible jump: " + bank);
                }
                if (takeoffs != 1 || jump.airborne() || jump.y != jump.ground_y)
                    throw std::runtime_error("Jump repeated or failed to land: " + bank);

                for (int direction : {1, -1}) {
                    Fighter flying, control, opponent;
                    for (Fighter* f : {&flying, &control, &opponent})
                        f->set_banks(fighter.atlas, moves);
                    flying.x = control.x = direction > 0 ? 280 : 320;
                    flying.facing = control.facing = direction;
                    opponent.x = 300;
                    opponent.facing = -direction;
                    const uint16_t input = IN_UP | (direction > 0 ? IN_RIGHT : IN_LEFT);
                    int turns = 0;
                    bool took_off = false;
                    for (int tick = 0; tick < 100; ++tick) {
                        const int previous_facing = flying.facing;
                        update_facing(flying, opponent);
                        if (previous_facing != flying.facing) ++turns;
                        flying.step(input);
                        control.step(input);
                        if (flying.y != control.y || !flying.current_atlas_frame())
                            throw std::runtime_error("Crossing changed jump trajectory: " + bank);
                        const auto* motion = flying.move();
                        if (!motion->movements[0].empty()) {
                            const int delta = motion->movements[0][flying.seq_pos] * 2 * flying.facing;
                            if (delta * direction < 0)
                                throw std::runtime_error("Crossing reversed travel: " + bank);
                            flying.x += delta;
                        }
                        took_off |= flying.airborne();
                        if (took_off && !flying.airborne()) break;
                    }
                    if (!took_off || flying.airborne() || turns != 1 ||
                        (flying.x - opponent.x) * direction <= 0)
                        throw std::runtime_error("Diagonal jump failed to cross once: " + bank);
                }
            }
            frontend.key(SDLK_RIGHT);
            frontend.key(SDLK_d);
        }
        if (static_cast<int>(selected.size()) != count) throw std::runtime_error("Roster did not cycle completely");
        if (count == 30 && (!selected.count('2') || !selected.count('3')))
            throw std::runtime_error("Bonus robots unavailable");
        frontend.key(SDLK_RETURN);
        if (!frontend.take_match_request()) throw std::runtime_error("Match not requested");
        std::printf("Intro, title, %d robot banks, held attacks, jumps and both crossing directions checked.\n", count);
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
