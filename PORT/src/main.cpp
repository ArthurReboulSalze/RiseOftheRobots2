// Ossature du port Rise 2 : fenetre SDL 1280x800 (logique 640x400), simulation 15 Hz,
// deux combattants hi-res animés (atlas RBT + mouvements MVS), transitions MVS actives.
#include "assets.h"
#include "fighter.h"
#include "frontend.h"
#include "SDL.h"
#include "SDL_image.h"
#include <cstdio>
#include <memory>
#include <string>

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

// orientation : le combattant regarde l'adversaire quand les positions diffèrent (fn_25615)
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

// applique les déplacements du mouvement courant (un pas par frame, x2, selon l'orientation)
static void apply_movement(Fighter& f) {
    const MvsMove* mv = f.move();
    if (!mv || f.seq_pos >= (int)mv->movements[f.speed_level].size()) return;
    int s = mv->movements[f.speed_level][f.seq_pos];
    if (s != 0) {
        // le pas pousse vers l'avant du regard (le fichier est écrit pour facing=1)
        f.x += s * 2 * f.facing;
        f.x = SDL_clamp(f.x, arena_min, arena_max);
    }
}

int main(int argc, char** argv) {
    std::string assets_dir = "..\\EXTRACTED";
    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == "--assets") assets_dir = argv[i + 1];
    }
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
        auto start_match = [&]() {
            auto configure = [&](Fighter& fighter, int player_index, int x, int facing) {
                const char slot = frontend.player(player_index).slot;
                const std::string bank = std::string("RBT") + slot;
                const std::string cl2_name = std::string("R") + slot;
                fighter = Fighter{};
                fighter.set_banks(assets->load_atlas(bank), assets->load_mvs(bank));
                fighter.cl2 = assets->load_cl2(cl2_name);
                fighter.x = x;
                fighter.y = ground_y;
                fighter.facing = facing;
                fprintf(stderr, "joueur %d: %s (%s)\n", player_index + 1,
                        frontend.player(player_index).name, bank.c_str());
            };
            configure(p1, 0, 220, 1);
            configure(p2, 1, 420, -1);
        };

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
                        if (ev.key.keysym.sym == SDLK_ESCAPE) frontend.return_to_select();
                    } else frontend.key(ev.key.keysym.sym);
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
            if (frontend.screen() == Screen::Fight && accumulator >= tick_seconds) {
                accumulator -= tick_seconds;
                if (accumulator >= tick_seconds) accumulator = 0.0;
                update_facing(p1, p2);
                const Uint8* kb = SDL_GetKeyboardState(nullptr);

                // Joueur 1 : flèches + J/K
                uint16_t in1 = horizontal_input(kb[SDL_SCANCODE_LEFT], kb[SDL_SCANCODE_RIGHT], p1.facing);
                if (kb[SDL_SCANCODE_DOWN])  in1 |= 0x04;
                if (kb[SDL_SCANCODE_UP])    in1 |= 0x02;
                if (kb[SDL_SCANCODE_J])     in1 |= 0x10;
                if (kb[SDL_SCANCODE_K])     in1 |= 0x08;
                // Joueur 2 : A/D = gauche/droite a l'ecran, W/S, I/O
                uint16_t in2 = horizontal_input(kb[SDL_SCANCODE_A], kb[SDL_SCANCODE_D], p2.facing);
                if (kb[SDL_SCANCODE_S]) in2 |= 0x04;
                if (kb[SDL_SCANCODE_W]) in2 |= 0x02;
                if (kb[SDL_SCANCODE_I]) in2 |= 0x10;
                if (kb[SDL_SCANCODE_O]) in2 |= 0x08;

                p1.hit_move = -1; // re-arm simplifié : un coup par frame d'attaque active
                p2.hit_move = -1;
                int old_move1 = p1.move_id, old_move2 = p2.move_id;
                int t1 = p1.step(in1);
                int t2 = p2.step(in2);
                if (t1 >= 0) fprintf(stderr, "p1: m%d -> m%d (entree=%#x)\n", old_move1, t1, in1);
                if (t2 >= 0) fprintf(stderr, "p2: m%d -> m%d (entree=%#x)\n", old_move2, t2, in2);
                apply_movement(p1);
                apply_movement(p2);
                update_facing(p1, p2);

                // --- collisions / dégâts (fn_381a9 + fn_38b72 simplifiés) ---
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
                            vic.flash = 3; // environ 0,13 s visibles avec la simulation a 15 Hz
                            att.super_meter = SDL_min(24, att.super_meter + 2);
                            att.hit_move = att.move_id;
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
            }

            if (frontend.screen() == Screen::Fight) {
                SDL_SetRenderDrawColor(ren, 16, 13, 20, 255);
                SDL_RenderClear(ren);
                SDL_SetRenderDrawColor(ren, 60, 48, 70, 255);
                SDL_Rect ground{ 0, ground_y, logical_w, 3 };
                SDL_RenderFillRect(ren, &ground);
                draw_fighter(ren, p1, 0);
                draw_fighter(ren, p2, 0);

                // --- HUD : barres de vie + jauges de super ---
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
    assets.reset(); // libere les textures avant la destruction du renderer
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    IMG_Quit();
    SDL_Quit();
    return ret;
}
