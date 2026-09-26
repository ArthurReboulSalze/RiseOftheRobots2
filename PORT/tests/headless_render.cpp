// Banc de verification headless : rend le combat en PNG sans fenetre.
// Usage : rotr2_headless <assets_dir> <out_dir>
// Produit : render_fight_XX.png (toutes les 20 ticks) + un bilan sur stdout.
#include "assets.h"
#include "fighter.h"
#include "frontend.h"
#include "SDL.h"
#include "SDL_image.h"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

static int logical_w = 640, logical_h = 400;
static int ground_y = 312;
static int arena_min = 40, arena_max = 600;

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

static void apply_movement(Fighter& f) {
    const MvsMove* mv = f.move();
    if (!mv || f.seq_pos >= (int)mv->movements[f.speed_level].size()) return;
    int s = mv->movements[f.speed_level][f.seq_pos];
    if (s != 0) {
        f.x += s * 2 * f.facing;
        f.x = SDL_clamp(f.x, arena_min, arena_max);
    }
}

int main(int argc, char** argv) {
    std::string assets_dir = argc > 1 ? argv[1] : "K:\\Projects\\RiseOftheRobots2\\EXTRACTED";
    std::string out_dir = argc > 2 ? argv[2] : ".";

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        return 1;
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, logical_w, logical_h, 32,
                                                          SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* ren = SDL_CreateSoftwareRenderer(surface);

    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        printf("[%s] %s\n", ok ? "OK  " : "ECHEC", what);
        if (!ok) ++failures;
    };

    try {
        Assets assets(ren, assets_dir);

        // 1) arene : le fichier existe et se charge en surface CPU (pas de GPU requis)
        const std::string arena_path = assets_dir + "/ggf/AGJ.png";
        SDL_Surface* arena = IMG_Load(arena_path.c_str());
        check(arena != nullptr, ("chargement arene " + arena_path).c_str());
        if (arena) {
            printf("       arene %dx%d\n", arena->w, arena->h);
            check(arena->w == 800 && arena->h == 400, "dimensions arene 800x400");
        }

        // 2) combattants : banques RBT (640) reduites a l'echelle du combat
        const AtlasBank* ab1 = assets.load_atlas("RBT0");
        const AtlasBank* ab2 = assets.load_atlas("RBTF");
        const MvsBank* mv1 = assets.load_mvs("RBT0");
        const MvsBank* mv2 = assets.load_mvs("RBTF");
        const Cl2Bank* cl1 = assets.load_cl2("R0");
        const Cl2Bank* clf = assets.load_cl2("RF");
        check(ab1->frame_count > 10, "banque RBT0 chargee");
        check(cl1->count > 10, "CL2 R0 charge");
        Fighter p1, p2;
        p1.set_banks(ab1, mv1); p1.cl2 = cl1;
        p2.set_banks(ab2, mv2); p2.cl2 = clf;
        for (Fighter* f : {&p1, &p2}) {
            if (f->atlas->frames[1].rect_h)
                f->sprite_scale = 156.0 / f->atlas->frames[1].rect_h;
        }
        printf("       echelles : %.3f / %.3f\n", p1.sprite_scale, p2.sprite_scale);
        check(p1.sprite_scale > 0.4 && p1.sprite_scale < 1.0, "echelle combat plausible (0.4-1.0)");

        p1.x = 220; p1.y = ground_y; p1.facing = 1;  p1.move_id = 0;
        p2.x = 420; p2.y = ground_y; p2.facing = -1; p2.move_id = 0;

        // 3) simulation scriptee : p1 marche puis frappe ; p2 subit ; 120 ticks
        struct Particle { float x, y, vx, vy; int life; };
        std::vector<Particle> particles;
        int hits = 0;
        auto spawn_sparks = [&](int x, int y) {
            for (int i = 0; i < 10; ++i) {
                const float a = (float)(rand() % 628) / 100.0f;
                const float sp = 1.5f + (rand() % 30) / 10.0f;
                particles.push_back({ (float)x, (float)y, cosf(a) * sp, sinf(a) * sp - 1.0f,
                                      6 + rand() % 8 });
            }
        };

        auto draw_fighter = [&](const Fighter& f) {
            const AtlasFrame* af = f.current_atlas_frame();
            if (!af || af->empty) return;
            SDL_Rect dst = f.frame_rect(*af, 0);
            SDL_Rect src{ af->rect_x, af->rect_y, af->rect_w, af->rect_h };
            const SDL_RendererFlip flip = f.facing >= 0 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
            SDL_RenderCopyEx(ren, f.atlas->pages[af->page], &src, &dst, 0, nullptr, flip);
        };

        auto render_frame = [&](const char* out) {
            if (arena) {
                SDL_Rect src{ (800 - 640) / 2, 0, 640, 400 };
                SDL_Rect dst{ 0, 0, 640, 400 };
                SDL_RenderCopy(ren, nullptr, &src, &dst); // placeholder evite : voir note
            }
            // rendu via texture temporaire : l'arene est une surface, on la blitte en CPU
            // (SDL_CreateSoftwareRenderer accepte SDL_RenderCopy depuis une texture, on
            //  convertit donc l'arene une fois en texture logicielle).
            IMG_SavePNG(surface, out);
        };

        // les textures logicielles exigent des textures : convertir l'arene
        SDL_Texture* arena_tex = nullptr;
        if (arena) {
            // IMG_Load peut retourner une surface indexee : convertir en RGBA32
            // avant SDL_UpdateTexture, sinon les indices de palette passent pour
            // des octets de couleur (arene delavee).
            SDL_Surface* rgba = SDL_ConvertSurfaceFormat(arena, SDL_PIXELFORMAT_RGBA32, 0);
            arena_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STATIC, rgba->w, rgba->h);
            SDL_UpdateTexture(arena_tex, nullptr, rgba->pixels, rgba->pitch);
            SDL_FreeSurface(rgba);
        }

        int health_before = p2.health;
        for (int tick = 0; tick < 120; ++tick) {
            SDL_SetRenderDrawColor(ren, 16, 13, 20, 255);
            SDL_RenderClear(ren);
            if (arena_tex) {
                SDL_Rect src{ 80, 0, 640, 400 };   // crop centre (arene 800)
                SDL_Rect dst{ 0, 0, 640, 400 };
                SDL_RenderCopy(ren, arena_tex, &src, &dst);
            }
            update_facing(p1, p2);
            p1.hit_move = -1;
            p2.hit_move = -1;
            // script : p1 avance 30 ticks puis punch (0x10) pendant 10 ticks, reprise
            uint16_t in1 = 0;
            if (tick < 30) in1 = horizontal_input(false, true, p1.facing);
            else if (tick >= 40 && tick < 55) in1 = 0x10;
            else if (tick >= 70 && tick < 85) in1 = 0x10;
            p1.step(in1);
            p2.step(0);
            apply_movement(p1);
            apply_movement(p2);
            update_facing(p1, p2);

            std::vector<Cl2Box> att1, bod2;
            p1.get_boxes(&att1, nullptr);
            p2.get_boxes(nullptr, &bod2);
            auto overlap = [](const Cl2Box& a, const Cl2Box& b) {
                return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
            };
            if (p1.hit_move != p1.move_id) {
                for (auto& a : att1) {
                    bool done = false;
                    for (auto& b : bod2) {
                        if (overlap(a, b)) {
                            p2.health = SDL_max(0, p2.health - a.damage_or_type);
                            p2.flash = 3;
                            p1.hit_move = p1.move_id;
                            spawn_sparks((a.x + b.x) / 2, (a.y + b.y) / 2);
                            ++hits;
                            done = true;
                            break;
                        }
                    }
                    if (done) break;
                }
            }
            for (auto& pt : particles) { pt.x += pt.vx; pt.y += pt.vy; pt.vy += 0.15f; --pt.life; }
            particles.erase(std::remove_if(particles.begin(), particles.end(),
                [](const Particle& p) { return p.life <= 0; }), particles.end());

            draw_fighter(p1);
            draw_fighter(p2);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            for (const auto& pt : particles) {
                SDL_SetRenderDrawColor(ren, 255, 230, 120, (int)(255 * pt.life / 14));
                SDL_Rect px{ (int)pt.x, (int)pt.y, 3, 3 };
                SDL_RenderFillRect(ren, &px);
            }
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

            if (tick % 20 == 0) {
                const std::string out = out_dir + "/render_fight_" + std::to_string(tick) + ".png";
                check(IMG_SavePNG(surface, out.c_str()) == 0, ("capture " + out).c_str());
            }
        }
        check(hits > 0, "au moins un coup porte pendant la simulation");
        check(p2.health < health_before, "les degats s'appliquent");
        printf("       hits=%d ; vie p2 : %d -> %d\n", hits, health_before, p2.health);
    } catch (const std::exception& e) {
        printf("[ECHEC] exception : %s\n", e.what());
        ++failures;
    }
    printf("=== %s : %d echec(s) ===\n", failures ? "ECHEC" : "TOUT OK", failures);
    return failures ? 1 : 0;
}