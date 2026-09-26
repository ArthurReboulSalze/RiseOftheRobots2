#include "fighter.h"
#include <algorithm>
#include <cmath>

const MvsMove* Fighter::move() const {
    if (!mvs || move_id < 0 || move_id >= (int)mvs->moves.size()) return nullptr;
    return &mvs->moves[move_id];
}

int Fighter::current_frame() const {
    const MvsMove* mv = move();
    if (!mv || speed_level < 0 || speed_level >= (int)mv->sequences.size()) return -1;
    const auto& seq = mv->sequences[speed_level];
    if (seq_pos < 0 || seq_pos >= (int)seq.size() || seq[seq_pos].end) return -1;
    return seq[seq_pos].image;
}

const AtlasFrame* Fighter::current_atlas_frame() const {
    int img = current_frame();
    // ANL/ANR atlases start with an empty entry; MVS image 0 is atlas frame 1.
    if (!atlas || img < 0 || img + 1 >= (int)atlas->frames.size()) return nullptr;
    return &atlas->frames[img + 1];
}

SDL_Rect Fighter::frame_rect(const AtlasFrame& frame, int camera_x) const {
    if (!atlas || atlas->frames.size() < 2) return {0, 0, 0, 0};
    // Frames are cropped from the same canvas; sprite_scale maps them to the
    // 640x400 fight space (RB4 banks render x2). The anchor must not move.
    const double s = sprite_scale;
    const AtlasFrame& reference = atlas->frames[1];
    const int anchor_x = reference.origin_x + reference.rect_w / 2;
    const int anchor_y = reference.origin_y + reference.rect_h;
    const int screen_x = x - camera_x;
    const int w = (int)std::lround(frame.rect_w * s);
    const int h = (int)std::lround(frame.rect_h * s);
    int left;
    if (facing >= 0)
        left = (int)std::lround(screen_x + (frame.origin_x - anchor_x) * s);
    else
        left = (int)std::lround(screen_x - (frame.origin_x - anchor_x) * s - w);
    const int top = (int)std::lround(y + (frame.origin_y - anchor_y) * s);
    return {left, top, w, h};
}

int Fighter::step(uint16_t inputs) {
    const MvsMove* mv = move();
    if (!mv) return -1;
    int state_change = -1;
    // Walk states loop without exit transitions. The idle state's
    // forward/backward transitions identify their target movement IDs.
    if (move_id != 0 && !mvs->moves.empty()) {
        for (const auto& t : mvs->moves[0].transitions) {
            if ((t.mask == IN_B0 || t.mask == IN_B5) && move_id == t.target &&
                !(inputs & t.mask)) {
                move_id = 0;
                seq_pos = 0;
                displacement = 0;
                playing = true;
                state_change = 0;
                mv = move();
                break;
            }
        }
    }
    // 1) Transitions: fn_234fa restricts comparison to walk bits (0x21)
    //    when those bits are present in the input mask.
    for (auto& t : mv->transitions) {
        uint16_t cur = inputs;
        if (t.mask & 0x21) cur = cur & 0x21;
        if (cur == t.mask) {
            seq_pos = 0;
            displacement = 0;
            move_id = t.target;
            playing = true;
            return t.target;
        }
    }
    if (state_change >= 0) return state_change;
    // 2) Advance the sequence.
    if (speed_level < 0 || speed_level >= (int)mv->sequences.size()) return -1;
    const auto& seq = mv->sequences[speed_level];
    if (seq.empty()) return -1;
    const int frame_count = (int)seq.size() - (seq.back().end ? 1 : 0);
    if (frame_count == 0) return -1;
    if (seq_pos < 0 || seq_pos >= frame_count) seq_pos = 0;
    if (!playing) return -1;
    // Fin de sequence : les etats de deplacement/idle bouclent ; les autres (coups,
    // reactions) reviennent a l'etat debout — sinon l'action se repete sans fin.
    if (seq_pos == frame_count - 1) {
        static const int kLoopStates[] = {0, 2, 3, 8, 9, 0x16, 0x1A, 0x20, 0x21,
                                          0x22, 0x23, 0x3C, 0x3D, 0x3E};
        const bool loops = std::find(std::begin(kLoopStates), std::end(kLoopStates),
                                     move_id) != std::end(kLoopStates);
        if (!loops) {
            seq_pos = 0;
            displacement = 0;
            move_id = 0;
            playing = true;
            return 0;
        }
    }
    // Apply current-step displacement (the caller already uses movement()).
    seq_pos++;
    if (sound_callback) {
        // fn_226cc : les mouvements declenchent le sample 3 (pitchs 0x5000/0x3000 = 1250/750
        // pour-mille du sample joué a 11025) ; le sample 15 variable reste au hit.
        const int pitch = (seq_pos % 2) ? 1250 : 750;
        sound_callback(player_index_, 3, pitch);
    }
    if (seq_pos >= frame_count) {
        // The end marker is not a renderable image.
        if (seq.back().end && mv->flags != 0 && mv->resume_index < frame_count) {
            seq_pos = mv->resume_index;
        } else {
            seq_pos = frame_count - 1;
            playing = false;
        }
    }
    return -1;
}
void Fighter::get_boxes(std::vector<Cl2Box>* attacks, std::vector<Cl2Box>* bodies) const {
    if (!cl2 || current_frame() < 0 || current_frame() >= (int)cl2->frames.size()) return;
    const Cl2Frame& fr = cl2->frames[current_frame()];
    auto place = [&](const Cl2Box& b) {
        Cl2Box s = b;
        // x: CL2 x4, anchored at fighter center (mirrored when facing left).
        // y: CL2 y2, absolute canvas y (already aligned to the ground).
        int bx = b.x * 4 - cl2_ref_x;
        if (facing < 0) {
            s.x = x - bx - b.w * 4;
            s.w = b.w * 4;
        } else {
            s.x = x + bx;
            s.w = b.w * 4;
        }
        s.y = b.y * 2;
        s.h = b.h * 2;
        return s;
    };
    for (auto& b : fr.attack_boxes) if (attacks) attacks->push_back(place(b));
    for (auto& b : fr.body_boxes) if (bodies) bodies->push_back(place(b));
}
