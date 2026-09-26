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
    one_shot.flags = 0;
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

    mvs.moves[0].transitions = {{IN_B0, 2}, {IN_B5, 3}};
    MvsMove walk = idle;
    walk.flags = 6;
    walk.transitions.clear();
    mvs.moves.push_back(walk);
    mvs.moves.push_back(walk);
    fighter.move_id = 0;
    fighter.seq_pos = 0;
    fighter.playing = true;
    fighter.hit_move = 2;
    if (!check(fighter.step(IN_B0) == 2 && fighter.move_id == 2,
               "forward input must enter the walk state")) return 1;
    if (!check(fighter.hit_move == -1,
               "a move transition must re-arm its collision sound/effect")) return 1;
    fighter.hit_move = 2;
    if (!check(fighter.step(0) == 0 && fighter.move_id == 0,
               "releasing a direction must restore idle")) return 1;
    if (!check(fighter.hit_move == -1,
               "returning to idle must re-arm the next move")) return 1;
    if (!check(fighter.step(IN_B5) == 3 && fighter.move_id == 3,
               "back input must enter the back-walk state")) return 1;
    if (!check(fighter.step(IN_B0) == 2 && fighter.move_id == 2,
               "opposite direction must switch walking state")) return 1;
    return 0;
}
