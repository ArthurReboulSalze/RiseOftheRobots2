// Banc de verification headless : rend le combat en PNG sans fenetre.
// Usage : rotr2_headless <assets_dir> <out_dir>
// Produit : render_fight_XX.png (toutes les 20 ticks) + un bilan sur stdout.
#include "assets.h"
#include "fighter.h"
#include "combat.h"
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

static uint16_t horizontal_input(bool left, bool right) {
    if (left == right) return 0;
    return right ? IN_RIGHT : IN_LEFT;
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

        p1.robot_id = 26; p2.robot_id = 5;
        p1.x = 220; p1.y = ground_y; p1.facing = 1;  p1.move_id = 0;
        p2.x = 420; p2.y = ground_y; p2.facing = -1; p2.move_id = 0;

        // 3) simulation scriptee : p1 marche puis frappe ; p2 subit ; 120 ticks
        Combat combat(assets.load_combat(),assets.load_atlas("EXTRA"),assets.load_cl2("EXTRA"));
        int hits = 0;
        auto draw_fighter = [&](const Fighter& f) {
            const AtlasFrame* af = f.current_atlas_frame();
            if (!af || af->empty) return;
            SDL_Rect dst = f.frame_rect(*af, 0);
            SDL_Rect src{ af->rect_x, af->rect_y, af->rect_w, af->rect_h };
            const SDL_RendererFlip flip = f.facing >= 0 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
            SDL_RenderCopyEx(ren, f.atlas->pages[af->page], &src, &dst, 0, nullptr, flip);
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
        int min_jump_y = ground_y;
        for (int tick = 0; tick < 160; ++tick) {
            SDL_SetRenderDrawColor(ren, 16, 13, 20, 255);
            SDL_RenderClear(ren);
            if (arena_tex) {
                SDL_Rect src{ 80, 0, 640, 400 };   // crop centre (arene 800)
                SDL_Rect dst{ 0, 0, 640, 400 };
                SDL_RenderCopy(ren, arena_tex, &src, &dst);
            }
            uint16_t in1 = 0;
            if (tick < 30 && p2.x-p1.x > 100) in1 = IN_RIGHT;
            else if (tick >= 40 && tick < 55) in1 = IN_PUNCH;
            else if (tick >= 70 && tick < 85) {
                // Follow the opponent's source pushback before the second strike.
                in1 = p2.x-p1.x > 100 ? IN_RIGHT : IN_PUNCH;
            }
            combat.tick(p1,p2,in1,tick>=120 ? IN_UP : 0);
            hits += (int)combat.hits().size();
            min_jump_y = SDL_min(min_jump_y,p2.y);

            draw_fighter(p1);
            draw_fighter(p2);
            combat.render(ren,p1,p2,0);

            if (tick % 20 == 0) {
                const std::string out = out_dir + "/render_fight_" + std::to_string(tick) + ".png";
                check(IMG_SavePNG(surface, out.c_str()) == 0, ("capture " + out).c_str());
            }
        }
        check(hits > 0, "au moins un coup porte pendant la simulation");
        check(hits == 2, "deux appuis maintenus produisent exactement deux impacts");
        check(p2.health < health_before, "les degats s'appliquent");
        check(min_jump_y < ground_y - 100 && !p2.airborne() && p2.y == ground_y,
              "le saut de RBTF monte puis atterrit sans repetition");
        const int first_health_after=p2.health;
        // Exercise a real bank's command, looping EXTRA projectile and on-hit
        // animation through the same Combat class as the interactive game.
        p1=Fighter{};p2=Fighter{};
        p1.set_banks(ab2,mv2);p1.cl2=clf;p1.robot_id=5;p1.x=180;
        p2.set_banks(ab1,mv1);p2.cl2=cl1;p2.robot_id=26;p2.x=400;p2.facing=-1;
        p1.sprite_scale=156.0/ab2->frames[1].rect_h;
        p2.sprite_scale=156.0/ab1->frames[1].rect_h;
        combat.reset();
        uint16_t latest=0;
        for (const auto& c : mv2->commands) if (c.target==84)
            for (auto it=c.inputs.rbegin();it!=c.inputs.rend();++it) {
                latest=*it==254 ? 0 : *it;p1.sample_inputs(latest);
            }
        int projectile_hits=0;
        for (int tick=0;tick<35;++tick) {
            combat.tick(p1,p2,tick==0 ? latest : 0,0);
            for (const auto& hit : combat.hits()) if (hit.projectile) ++projectile_hits;
            if (arena_tex) {
                SDL_Rect src{80,0,640,400},dst{0,0,640,400};
                SDL_RenderCopy(ren,arena_tex,&src,&dst);
            } else {SDL_SetRenderDrawColor(ren,16,13,20,255);SDL_RenderClear(ren);}
            draw_fighter(p1);draw_fighter(p2);combat.render(ren,p1,p2,0);
            if (tick==7 || tick==10 || tick==13)
                check(IMG_SavePNG(surface,(out_dir+"/projectile_"+std::to_string(tick)+".png").c_str())==0,
                      "capture FX projectile et explosion d'origine");
        }
        check(projectile_hits==1 && p2.health<120,"projectile original : un impact et des degats");
        const auto* war_atlas=assets.load_atlas("RBTC");
        const auto* war_moves=assets.load_mvs("RBTC");
        p1=Fighter{};p2=Fighter{};
        p1.set_banks(war_atlas,war_moves);p1.cl2=assets.load_cl2("RC");p1.robot_id=2;p1.x=220;
        p2.set_banks(ab1,mv1);p2.cl2=cl1;p2.robot_id=26;p2.x=320;p2.facing=-1;p2.health=1;
        p1.sprite_scale=156.0/war_atlas->frames[1].rect_h;
        p2.sprite_scale=156.0/ab1->frames[1].rect_h;
        combat.reset();
        for (int tick=0;tick<40 && combat.phase()==RoundPhase::Fighting;++tick)
            combat.tick(p1,p2,tick==0 ? IN_PUNCH : 0,0);
        check(combat.phase()==RoundPhase::FinishWindow,"KO reel : fenetre de finishing");
        combat.tick(p1,p2,0,0);
        for (const auto& c : war_moves->commands) if (c.target==56)
            for (auto it=c.inputs.rbegin();it!=c.inputs.rend();++it) {
                latest=*it==254 ? 0 : *it;p1.sample_inputs(latest);
            }
        bool started=false,death=false;
        for (int tick=0;tick<210;++tick) {
            combat.tick(p1,p2,tick==0 ? latest : 0,0);
            started|=combat.phase()==RoundPhase::Finishing;
            death|=p2.move_id>=49 && p2.move_id<=59 && (p2.move_id&1);
            if (tick==15 || tick==45 || tick==75) {
                if (arena_tex) {SDL_Rect src{80,0,640,400},dst{0,0,640,400};SDL_RenderCopy(ren,arena_tex,&src,&dst);}
                draw_fighter(p1);draw_fighter(p2);combat.render(ren,p1,p2,0);
                check(IMG_SavePNG(surface,(out_dir+"/finishing_"+std::to_string(tick)+".png").c_str())==0,
                      "capture finishing original");
            }
        }
        check(started && death,"finishing reel : commande et reaction CL2 de mort");
        SDL_DestroyTexture(arena_tex);
        if (arena) SDL_FreeSurface(arena);
        printf("       hits=%d ; vie p2 : %d -> %d\n", hits, health_before, first_health_after);
    } catch (const std::exception& e) {
        printf("[ECHEC] exception : %s\n", e.what());
        ++failures;
    }
    printf("=== %s : %d echec(s) ===\n", failures ? "ECHEC" : "TOUT OK", failures);
    return failures ? 1 : 0;
}
