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

void Fighter::sample_inputs(uint16_t inputs) {
    constexpr uint16_t attacks = IN_PUNCH | IN_KICK;
    // FUN_197b6 suppresses ALL attack buttons if any was held last sample.
    if (!(held_inputs & attacks)) pressed_inputs |= inputs & attacks;
    pressed_inputs |= inputs & ~held_inputs & IN_UP;
    held_inputs = inputs;
}

void Fighter::enter_move(int target, int first_frame) {
    if (!mvs || target < 0 || target >= (int)mvs->moves.size()) return;
    move_id = target;
    seq_pos = first_frame;
    displacement = repeat_count = 0;
    playing = true;
    hit_move = -1;
    move_started = false;
}

void Fighter::turn_to(int direction) {
    if (direction == facing) return;
    // FUN_25615 permits turning only in these states. Ordinary attacks keep
    // their orientation until completion; normal combat uses link mode -1.
    switch (move_id) {
    case 0: case 2: case 3: case 6: case 16: case 18: case 22: case 26:
    case 32: case 33: case 34: case 35: case 60: case 61: case 62:
    case 74: case 75: case 76: case 77: case 79:
        break;
    default:
        return;
    }
    facing = direction;
    if (move_id == 34 || move_id == 35) {
        // Swap forward/backward jump together with facing. Their signed
        // displacement streams then keep the SAME direction on screen.
        const int target = move_id == 34 ? 35 : 34;
        if (!mvs || target >= (int)mvs->moves.size()) return;
        move_id = target;
        const MvsMove* mv = move();
        if (!mv->sequences.empty()) {
            const auto& seq = mv->sequences[0]; // FUN_26321 checks the first stream
            const int count = (int)seq.size() - (!seq.empty() && seq.back().end ? 1 : 0);
            if (count > 0) seq_pos = std::clamp(seq_pos, 0, count - 1);
        }
        // FUN_26321 only clamps the frame. Preserve velocity, fraction, move
        // initialization and hit latch; turning must not restart takeoff.
    } else if (move_id != 32 && move_id != 75) {
        enter_move(move_id == 16 ? 69 : 68); // crouching/standing turn animations
    }
}

void update_facing(Fighter& a, Fighter& b) {
    if (a.x == b.x) {
        // DOS tie rule: one grounded fighter turns if both face the same way.
        // Airborne equality preserves orientation; there is no side to choose.
        if (a.facing == b.facing && a.y == a.ground_y && b.y == b.ground_y)
            a.facing = -a.facing;
        return;
    }
    const int direction = a.x < b.x ? 1 : -1;
    a.turn_to(direction);
    b.turn_to(-direction);
}

void Fighter::update_vertical() {
    const MvsMove* mv = move();
    if (!mv) return;
    if (!move_started) {
        move_started = true;
        // FUN_21baf: the impulse is a SIGNED byte, gravity an UNSIGNED STS byte.
        if (mv->flags & 0x40) {
            if (!airborne() || (mv->state_flags & 0x10))
                vertical_velocity = (int)(int8_t)mv->param * 256;
            vertical_gravity = (int)mv->gravity * 3 / 4;
        }
    }
    if (!airborne()) return;
    // FUN_231ac: truncate signed fixed-point displacement toward zero, then
    // retain the low byte of the accumulator, as the original MOV byte does.
    vertical_velocity += vertical_gravity * 2;
    const int accumulated = vertical_fraction + vertical_velocity;
    y += (accumulated / 256) * 2;
    vertical_fraction = (unsigned)accumulated & 0xff;
    if (y >= ground_y) {
        y = ground_y;
        vertical_velocity = vertical_fraction = 0;
        if ((mv->flags & 0x82) == 0x02)
            enter_move(mv->auto_move, mv->resume_index);
    }
}

int Fighter::step(uint16_t inputs) {
    const MvsMove* mv = move();
    if (!mv) return -1;
    const int old_move = move_id;
    sample_inputs(inputs);
    constexpr uint16_t attacks = IN_PUNCH | IN_KICK;
    uint16_t effective = (inputs & ~attacks) | (pressed_inputs & attacks);
    // One jump per press; direction and crouch continue to use held inputs.
    if (!(pressed_inputs & IN_UP) && !airborne()) effective &= ~IN_UP;
    pressed_inputs = 0;
    // FUN_197b6 swaps horizontal bits when the fighter faces left.
    if (facing < 0)
        effective = (effective & ~(IN_LEFT | IN_RIGHT)) |
                    ((effective & IN_LEFT) >> 1) | ((effective & IN_RIGHT) << 1);

    bool changed = false;
    for (const auto& t : mv->transitions) {
        uint16_t cur = effective;
        // FUN_234fa compares attack masks independently of held directions.
        if (t.mask & attacks) cur &= attacks;
        if (cur == t.mask && t.target != move_id && t.target < mvs->moves.size()) {
            enter_move(t.target);
            changed = true;
            mv = move();
            break;
        }
    }
    if (!changed && playing && speed_level >= 0 && speed_level < (int)mv->sequences.size()) {
        const auto& seq = mv->sequences[speed_level];
        const int count = (int)seq.size() - (!seq.empty() && seq.back().end ? 1 : 0);
        if (count > 0) {
            seq_pos = std::clamp(seq_pos, 0, count - 1);
            if (seq_pos + 1 < count) {
                ++seq_pos;
            } else if ((mv->flags & 2) && !airborne()) {
                // Automatic target, not an arbitrary list of looping move IDs.
                enter_move(mv->auto_move, (mv->flags & 8) ? mv->resume_index : 0);
            } else if (mv->flags & 2) {
                // The DOS sequence waits at its final visible frame until landing.
                seq_pos = count - 1;
            } else if (mv->flags & 0x10) {
                if (++repeat_count == mv->param)
                    enter_move(mv->auto_move, mv->resume_index);
                else
                    seq_pos = (mv->flags & 8) && mv->resume_index < count
                              ? mv->resume_index : count - 1;
            } else if ((mv->flags & 8) && mv->resume_index < count) {
                seq_pos = mv->resume_index;
            } else if (mv->flags & 1) {
                playing = false;
            } else {
                enter_move(0);
            }
        }
    }
    update_vertical();
    return changed || move_id != old_move ? move_id : -1;
}
void Fighter::get_boxes(std::vector<Cl2Box>* attacks, std::vector<Cl2Box>* bodies) const {
    if (!cl2 || current_frame() < 0 || current_frame() >= (int)cl2->frames.size()) return;
    const Cl2Frame& fr = cl2->frames[current_frame()];
    auto place = [&](const Cl2Box& b) {
        Cl2Box s = b;
        // x: CL2 x4, anchored at fighter center (mirrored when facing left).
        // y: CL2 y2, aligned to the ground plus the fighter's airborne offset.
        int bx = b.x * 4 - cl2_ref_x;
        if (facing < 0) {
            s.x = x - bx - b.w * 4;
            s.w = b.w * 4;
        } else {
            s.x = x + bx;
            s.w = b.w * 4;
        }
        s.y = b.y * 2 + y - ground_y;
        s.h = b.h * 2;
        return s;
    };
    for (auto& b : fr.attack_boxes) if (attacks) attacks->push_back(place(b));
    for (auto& b : fr.body_boxes) if (bodies) bodies->push_back(place(b));
}
