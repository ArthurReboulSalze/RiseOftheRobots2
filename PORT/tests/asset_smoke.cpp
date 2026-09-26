// Exercise the real C++ loaders and every selectable robot in a private profile.
#include "assets.h"
#include "fighter.h"
#include "combat.h"
#include "frontend.h"
#include "presentation.h"
#include "SDL_image.h"
#include <cstdio>
#include <set>
#include <stdexcept>
#include <filesystem>
#include <chrono>
#include <algorithm>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::fprintf(stderr, "Usage: rotr2_asset_smoke <profile/EXTRACTED> [capture_dir]\n");
        return 2;
    }
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) || !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) return 1;
    SDL_Window* window = SDL_CreateWindow("Asset check", 0, 0, 1280, 800, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    int result = 0;
    try {
        if (!window || !renderer) throw std::runtime_error(SDL_GetError());
        Assets assets(renderer, argv[1]);
        if (argc==3) std::filesystem::create_directories(argv[2]);
        auto capture=[&](const std::string& name) {
            if (argc!=3) return;
            SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormat(0,1280,800,32,SDL_PIXELFORMAT_ARGB8888);
            if (!surface || SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch)<0)
                throw std::runtime_error(SDL_GetError());
            const auto output=std::filesystem::path(argv[2])/(name+".png");
            if (IMG_SavePNG(surface,output.string().c_str())<0) throw std::runtime_error(SDL_GetError());
            SDL_FreeSurface(surface);
        };
        for (const auto key : {SDLK_F1,SDLK_F10,SDLK_F11,SDLK_F12})
            if (dos_to_sdl(sdl_to_dos(key))!=key || std::string(dos_key_name(sdl_to_dos(key)))!=SDL_GetKeyName(key))
                throw std::runtime_error("Function key names and bindings must use the same DOS scancodes");
        Frontend frontend(renderer, assets, argv[1]);
        const auto* extra = assets.load_atlas("EXTRA");
        const auto* extra_cl2 = assets.load_cl2("EXTRA");
        const auto* combat_data = assets.load_combat();
        Combat combat(combat_data,extra,extra_cl2);
        // Isolated controls/options: never change the user's imported profile.
        const auto test_dir=std::filesystem::temp_directory_path() /
            ("rise2-ui-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(test_dir/"ui");
        std::filesystem::copy_file(std::filesystem::path(argv[1])/"ui/font.png",test_dir/"ui/font.png");
        {
            Frontend menu(renderer,assets,test_dir.string());
            Presentation display(renderer);
            menu.key(SDLK_RETURN);menu.key(SDLK_DOWN);menu.key(SDLK_RETURN);
            if (menu.screen()!=Screen::Options) throw std::runtime_error("Title must open Options");
            menu.key(SDLK_LEFT); menu.key(SDLK_DOWN);
            for (int i=0;i<20;++i) menu.key(SDLK_LEFT);
            menu.key(SDLK_DOWN);menu.key(SDLK_RIGHT);menu.key(SDLK_DOWN);menu.key(SDLK_RIGHT);
            if (menu.settings().music_volume!=95 || menu.settings().game_volume!=0 ||
                !menu.settings().easy_finishings || menu.settings().filter!=DisplayFilter::Bilinear)
                throw std::runtime_error("Options controls did not change the intended settings");
            for (int i=0;i<static_cast<int>(DisplayFilter::Count);++i) {
                display.begin();menu.render();
                const auto start=SDL_GetPerformanceCounter();
                display.present(menu.settings().filter,true);
                const double ms=1000.*(SDL_GetPerformanceCounter()-start)/SDL_GetPerformanceFrequency();
                printf("Options filter %s: %.1f ms (software renderer)\n",filter_name(menu.settings().filter),ms);
                capture("options_filter_"+std::to_string(static_cast<int>(menu.settings().filter)));
                menu.key(SDLK_RIGHT);
            }
            menu.key(SDLK_DOWN);menu.key(SDLK_RETURN);
            if (menu.screen()!=Screen::KeyMapping) throw std::runtime_error("Key mapping must be inside Options");
            for (int i=0;i<4;++i) menu.key(SDLK_DOWN);
            menu.key(SDLK_RETURN);menu.key(SDLK_z);
            display.begin();menu.render();display.present(menu.settings().filter,true);capture("key_mapping");
            menu.key(SDLK_ESCAPE);
            if (menu.screen()!=Screen::Options) throw std::runtime_error("Key mapping must return to Options");
            menu.key(SDLK_ESCAPE);menu.key(SDLK_DOWN);menu.key(SDLK_RETURN);
            if (menu.screen()!=Screen::Player) throw std::runtime_error("Title must open Player");
            display.begin();menu.render();display.present(DisplayFilter::Nearest,true);capture("player");
            if (menu.movie_count()>0) {
                menu.key(SDLK_RETURN);
                if (menu.screen()!=Screen::Playback) throw std::runtime_error("Player must open the selected movie");
                menu.update(.21);menu.key(SDLK_SPACE);
                const int frame=menu.animation_frame();menu.update(.25);
                if (!menu.movie_paused() || menu.animation_frame()!=frame) throw std::runtime_error("Movie pause must freeze time");
                menu.key(SDLK_RIGHT);
                if (menu.animation_frame()<=frame) throw std::runtime_error("Movie seeking must move forward");
                display.begin();menu.render();display.present(DisplayFilter::Nearest,true);capture("player_playback");
                menu.key(SDLK_ESCAPE);
                // Endings must continue to the matching epilogue, then return.
                menu.key(SDLK_RIGHT);menu.key(SDLK_RETURN);
                if (menu.screen()==Screen::Playback) {
                    const auto first=menu.playing_movie();
                    bool epilogue=false;
                    for (int i=0;i<1000 && menu.screen()==Screen::Playback;++i) {
                        menu.update(.1);epilogue|=menu.playing_movie()=="END" || menu.playing_movie()=="ENL";
                    }
                    if (!epilogue || menu.screen()!=Screen::Player) throw std::runtime_error("Ending + epilogue series must finish cleanly");
                }
            }
        }
        {
            Frontend reload(renderer,assets,test_dir.string());
            if (reload.settings().music_volume!=95 || reload.settings().game_volume!=0 ||
                !reload.settings().easy_finishings || reload.key_for(0,4)!=sdl_to_dos(SDLK_z))
                throw std::runtime_error("Options and rebound attacks must survive reopening the frontend");
        }
        for (const char* file : {"font.png","port_options.json","rise2.cfg"}) std::filesystem::remove(test_dir/"ui"/file);
        std::filesystem::remove(test_dir/"ui");std::filesystem::remove(test_dir);
        int movies_checked=0;
        for (const auto& info : assets.movie_catalog()) {
            const auto* movie=assets.load_video(info.name,false);
            for (int frame : {0,movie->playable_frames/2,movie->playable_frames-1}) {
                SDL_Texture* texture=assets.video_frame(info.name,frame);
                int width=0,height=0;
                if (!texture || SDL_QueryTexture(texture,nullptr,nullptr,&width,&height) ||
                    width!=info.width || height!=info.height) throw std::runtime_error("Invalid movie frame: "+info.name);
                if (movie->streamed && std::count_if(movie->frames.begin(),movie->frames.end(),
                    [](SDL_Texture* frame) {return frame!=nullptr;})!=1)
                    throw std::runtime_error("Streaming movie must retain only one frame: "+info.name);
            }
            if (info.name!="LLOGO") assets.unload_video(info.name);
            ++movies_checked;
        }
        printf("%d movies checked at first/middle/last frames; streamed clips retain one texture.\n",movies_checked);
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
