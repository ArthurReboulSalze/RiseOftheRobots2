// Rise 2 port skeleton: SDL window 1280x800 (logical 640x400), 15 Hz simulation,
// Two animated high-resolution fighters (RBT atlases and MVS movement transitions).
#include "assets.h"
#include "audio.h"
#include "fighter.h"
#include "combat.h"
#include "frontend.h"
#include "presentation.h"
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include <cstdio>
#include <cstdarg>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <algorithm>

static FILE* g_log = nullptr;
static void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

static int logical_w = 640, logical_h = 400;
static int ground_y = 312;
static int arena_min = 40, arena_max = 600;

static void draw_fighter(SDL_Renderer* r, const Fighter& f, int cam_x) {
    const AtlasFrame* af = f.current_atlas_frame();
    if (!af || af->empty) return;
    SDL_Texture* page = f.atlas->pages[af->page];
    SDL_Rect src{ af->rect_x, af->rect_y, af->rect_w, af->rect_h };
    SDL_Rect dst = f.frame_rect(*af, cam_x);
    const SDL_RendererFlip flip = f.facing >= 0 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    SDL_RenderCopyEx(r, page, &src, &dst, 0, nullptr, flip);
}

int main(int argc, char** argv) {
    std::string assets_dir = "..\\EXTRACTED";
    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == "--assets") assets_dir = argv[i + 1];
    }
    g_log = fopen("rotr2.log", "w");
    logf("=== rotr2 demarre (assets=%s) ===\n", argv[argc - 1]);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("Rise 2 : Resurrection Port",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800,
        SDL_WINDOW_RESIZABLE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!win || !ren) {fprintf(stderr,"SDL video: %s\n",SDL_GetError());SDL_Quit();return 1;}
    SDL_RenderSetLogicalSize(ren, logical_w, logical_h);
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);

    auto assets = std::make_unique<Assets>(ren, assets_dir);
    int ret = 0;
    try {
        Frontend frontend(ren, *assets, assets_dir);
        Presentation presentation(ren);
        Fighter p1, p2;
        UiFont fight_font(ren,assets_dir);
        Combat combat(assets->load_combat(),assets->load_atlas("EXTRA"),assets->load_cl2("EXTRA"));
        bool move_list = false, second_keyboard = false;
        int ai_clock = 0;

        // Audio : SDL_mixer (convertit automatiquement les echantillons, joue les MP3).
        Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC);
        const bool audio_ok = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == 0;
        int music_volume=-1,game_volume=-1;
        auto apply_options = [&]() {
            const auto& options=frontend.settings();
            combat.set_easy_finishings(options.easy_finishings);
            if (audio_ok && options.music_volume!=music_volume) {
                music_volume=options.music_volume; Mix_VolumeMusic((music_volume*MIX_MAX_VOLUME+50)/100);
            }
            if (audio_ok && options.game_volume!=game_volume) {
                game_volume=options.game_volume; Mix_Volume(-1,(game_volume*MIX_MAX_VOLUME+50)/100);
            }
        };
        apply_options();
        if (audio_ok) {
            int rate = 0, channels = 0;
            Uint16 format = 0;
            Mix_QuerySpec(&rate, &format, &channels);
            logf("audio mixer : OK (%d Hz, %d bits, %d channels); effects: sinc conversion\n",
                 rate, SDL_AUDIO_BITSIZE(format), channels);
        } else {
            logf("audio mixer : ECHEC (%s)\n", Mix_GetError());
        }
        // Each fighter has an independent R<slot>.MRW bank.  The direct-hit
        // path uses sample 15 from the attacker's bank (FUN_39996).
        struct FighterSounds { Mix_Chunk* samples[24] = {}; };
        FighterSounds snd[2];

        auto clear_fighter_sounds = [&](int player_index) {
            for (Mix_Chunk*& sample : snd[player_index].samples) {
                if (sample) Mix_FreeChunk(sample);
                sample = nullptr;
            }
        };

        auto load_fighter_sounds = [&](int player_index, char slot) {
            clear_fighter_sounds(player_index);
            for (int i = 0; i < 24; ++i) {
                if (!audio_ok) break;
                // Original 11,025 Hz PCM is converted with a windowed-sinc filter.
                const std::string path = assets_dir + "/audio/mrw/R" + slot +
                                         "/R" + slot + "_" +
                                         (i < 10 ? "0" : "") + std::to_string(i) + ".wav";
                // FUN_39996: EDX=0x2000 is volume; ECX=0x10000 is normal rate.
                const double gain = i == 15 ? double(0x2000) / 0x7fff : 1.0;
                snd[player_index].samples[i] = load_effect_wav(path.c_str(), gain);
            }
            int loaded = 0;
            for (int i = 0; i < 24; ++i) if (snd[player_index].samples[i]) ++loaded;
            logf("sons joueur %d (%c) : %d/24 ; impact[15]=%s\n", player_index + 1, slot,
                 loaded, snd[player_index].samples[15] ? "OK (sinc, gain 25%)" : "absent");
        };
        // Music lives beside a normal imported profile (../music/02.mp3, etc.).
        // Keep the older EXTRACTED/audio/cd location as a fallback for existing local installs.
        const std::filesystem::path asset_path(assets_dir);
        const std::vector<std::filesystem::path> music_dirs{
            asset_path.parent_path() / "music",
            asset_path / "audio" / "cd",
        };
        auto music_track_path = [&](int track_number) {
            char number[3];
            snprintf(number, sizeof(number), "%02d", track_number);
            static const char* extensions[] = {".wav", ".mp3", ".flac", ".ogg", ".m4a"};
            for (const auto& directory : music_dirs) {
                for (const char* extension : extensions) {
                    const auto imported = directory / (std::string(number) + extension);
                    if (std::filesystem::exists(imported)) return imported;
                    const auto legacy = directory / ("Piste " + std::string(number) + extension);
                    if (std::filesystem::exists(legacy)) return legacy;
                }
            }
            return std::filesystem::path{};
        };

        // CD audio: track 02 is the menu; 03–10 are selected for combat.
        Mix_Music* menu_music = nullptr;
        std::vector<Mix_Music*> fight_tracks;
        if (audio_ok) {
            const auto menu_path = music_track_path(2);
            if (!menu_path.empty()) menu_music = Mix_LoadMUS(menu_path.string().c_str());
            for (int n = 3; n <= 10; ++n) {
                const auto track_path = music_track_path(n);
                Mix_Music* m = track_path.empty() ? nullptr : Mix_LoadMUS(track_path.string().c_str());
                if (m) fight_tracks.push_back(m);
            }
            logf("musique : menu=%s ; %d pistes de combat\n",
                 menu_music ? "OK" : "absente", (int)fight_tracks.size());
            if (menu_music) Mix_PlayMusic(menu_music, -1);
        }
        auto play_random_fight_music = [&]() {
            if (!audio_ok || fight_tracks.empty()) return;
            Mix_HaltMusic();
            Mix_PlayMusic(fight_tracks[rand() % (int)fight_tracks.size()], -1);
        };

        // Fond d'arene : charge au demarrage du match (AGJ = l'arene de la capture de reference).
        SDL_Texture* arena = nullptr;
        auto load_arena = [&]() {
            if (arena) { SDL_DestroyTexture(arena); arena = nullptr; }
            const std::string path = assets_dir + "/ggf/AGJ.png";
            arena = IMG_LoadTexture(ren, path.c_str());
            logf("arene %s : %s\n", path.c_str(), arena ? "OK" : "ECHEC");
        };

        auto start_match = [&]() {
            auto configure = [&](Fighter& fighter, int player_index, int x, int facing) {
                const char slot = frontend.player(player_index).slot;
                // Resolution 640 uniquement (decision utilisateur) : banques RBT reduites
                // a l'echelle du combat (~156 px, calibree sur les boites CL2).
                const std::string bank = std::string("RBT") + slot;
                double scale = 2.0;
                fighter = Fighter{};
                for (int i=0; i<(int)kRoster.size(); ++i)
                    if (kRoster[i].slot == slot) fighter.robot_id = portrait_index(i);
                fighter.set_banks(assets->load_atlas(bank), assets->load_mvs(bank));
                if (fighter.atlas && fighter.atlas->frames.size() > 1 && fighter.atlas->frames[1].rect_h)
                    scale = 156.0 / fighter.atlas->frames[1].rect_h;
                const std::string cl2_name = std::string("R") + slot;
                fighter.cl2 = assets->load_cl2(cl2_name);
                fighter.sprite_scale = scale;
                logf("joueur %d : %s (banque %s, echelle %.2f)\n", player_index + 1,
                     frontend.player(player_index).name, bank.c_str(), scale);
                fighter.x = x;
                fighter.y = ground_y;
                fighter.ground_y = ground_y;
                fighter.facing = facing;
                fprintf(stderr, "Player %d: %s (%s)\n", player_index + 1,
                        frontend.player(player_index).name, bank.c_str());
            };
            configure(p1, 0, 220, 1);
            configure(p2, 1, 420, -1);
            load_fighter_sounds(0, frontend.player(0).slot);
            load_fighter_sounds(1, frontend.player(1).slot);
            combat.reset(); ai_clock = 0; move_list = false;
            load_arena();
            play_random_fight_music();
        };

        // Touches configurees (RISE2.CFG du port) : 10 entrees par joueur.
        SDL_Keycode p1keys[10], p2keys[10];
        auto load_keys = [&]() {
            for (int e = 0; e < 10; ++e) {
                p1keys[e] = dos_to_sdl(frontend.key_for(0, e));
                p2keys[e] = dos_to_sdl(frontend.key_for(1, e));
            }
        };
        load_keys();
        bool paused = false;
        int pause_choice = 0;

        bool run = true;
        constexpr double tick_seconds = 1.0 / 15.0;
        const double counter_frequency = static_cast<double>(SDL_GetPerformanceFrequency());
        Uint64 previous_counter = SDL_GetPerformanceCounter();
        double accumulator = 0.0;
        while (run) {
            bool scene_changed=false;
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) run = false;
                else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                    scene_changed=true;
                    if (frontend.screen() == Screen::Fight) {
                        if (paused) {
                            if (ev.key.keysym.sym == SDLK_UP) pause_choice = (pause_choice + 2) % 3;
                            else if (ev.key.keysym.sym == SDLK_DOWN) pause_choice = (pause_choice + 1) % 3;
                            else if (ev.key.keysym.sym == SDLK_F9) pause_choice = 1;
                            else if (ev.key.keysym.sym == SDLK_F10) pause_choice = 2;
                            else if (ev.key.keysym.sym == SDLK_ESCAPE) paused = false;
                            else if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) {
                                if (pause_choice == 0) paused = false;
                                else if (pause_choice == 2) {
                                    paused = false; frontend.return_to_select(); load_keys();
                                    if (audio_ok && menu_music) Mix_PlayMusic(menu_music, -1);
                                }
                                // 1 = calibrate joysticks : a venir
                            }
                        } else if (ev.key.keysym.sym == SDLK_F1) {
                            move_list = !move_list;
                        } else if (ev.key.keysym.sym == SDLK_F2) {
                            second_keyboard = !second_keyboard;
                        } else if (ev.key.keysym.sym == SDLK_RETURN && combat.phase() == RoundPhase::Result) {
                            start_match();
                        } else if (ev.key.keysym.sym == SDLK_ESCAPE) {
                            paused = true;
                            pause_choice = 0;
                        }
                    } else { frontend.key(ev.key.keysym.sym); load_keys(); }
                }
            }
            if (frontend.take_match_request()) {start_match();scene_changed=true;}
            if (frontend.quit_requested()) run = false;
            apply_options();
            const Uint64 counter = SDL_GetPerformanceCounter();
            double elapsed = static_cast<double>(counter - previous_counter) / counter_frequency;
            previous_counter = counter;
            if (elapsed > tick_seconds * 2) elapsed = tick_seconds * 2;
            accumulator += elapsed;
            const int intro_frame=frontend.animation_frame();
            const Screen old_screen=frontend.screen();
            frontend.update(elapsed);
            scene_changed|=intro_frame!=frontend.animation_frame() || old_screen!=frontend.screen();
            uint16_t in1 = 0, in2 = 0;
            if (frontend.screen() == Screen::Fight && !paused && !move_list) {
                const Uint8* kb = SDL_GetKeyboardState(nullptr);
                auto read_input = [&](const SDL_Keycode* keys,uint8_t& buttons) {
                    auto down = [&](int entry) { return keys[entry] != SDLK_UNKNOWN &&
                        kb[SDL_GetScancodeFromKey(keys[entry])]; };
                    uint16_t input = 0;
                    if (down(3)) input |= IN_RIGHT;
                    if (down(2)) input |= IN_LEFT;
                    if (down(0)) input |= IN_UP;
                    if (down(1)) input |= IN_DOWN;
                    for (int group=0; group<2; ++group) {
                        for (int force=0; force<3; ++force) {
                            if (!down(4+group*3+force)) continue;
                            buttons|=1<<(group*3+force);
                            input |= group == 0 ? IN_PUNCH : IN_KICK;
                            input &= ~(IN_MEDIUM|IN_HEAVY);
                            if (force == 1) input |= IN_MEDIUM;
                            if (force == 2) input |= IN_HEAVY;
                        }
                    }
                    return input;
                };
                uint8_t buttons1=0,buttons2=0;
                in1 = read_input(p1keys,buttons1);
                if (second_keyboard) in2 = read_input(p2keys,buttons2);
                combat.sample_attack_buttons(buttons1,buttons2);
                p1.sample_inputs(in1);
                if (second_keyboard) p2.sample_inputs(in2);
            }
            if (frontend.screen() == Screen::Fight && !paused && !move_list && accumulator >= tick_seconds) {
                accumulator -= tick_seconds;
                if (accumulator >= tick_seconds) accumulator = 0.0;
                // Basic sparring AI; original AIP decision rules are still separate.
                ++ai_clock;
                if (!second_keyboard && combat.phase() == RoundPhase::Fighting) {
                    const int distance = std::abs(p2.x-p1.x);
                    if (distance > 170) in2 = p2.x < p1.x ? IN_RIGHT : IN_LEFT;
                    else if (ai_clock % 60 < 12 && p2.flash == 0) in2 = IN_PUNCH;
                }
                combat.tick(p1,p2,in1,in2);
                scene_changed=true;
                for (const auto& hit : combat.hits()) {
                    if (audio_ok && snd[hit.attacker].samples[15])
                        Mix_PlayChannel(-1,snd[hit.attacker].samples[15],0);
                    logf("HIT p%d damage=%d blocked=%d projectile=%d hp=%d/%d\n",
                         hit.attacker+1,hit.damage,hit.blocked,hit.projectile,p1.health,p2.health);
                }
            } else if (paused || move_list) accumulator = 0.0;

            presentation.begin();
            if (frontend.screen() == Screen::Fight) {
                // camera : suit le point median, bornee aux 160 px de defilement (arene 800 large)
                const int cam_x = SDL_clamp((p1.x + p2.x) / 2 - logical_w / 2, 0, 800 - logical_w);
                if (arena) {
                    SDL_Rect src{ cam_x, 0, logical_w, logical_h };
                    SDL_Rect dst{ 0, 0, logical_w, logical_h };
                    SDL_RenderCopy(ren, arena, &src, &dst);
                } else {
                    SDL_SetRenderDrawColor(ren, 16, 13, 20, 255);
                    SDL_RenderClear(ren);
                    SDL_SetRenderDrawColor(ren, 60, 48, 70, 255);
                    SDL_Rect ground{ 0, ground_y, logical_w, 3 };
                    SDL_RenderFillRect(ren, &ground);
                }
                draw_fighter(ren, p1, cam_x);
                draw_fighter(ren, p2, cam_x);

                combat.render(ren,p1,p2,cam_x);

                // --- HUD: health bars and super meters ---
                {
                    int w = 240;
                    float f1 = p1.health / 120.0f, f2 = p2.health / 120.0f;
                    SDL_Rect b1{ 24, 16, (int)(w * f1), 14 };
                    SDL_SetRenderDrawColor(ren, 40, 200, 60, 255);
                    SDL_RenderFillRect(ren, &b1);
                    SDL_Rect b2{ logical_w - 24 - (int)(w * f2), 16, (int)(w * f2), 14 };
                    SDL_RenderFillRect(ren, &b2);
                    SDL_Rect s1{ 24, 36, p1.super_meter * 6, 6 };
                    SDL_Rect s2{ logical_w - 24 - p2.super_meter * 6, 36, p2.super_meter * 6, 6 };
                    SDL_SetRenderDrawColor(ren, 240, 200, 40, 255);
                    SDL_RenderFillRect(ren, &s1);
                    SDL_RenderFillRect(ren, &s2);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    if (p1.flash > 0) {
                        SDL_SetRenderDrawColor(ren, 255, 80, 80, 110);
                        SDL_Rect f1r{ 24, 16, w, 14 };
                        SDL_RenderFillRect(ren, &f1r);
                    }
                    if (p2.flash > 0) {
                        SDL_SetRenderDrawColor(ren, 255, 80, 80, 110);
                        SDL_Rect f2r{ logical_w - 24 - w, 16, w, 14 };
                        SDL_RenderFillRect(ren, &f2r);
                    }
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                }

                const SDL_Color white{235,240,255,255}, yellow{255,218,72,255};
                fight_font.draw(second_keyboard ? "F1 MOVES   F2: P2 KEYBOARD" : "F1 MOVES   F2: P2 CPU",
                                320,378,white,7,11,true);
                if (combat.phase() == RoundPhase::FinishWindow) {
                    fight_font.draw("P"+std::to_string(combat.winner()+1)+" - FINISH YOUR OPPONENT!",
                                    320,54,yellow,10,15,true);
                    fight_font.draw(frontend.settings().easy_finishings ? "PRESS AN ATTACK BUTTON - F1 DETAILS" :
                                    "F1: YOUR FINISHING COMMAND",320,72,white,7,11,true);
                } else if (combat.phase() == RoundPhase::Finishing) {
                    fight_font.draw("FINISHING",320,54,yellow,12,18,true);
                } else if (combat.phase() == RoundPhase::Ending || combat.phase() == RoundPhase::Result) {
                    fight_font.draw(combat.winner()<0 ? "DOUBLE KO" :
                                    "P"+std::to_string(combat.winner()+1)+" WINS",
                                    320,54,yellow,12,18,true);
                    if (combat.phase() == RoundPhase::Result)
                        fight_font.draw("ENTER: REMATCH   ESC: MENU",320,76,white,8,12,true);
                }
                if (move_list) {
                    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ren,0,0,0,235);
                    SDL_Rect full{0,45,640,325}; SDL_RenderFillRect(ren,&full);
                    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
                    fight_font.draw("ROBOT COMMANDS - F1 TO RETURN",320,50,yellow,9,14,true);
                    const Fighter* fighters[] = {&p1,&p2};
                    const SDL_Keycode* keys[] = {p1keys,p2keys};
                    for (int side=0; side<2; ++side) {
                        const auto& f = *fighters[side]; const int x = 12+side*320;
                        const auto finishers=combat.finishers(f);
                        fight_font.draw("P"+std::to_string(side+1)+" "+frontend.player(side).name,
                                        x,72,white,8,12);
                        std::string controls = "P:";
                        for (int i=4; i<10; ++i) {
                            if (i==7) controls += " K:";
                            controls += SDL_GetKeyName(keys[side][i]);
                            if (i!=6 && i!=9) controls += "/";
                        }
                        fight_font.draw(controls,x,87,white,6,10);
                        int row=0;
                        for (const auto& c : f.mvs->commands) {
                            std::string label = std::to_string(c.target)+" ";
                            if (!command_reachable(c)) label += "NO INPUT ";
                            else if (!f.has_action(c.target)) label += "NO DATA ";
                            else if (command_shadowed(*f.mvs,c)) label += "ORDER ";
                            else if (c.target==88) label += "SUPER ";
                            else if (c.target>=90) label += "LOCKED ";
                            else if (c.target>=48 && c.target<=58 && !(c.target&1))
                                label += combat.usable_finisher(f,c.target) ? "FINISH " : "NO DATA ";
                            const auto easy=std::find(finishers.begin(),finishers.end(),c.target);
                            if (frontend.settings().easy_finishings && easy!=finishers.end()) {
                                const int index=(int)(easy-finishers.begin());
                                if (finishers.size()==1) label+="[ANY ATTACK] EASY";
                                else {
                                    label += "["+std::string(SDL_GetKeyName(keys[side][4+index]));
                                    if (finishers.size()<=3) label+=" / "+std::string(SDL_GetKeyName(keys[side][7+index]));
                                    label+="] EASY";
                                }
                            } else label += command_notation(c);
                            fight_font.draw(label,x,104+row++*13,c.target==88 ? yellow : white,5,10);
                        }
                    }
                    fight_font.draw("F/B: TOWARD/AWAY   U/D: UP/DOWN   N: RELEASE   *: ANY INPUT",
                                    320,332,white,6,10,true);
                    fight_font.draw("P/K: PUNCH/KICK   KEYS: LIGHT / MEDIUM / HEAVY",320,347,white,7,11,true);
                    fight_font.draw("ORDER: EARLIER COMMAND TAKES PRIORITY",320,360,white,6,10,true);
                }

                // --- menu pause (Echap) ---
                if (paused) {
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ren, 0, 0, 0, 140);
                    SDL_Rect full{0, 0, logical_w, logical_h};
                    SDL_RenderFillRect(ren, &full);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                    static const char* items[] = {"CONTINUE MATCH", "F9  CALIBRATE JOYSTICKS", "F10 QUIT MATCH"};
                    frontend.draw_pause_overlay(pause_choice);
                }
            } else {
                frontend.render();
            }
            presentation.present(frontend.settings().filter,scene_changed);
            SDL_Delay(1);
        }
        Mix_HaltMusic();
        clear_fighter_sounds(0);
        clear_fighter_sounds(1);
        if (menu_music) Mix_FreeMusic(menu_music);
        for (Mix_Music* track : fight_tracks) Mix_FreeMusic(track);
    } catch (const std::exception& e) {
        fprintf(stderr, "ERREUR: %s\n", e.what());
        ret = 1;
    }
    assets.reset(); // Release textures before destroying the renderer.
    Mix_CloseAudio();
    Mix_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    IMG_Quit();
    SDL_Quit();
    return ret;
}
