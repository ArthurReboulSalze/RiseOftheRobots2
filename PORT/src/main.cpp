// Rise 2 port skeleton: SDL window 1280x800 (logical 640x400), 15 Hz simulation,
// Two animated high-resolution fighters (RBT atlases and MVS movement transitions).
#include "assets.h"
#include "fighter.h"
#include "frontend.h"
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

// Facing: each fighter looks toward the opponent when positions differ (fn_25615).
static void update_facing(Fighter& a, Fighter& b) {
    if (a.x < b.x) a.facing = 1;
    else if (a.x > b.x) a.facing = -1;
    if (b.x < a.x) b.facing = 1;
    else if (b.x > a.x) b.facing = -1;
}

static uint16_t horizontal_input(bool left, bool right, int facing) {
    if (left == right) return 0;
    const bool forward = facing >= 0 ? right : left;
    return forward ? IN_B0 : IN_B5;
}

// Apply current-movement displacement (one step per frame, scaled x2 by facing).
static void apply_movement(Fighter& f) {
    const MvsMove* mv = f.move();
    if (!mv || f.seq_pos >= (int)mv->movements[f.speed_level].size()) return;
    int s = mv->movements[f.speed_level][f.seq_pos];
    if (s != 0) {
        // A step moves along facing; the file encodes facing=1.
        f.x += s * 2 * f.facing;
        f.x = SDL_clamp(f.x, arena_min, arena_max);
    }
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
    SDL_Window* win = SDL_CreateWindow("Rise 2 - port (ossature)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800,
        SDL_WINDOW_RESIZABLE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, logical_w, logical_h);
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);

    auto assets = std::make_unique<Assets>(ren, assets_dir);
    int ret = 0;
    try {
        Frontend frontend(ren, *assets, assets_dir);
        Fighter p1, p2;

        // Audio : SDL_mixer (convertit automatiquement les echantillons, joue les MP3).
        Mix_Init(MIX_INIT_MP3);
        const bool audio_ok = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == 0;
        logf("audio mixer : %s (%s)\n", audio_ok ? "OK" : "ECHEC",
             audio_ok ? "44100 stereo" : Mix_GetError());
        Mix_Chunk* hit_wav = nullptr;
        if (audio_ok) {
            hit_wav = Mix_LoadWAV((assets_dir + "/audio/mrw/R0/R0_04.wav").c_str());
            if (hit_wav) logf("son de hit charge\n");
        }
        // Banques sonores PAR COMBATANT : chaque robot a ses propres samples
        // (l'impact de coup = sample 15 de la banque du robot, cf. fn_39996).
        struct FighterSounds { Mix_Chunk* samples[24] = {}; };
        FighterSounds snd[2];

        // whooshes de mouvement : sample 3 du robot (fn_226cc), canaux dedies.
        auto sound_cb = [&](int player_index, int sample_id, int pitch_permille) {
            if (!audio_ok) return;
            Mix_Chunk* s = snd[player_index].samples[sample_id];
            if (!s) return;
            Mix_PlayChannel(player_index == 0 ? 2 : 3, s, 0);
        };
        p1.sound_callback = sound_cb;
        p2.sound_callback = sound_cb;
        p1.player_index_ = 0;
        p2.player_index_ = 1;

        auto load_fighter_sounds = [&](int player_index, char slot) {
            for (int i = 0; i < 24; ++i) {
                if (!audio_ok) break;
                const std::string path = assets_dir + "/audio/mrw_hq/R" + slot +
                                         "/R" + slot + "_" +
                                         (i < 10 ? "0" : "") + std::to_string(i) + ".wav";
                snd[player_index].samples[i] = Mix_LoadWAV(path.c_str());
            }
            int loaded = 0;
            for (int i = 0; i < 24; ++i) if (snd[player_index].samples[i]) ++loaded;
            logf("sons joueur %d (%c) : %d/24\n", player_index + 1, slot, loaded);
        };
        // Musique : CD rippé (Piste 02 = titre/menu ; 03-10 = combat, tirage aléatoire).
        Mix_Music* menu_music = nullptr;
        std::vector<Mix_Music*> fight_tracks;
        if (audio_ok) {
            menu_music = Mix_LoadMUS((assets_dir + "/audio/cd/Piste 02.mp3").c_str());
            for (int n = 3; n <= 10; ++n) {
                char name[32];
                snprintf(name, sizeof(name), "/audio/cd/Piste %02d.mp3", n);
                Mix_Music* m = Mix_LoadMUS((assets_dir + name).c_str());
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

        // Particules d'impact (remplace le systeme fn_22848 le temps d'identifier ses donnees).
        struct Particle { float x, y, vx, vy; int life; };
        std::vector<Particle> particles;

        auto spawn_sparks = [&](int x, int y) {
            for (int i = 0; i < 10; ++i) {
                const float a = (float)(rand() % 628) / 100.0f;
                const float sp = 1.5f + (rand() % 30) / 10.0f;
                particles.push_back({ (float)x, (float)y,
                                      cosf(a) * sp, sinf(a) * sp - 1.0f,
                                      6 + rand() % 8 });
            }
            // le son du coup = sample 15 de la banque du robot (joue dans le bloc hit)
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
                fighter.facing = facing;
                fprintf(stderr, "Player %d: %s (%s)\n", player_index + 1,
                        frontend.player(player_index).name, bank.c_str());
            };
            configure(p1, 0, 220, 1);
            configure(p2, 1, 420, -1);
            load_fighter_sounds(0, frontend.player(0).slot);
            load_fighter_sounds(1, frontend.player(1).slot);
            particles.clear();
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
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) run = false;
                else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
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
                        } else if (ev.key.keysym.sym == SDLK_ESCAPE) {
                            paused = true;
                            pause_choice = 0;
                        }
                    } else { frontend.key(ev.key.keysym.sym); load_keys(); }
                }
            }
            if (frontend.take_match_request()) start_match();
            if (frontend.quit_requested()) run = false;
            const Uint64 counter = SDL_GetPerformanceCounter();
            double elapsed = static_cast<double>(counter - previous_counter) / counter_frequency;
            previous_counter = counter;
            if (elapsed > tick_seconds * 2) elapsed = tick_seconds * 2;
            accumulator += elapsed;
            frontend.update(elapsed);
            if (frontend.screen() == Screen::Fight && !paused && accumulator >= tick_seconds) {
                accumulator -= tick_seconds;
                if (accumulator >= tick_seconds) accumulator = 0.0;
                update_facing(p1, p2);
                const Uint8* kb = SDL_GetKeyboardState(nullptr);

                // Player 1: arrow keys and J/K.
                // Entrees : semantique exacte de fn_197b6 —
                // 0x02 = droite (avance), 0x04 = gauche (recul), 0x08 = haut, 0x10 = bas,
                // 0x01 = poings (01/02/03), 0x20 = pieds (P1/P2/P3). JAMAIS relatifs au facing.
                auto down = [&](SDL_Keycode k) { return k != SDLK_UNKNOWN && kb[SDL_GetScancodeFromKey(k)]; };
                uint16_t in1 = 0;
                if (down(p1keys[3])) in1 |= 0x02;   // RIGHT
                if (down(p1keys[2])) in1 |= 0x04;   // LEFT
                if (down(p1keys[0])) in1 |= 0x08;   // UP
                if (down(p1keys[1])) in1 |= 0x10;   // DOWN
                if (down(p1keys[4]) || down(p1keys[5]) || down(p1keys[6])) in1 |= 0x01;  // poings
                if (down(p1keys[7]) || down(p1keys[8]) || down(p1keys[9])) in1 |= 0x20;  // pieds
                uint16_t in2 = 0;
                if (down(p2keys[3])) in2 |= 0x02;
                if (down(p2keys[2])) in2 |= 0x04;
                if (down(p2keys[0])) in2 |= 0x08;
                if (down(p2keys[1])) in2 |= 0x10;
                if (down(p2keys[4]) || down(p2keys[5]) || down(p2keys[6])) in2 |= 0x01;
                if (down(p2keys[7]) || down(p2keys[8]) || down(p2keys[9])) in2 |= 0x20;

                p1.hit_move = -1; // Simplified re-arm: one hit per active attack frame.
                p2.hit_move = -1;
                int old_move1 = p1.move_id, old_move2 = p2.move_id;

                // IA minimale (placeholder du 35eb8) : s'approche, cogne de temps en temps.
                static int ai_clock = 0;
                ++ai_clock;
                uint16_t in2_ai = 0;
                {
                    const int dx = std::abs(p2.x - p1.x);
                    const bool forward = p2.facing >= 0 ? kb[SDL_SCANCODE_D] || true : kb[SDL_SCANCODE_A] || true;
                    (void)forward;
                    if (dx > 170) {
                        // marche vers p1 : la droite ecran si p2 est a gauche de p1
                        in2_ai = (p2.x < p1.x) ? 0x02 : 0x04;
                    } else if (ai_clock % 60 < 12 && p2.flash == 0) {
                        in2_ai = 0x10; // punch 1
                    }
                }

                int t1 = p1.step(in1);
                int t2 = p2.step(in2_ai);
                if (t1 >= 0) fprintf(stderr, "p1: m%d -> m%d (entree=%#x)\n", old_move1, t1, in1);
                if (t2 >= 0) fprintf(stderr, "p2: m%d -> m%d (entree=%#x)\n", old_move2, t2, in2);
                apply_movement(p1);
                apply_movement(p2);
                update_facing(p1, p2);

                // --- Collisions and damage (simplified fn_381a9 + fn_38b72) ---
                static std::vector<Cl2Box> att1, bod1, att2, bod2;
                att1.clear(); bod1.clear(); att2.clear(); bod2.clear();
                p1.get_boxes(&att1, &bod1);
                p2.get_boxes(&att2, &bod2);
                auto overlap = [](const Cl2Box& a, const Cl2Box& b) {
                    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
                };
                auto try_hit = [&](Fighter& att, Fighter& vic, const char* na, const char* nv,
                                  std::vector<Cl2Box>& A, std::vector<Cl2Box>& B) {
                    if (att.hit_move == att.move_id || att.flash > 0) return;
                    for (auto& a : A) {
                        for (auto& b : B) {
                            if (!overlap(a, b)) continue;
                            int dmg = a.damage_or_type;
                            vic.health = SDL_max(0, vic.health - dmg);
                            vic.flash = 3; // About 0.13 s visible at the 15 Hz simulation rate.
                            att.super_meter = SDL_min(24, att.super_meter + 2);
                            att.hit_move = att.move_id;
                            spawn_sparks((a.x + b.x) / 2 + (b.w > a.w ? b.w : a.w) / 2,
                                         (a.y + b.y) / 2);
                            {
                                const int bank = (&att == &p1) ? 0 : 1;
                                if (snd[bank].samples[15]) Mix_PlayChannel(-1, snd[bank].samples[15], 0);
                            }
                            fprintf(stderr, "HIT %s -> %s : dmg=%d (vie %s=%d)\n", na, nv, dmg, nv, vic.health);
                            return;
                        }
                    }
                };
                try_hit(p1, p2, "p1", "p2", att1, bod2);
                try_hit(p2, p1, "p2", "p1", att2, bod1);
                if (p1.flash > 0) p1.flash--;
                if (p2.flash > 0) p2.flash--;
                if (p1.hit_move != p1.move_id) p1.hit_move = -1;
                if (p2.hit_move != p2.move_id) p2.hit_move = -1;

                // particules
                for (auto& pt : particles) { pt.x += pt.vx; pt.y += pt.vy; pt.vy += 0.15f; --pt.life; }
                particles.erase(std::remove_if(particles.begin(), particles.end(),
                    [](const Particle& p) { return p.life <= 0; }), particles.end());
            }

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

                // particules d'impact
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                for (const auto& pt : particles) {
                    SDL_SetRenderDrawColor(ren, 255, 230, 120, (int)(255 * pt.life / 14));
                    SDL_Rect px{ (int)pt.x - cam_x, (int)pt.y, 3, 3 };
                    SDL_RenderFillRect(ren, &px);
                }
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

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
            SDL_RenderPresent(ren);
            SDL_Delay(1);
        }
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
