#define SDL_MAIN_HANDLED
#include "fighter.h"
#include <cstdio>

static bool check(bool ok, const char* message) {
    if (!ok) std::fprintf(stderr, "%s\n", message);
    return ok;
}

int main() {
    AtlasBank atlas;
    atlas.frames.resize(3);
    atlas.frames[0].id = 0;
    atlas.frames[0].empty = true;
    atlas.frames[1].id = 1;
    atlas.frames[1].empty = false;
    atlas.frames[1].origin_x = 201;
    atlas.frames[1].origin_y = 124;
    atlas.frames[1].rect_w = 233;
    atlas.frames[1].rect_h = 201;
    atlas.frames[2].id = 2;
    atlas.frames[2].empty = false;
    atlas.frames[2].origin_x = 171;
    atlas.frames[2].origin_y = 121;
    atlas.frames[2].rect_w = 284;
    atlas.frames[2].rect_h = 204;

    MvsMove idle{};
    idle.flags = 2;
    idle.resume_index = 0;
    idle.sequences = {
        {{0, 0, false}, {1, 0, false}, {-1, 0, true}},
        {{0, 0, false}, {1, 0, false}, {-1, 0, true}},
        {{0, 0, false}, {1, 0, false}, {-1, 0, true}},
    };
    MvsMove one_shot = idle;
    one_shot.flags = 6; // automatic return + attack-strength reset
    MvsBank mvs;
    mvs.moves = {idle, one_shot};

    Cl2Bank cl2;
    cl2.frames.resize(2);
    cl2.frames[0].body_boxes.push_back({10, 20, 3, 4, 5});

    Fighter fighter;
    fighter.set_banks(&atlas, &mvs);
    fighter.cl2 = &cl2;
    if (!check(fighter.current_atlas_frame() == &atlas.frames[1],
               "MVS image 0 must select atlas frame 1")) return 1;
    std::vector<Cl2Box> bodies;
    fighter.get_boxes(nullptr, &bodies);
    if (!check(bodies.size() == 1, "MVS image 0 must keep CL2 record 0")) return 1;

    fighter.x = 220;
    fighter.y = 312;
    SDL_Rect first = fighter.frame_rect(atlas.frames[1], 0);
    SDL_Rect second = fighter.frame_rect(atlas.frames[2], 0);
    if (!check(first.x - atlas.frames[1].origin_x == second.x - atlas.frames[2].origin_x &&
               first.y - atlas.frames[1].origin_y == second.y - atlas.frames[2].origin_y,
               "cropped frames must keep a fixed right-facing canvas anchor")) return 1;
    fighter.facing = -1;
    first = fighter.frame_rect(atlas.frames[1], 0);
    second = fighter.frame_rect(atlas.frames[2], 0);
    if (!check(first.x + atlas.frames[1].origin_x + first.w ==
               second.x + atlas.frames[2].origin_x + second.w &&
               first.y - atlas.frames[1].origin_y == second.y - atlas.frames[2].origin_y,
               "mirrored frames must keep a fixed canvas anchor")) return 1;

    for (int i = 0; i < 20; ++i) {
        fighter.step(0);
        if (!check(fighter.current_frame() >= 0 && fighter.current_atlas_frame() != nullptr,
                   "loop must not expose the MVS end marker")) return 1;
    }

    fighter.move_id = 1;
    fighter.seq_pos = 0;
    fighter.playing = true;
    for (int i = 0; i < 5; ++i) fighter.step(0);
    if (!check(fighter.move_id == 0 && fighter.playing && fighter.current_frame() >= 0,
               "one-shot move must return to a visible idle frame")) return 1;

    mvs.moves[0].transitions = {{IN_RIGHT, 2}, {IN_LEFT, 3}};
    MvsMove walk = idle;
    walk.flags = 8;
    walk.transitions = {{0, 0}, {IN_LEFT, 3}};
    mvs.moves.push_back(walk);
    walk.transitions = {{0, 0}, {IN_RIGHT, 2}};
    mvs.moves.push_back(walk);
    fighter.facing = 1;
    fighter.move_id = 0;
    fighter.seq_pos = 0;
    fighter.playing = true;
    fighter.hit_move = 2;
    if (!check(fighter.step(IN_RIGHT) == 2 && fighter.move_id == 2,
               "forward input must enter the walk state")) return 1;
    if (!check(fighter.hit_move == -1,
               "a move transition must re-arm its collision sound/effect")) return 1;
    fighter.hit_move = 2;
    if (!check(fighter.step(0) == 0 && fighter.move_id == 0,
               "releasing a direction must restore idle")) return 1;
    if (!check(fighter.hit_move == -1,
               "returning to idle must re-arm the next move")) return 1;
    if (!check(fighter.step(IN_LEFT) == 3 && fighter.move_id == 3,
               "back input must enter the back-walk state")) return 1;
    if (!check(fighter.step(IN_RIGHT) == 2 && fighter.move_id == 2,
               "opposite direction must switch walking state")) return 1;

    // Attack buttons are a group: switching punch to kick while held must not
    // retrigger. The original FUN_197b6 produces [punch, 0, 0, 0, kick].
    mvs.moves[0].transitions.push_back({IN_PUNCH, 1});
    mvs.moves[0].transitions.push_back({IN_KICK, 1});
    Fighter attacker;
    attacker.set_banks(&atlas, &mvs);
    int attacks = 0;
    for (int tick = 0; tick < 40; ++tick) {
        if (attacker.step(IN_PUNCH) == 1) ++attacks;
    }
    if (!check(attacks == 1 && attacker.move_id == 0,
               "holding punch must play exactly one complete attack")) return 1;
    for (int tick = 0; tick < 20; ++tick) {
        if (attacker.step(IN_KICK) == 1) ++attacks;
    }
    if (!check(attacks == 1, "attack group must be released before switching buttons")) return 1;
    attacker.step(0);
    if (!check(attacker.step(IN_KICK) == 1, "release and press must re-arm an attack")) return 1;

    Fighter tap;
    tap.set_banks(&atlas, &mvs);
    tap.sample_inputs(IN_PUNCH);
    tap.sample_inputs(0);
    if (!check(tap.step(0) == 1, "a short tap between simulation ticks must survive")) return 1;
    for (int i = 0; i < 20; ++i) tap.step(0);
    if (!check(tap.move_id == 0, "a latched tap must not remain pressed")) return 1;

    Fighter mirrored;
    mirrored.set_banks(&atlas, &mvs);
    mirrored.facing = -1;
    if (!check(mirrored.step(IN_RIGHT) == 3,
               "screen right must select backward movement when facing left")) return 1;

    // Fixed-point trajectory independently measured by verify_x86_fighter.py
    // executing FUN_231ac. This catches signed impulse, unsigned gravity and
    // negative fractional rounding errors, rather than copying the port maths.
    MvsMove jump = idle;
    jump.flags = 0x4b;
    jump.param = 237; // -19 as a signed byte
    jump.gravity = 248;
    jump.auto_move = 0;
    jump.transitions.clear();
    mvs.moves.push_back(jump);
    mvs.moves[0].transitions.insert(mvs.moves[0].transitions.begin(), {IN_UP, 4});
    Fighter jumper;
    jumper.set_banks(&atlas, &mvs);
    jumper.cl2 = &cl2;
    const int dos_y[] = {278, 248, 220, 196, 174, 156, 140, 128, 118, 110,
                         106, 106, 106, 108, 114, 122, 134, 148, 166, 186,
                         208, 234, 264, 294, 312};
    for (int expected : dos_y) {
        jumper.step(IN_UP);
        if (!check(jumper.y == expected && jumper.current_frame() >= 0,
                   "jump must match the DOS trajectory and retain a visible frame")) return 1;
        bodies.clear();
        jumper.get_boxes(nullptr, &bodies);
        if (!bodies.empty() && !check(bodies[0].y == 40 + expected - 312,
                                     "collision boxes must rise and fall with the sprite")) return 1;
    }
    for (int i = 0; i < 40; ++i) jumper.step(IN_UP);
    if (!check(jumper.y == 312 && !jumper.airborne() && jumper.move_id == 0,
               "holding up must not launch another jump after landing")) return 1;
    jumper.step(0);
    if (!check(jumper.step(IN_UP) == 4 && jumper.airborne(),
               "releasing and pressing up must launch a new jump")) return 1;

    // Air attacks retain the current trajectory unless STS explicitly allows
    // another impulse (bit 0x10). Reusing a jump descriptor must not reset it.
    mvs.moves[4].transitions = {{IN_PUNCH, 5}};
    mvs.moves.push_back(jump);
    jumper.step(IN_UP | IN_PUNCH);
    if (!check(jumper.move_id == 5 && jumper.y == dos_y[1],
               "an airborne attack must not apply a second takeoff impulse")) return 1;

    MvsMove wait = idle;
    wait.sequences = {{{0, 0, false}, {-1, 0, true}}};
    wait.flags = 0x11;
    wait.param = 5;
    wait.auto_move = 0;
    wait.transitions.clear();
    mvs.moves.push_back(wait);
    Fighter delayed;
    delayed.set_banks(&atlas, &mvs);
    delayed.move_id = 6;
    for (int i = 0; i < 4; ++i) delayed.step(0);
    if (!check(delayed.move_id == 6, "counted hold must keep its full completion delay")) return 1;
    delayed.step(0);
    if (!check(delayed.move_id == 0, "counted hold must reach its automatic target")) return 1;
    return 0;
}
