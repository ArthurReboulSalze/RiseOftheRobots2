// Exercise the real C++ loaders and every selectable robot in a private profile.
#include "assets.h"
#include "fighter.h"
#include "combat.h"
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
        const auto* extra = assets.load_atlas("EXTRA");
        const auto* extra_cl2 = assets.load_cl2("EXTRA");
        const auto* combat_data = assets.load_combat();
        Combat combat(combat_data,extra,extra_cl2);
        int commands_checked=0, unsupported=0;
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
            fighter.robot_id = slot>='A' && slot<='Z' ? slot-'A' : 26+slot-'0';
            fighter.step(0);
            if (!fighter.current_atlas_frame()) throw std::runtime_error("No idle frame: " + bank);
            const auto* moves = fighter.mvs;
            for (const auto& c : moves->commands) {
                if (!command_reachable(c) || !fighter.has_action(c.target) || (c.target>=48 && c.target<=58 && !(c.target&1) &&
                                   !combat.usable_finisher(fighter,c.target))) {++unsupported;continue;}
                Fighter command, opponent;
                command.mvs=moves; command.robot_id=fighter.robot_id;
                command.cl2=fighter.cl2;
                command.super_meter=c.target==88 ? 24 : 0;
                command.stolen_moves=c.target>=90 ? 1<<(c.target-90) : 0;
                command.x=250; opponent.x=300; opponent.robot_id=6; opponent.health=30;
                if (moves->moves[c.target].ground_mode==2) command.y=200;
                command.step(0,&opponent,true);
                uint16_t latest=0;
                for (auto it=c.inputs.rbegin();it!=c.inputs.rend();++it) {
                    latest=*it==254 ? 0 : *it;
                    command.sample_inputs(latest);
                }
                command.step(latest,&opponent,true);
                if (command.move_id!=c.target) {
                    // Native scripts are ordered. A shorter earlier command
                    // can mask a later one (verified on the user's RBTY bank).
                    bool priority=false;
                    for (const auto& earlier : moves->commands) {
                        if (&earlier==&c) break;
                        if (earlier.target!=command.move_id || earlier.inputs.size()>c.inputs.size()) continue;
                        bool prefix=true;
                        for (size_t i=0;i<earlier.inputs.size();++i)
                            if (earlier.inputs[i]!=254 && earlier.inputs[i]!=(c.inputs[i]==254 ? 0 : c.inputs[i])) prefix=false;
                        priority|=prefix;
                    }
                    if (priority) {++unsupported;continue;}
                    throw std::runtime_error("Command mismatch: "+bank+" target="+std::to_string(c.target)+
                                             " actual="+std::to_string(command.move_id));
                }
                ++commands_checked;
            }
            for (const auto& movement : moves->moves) {
                auto validate = [&](const EffectScript& script) {
                    for (const auto& f : script) {
                        if (f.image<0 || f.image==1000) continue;
                        const int limit=f.flags&2 ? (int)extra->frames.size() : (int)fighter.atlas->frames.size()-1;
                        if (f.image>=limit) throw std::runtime_error("Out of range FX: "+bank);
                    }
                };
                for (const auto& s : movement.attached) validate(s);
                validate(movement.projectile); validate(movement.impact);
            }
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
        std::printf("%d executable commands and all FX frame references checked; %d placeholder/unreachable/shadowed commands retained.\n",
                    commands_checked,unsupported);
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
