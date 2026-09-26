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
        // The first source robot is A (Cyborg), not 0 (Surpressor). Exercise
        // the user's actual quarter-circle punch, including the mirrored FX.
        const auto* cyborg_atlas=assets.load_atlas("RBTA");
        const auto* cyborg_moves=assets.load_mvs("RBTA");
        for (int direction : {1,-1}) {
            p1=Fighter{};p2=Fighter{};
            p1.set_banks(cyborg_atlas,cyborg_moves);p1.cl2=assets.load_cl2("RA");p1.robot_id=0;
            p2.set_banks(ab1,mv1);p2.cl2=cl1;p2.robot_id=26;
            p1.x=direction>0 ? 140:500;p2.x=direction>0 ? 580:60;
            p1.facing=direction;p2.facing=-direction;
            p1.sprite_scale=156.0/cyborg_atlas->frames[1].rect_h;
            p2.sprite_scale=156.0/ab1->frames[1].rect_h;
            combat.reset();
            const int forward=direction>0 ? IN_RIGHT:IN_LEFT;
            for (int input : {int(IN_DOWN),int(IN_DOWN)|forward,forward,int(IN_PUNCH)}) p1.sample_inputs(input);
            bool colored_fx=false;
            for (int tick=0;tick<20;++tick) {
                combat.tick(p1,p2,tick==0 ? IN_PUNCH:0,0);
                if (tick==0) check(p1.move_id==81,"Cyborg : bas, diagonale, avant, poing declenche le vrai uppercut");
                if (tick!=2 && tick!=5) continue;
                SDL_SetRenderDrawColor(ren,0,0,0,255);SDL_RenderClear(ren);
                combat.render(ren,p1,p2,0);
                SDL_RenderPresent(ren);
                for (int y=0;y<surface->h;++y) for (int x=0;x<surface->w;++x) {
                    Uint8 r,g,b,a;
                    const auto* row=(const Uint32*)((const Uint8*)surface->pixels+y*surface->pitch);
                    SDL_GetRGBA(row[x],surface->format,&r,&g,&b,&a);
                    colored_fx|=r>200 && g>200 && b>200;
                }
                if (arena_tex) {SDL_Rect src{80,0,640,400},dst{0,0,640,400};SDL_RenderCopy(ren,arena_tex,&src,&dst);}
                draw_fighter(p1);draw_fighter(p2);combat.render(ren,p1,p2,0);
                check(IMG_SavePNG(surface,(out_dir+"/cyborg_uppercut_"+std::to_string(direction)+"_"+std::to_string(tick)+".png").c_str())==0,
                      "capture uppercut Cyborg et couleurs FX");
            }
            check(colored_fx,"le FX de Cyborg conserve ses details blancs, sans aplat vert");
        }
        const auto* prime8_atlas=assets.load_atlas("RBTC");
        const auto* prime8_moves=assets.load_mvs("RBTC");
        for (bool easy : {false,true}) {
        p1=Fighter{};p2=Fighter{};
        p1.set_banks(prime8_atlas,prime8_moves);p1.cl2=assets.load_cl2("RC");p1.robot_id=2;p1.x=220;
        p2.set_banks(ab1,mv1);p2.cl2=cl1;p2.robot_id=26;p2.x=320;p2.facing=-1;p2.health=1;
        p1.sprite_scale=156.0/prime8_atlas->frames[1].rect_h;
        p2.sprite_scale=156.0/ab1->frames[1].rect_h;
        combat.reset();
        combat.set_easy_finishings(easy);
        for (int tick=0;tick<40 && combat.phase()==RoundPhase::Fighting;++tick)
            combat.tick(p1,p2,tick==0 ? IN_PUNCH : 0,0);
        check(combat.phase()==RoundPhase::FinishWindow,"KO reel : fenetre de finishing");
        combat.tick(p1,p2,0,0);
        latest=0;
        if (easy) combat.sample_attack_buttons(1<<5,0); // rebound heavy kick
        else for (const auto& c : prime8_moves->commands) if (c.target==56)
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
                check(IMG_SavePNG(surface,(out_dir+(easy ? "/easy_finishing_":"/finishing_")+std::to_string(tick)+".png").c_str())==0,
                      "capture finishing original");
            }
        }
        check(started && death,easy ? "finishing facile reel : une touche et reaction CL2 de mort":
                                    "finishing reel : commande et reaction CL2 de mort");
        }
        // Reproduce the reported Cyborg miss: his late frame 333 has a
        // distant death box. Check the actual victim state, on both keyboards
        // and at both walls, rather than merely checking that move 58 starts.
        for (int winner : {0,1}) for (int direction : {1,-1}) for (bool wall : {false,true}) {
            p1=Fighter{};p2=Fighter{};
            Fighter& cyborg=winner==0 ? p1:p2;
            Fighter& victim=winner==0 ? p2:p1;
            cyborg.set_banks(cyborg_atlas,cyborg_moves);cyborg.cl2=assets.load_cl2("RA");cyborg.robot_id=0;
            victim.set_banks(ab1,mv1);victim.cl2=cl1;victim.robot_id=26;victim.health=0;
            victim.x=wall ? (direction>0 ? 60:580):320;
            cyborg.x=direction>0 ? victim.x-10:victim.x+10;
            cyborg.facing=direction;victim.facing=-direction;
            cyborg.sprite_scale=156.0/cyborg_atlas->frames[1].rect_h;
            victim.sprite_scale=156.0/ab1->frames[1].rect_h;
            combat.reset();combat.set_easy_finishings(true);combat.tick(p1,p2,0,0);
            const int distance=combat.assisted_finishing_distance(cyborg,victim,58);
            combat.sample_attack_buttons(winner==0 ? 1:0,winner==1 ? 1:0);
            bool death=false,started=false;
            for (int tick=0;tick<210;++tick) {
                combat.tick(p1,p2,0,0);started|=cyborg.move_id==58;death|=victim.move_id==59;
                if (tick==15 && winner==0 && !wall) {
                    SDL_SetRenderDrawColor(ren,0,0,0,255);SDL_RenderClear(ren);
                    draw_fighter(p1);draw_fighter(p2);combat.render(ren,p1,p2,0);
                    IMG_SavePNG(surface,(out_dir+"/cyborg_finishing_"+std::to_string(direction)+".png").c_str());
                }
            }
            printf("Cyborg finishing: player=%d facing=%d wall=%d distance=%d\n",winner+1,direction,wall,distance);
            check(started && death,"Cyborg : touche de finishing et veritable reaction de mort 59, meme pres du mur");
        }
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
