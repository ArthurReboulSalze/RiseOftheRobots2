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
    if (!(held_inputs & attacks) && (inputs & attacks)) {
        pressed_inputs |= inputs & attacks;
        pressed_strength = (inputs & IN_HEAVY) ? 90 : (inputs & IN_MEDIUM) ? 60 : 30;
    }
    if ((inputs & 0x3f) != (held_inputs & 0x3f)) {
        uint16_t event = inputs & 0x3f;
        if (held_inputs & attacks) event &= ~attacks;
        if (input_events.size() < 32) input_events.push_back(event);
    }
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
    hits_in_move = 0;
    move_started = false;
    combo_cancel = false;
    ++move_serial;
    // FUN_21baf: saved strength selects a stream against full energy (120).
    speed_level = std::clamp((attack_strength * 3 + 3) / 120, 0, 2);
    if (speed_level >= (int)move()->sequences.size()) speed_level = 0;
}

void Fighter::discard_inputs() {
    pressed_inputs=0; input_events.clear(); history.fill(240); history_age=0;
}

void Fighter::force_move(int target, int first_frame) { enter_move(target, first_frame); }

void Fighter::stop_vertical() {
    y = ground_y;
    vertical_velocity = vertical_fraction = 0;
}

void Fighter::remember(int input) {
    // FUN_161c0 stores changes newest first, with F0 marking a held-input gap.
    if (input & 0x21) input &= 0x21;
    const int comparison = history[0] == 240 ? history[1] : history[0];
    if (input != comparison) {
        std::move_backward(history.begin(), history.end()-1, history.end());
        history[0] = input;
        history_age = 0;
    }
}

bool Fighter::command_allowed(int target, const Fighter* opponent, bool finishing, int slots) const {
    if (!mvs || target < 0 || target >= (int)mvs->moves.size()) return false;
    const auto& desired = mvs->moves[target];
    if (!has_action(target)) return false;
    if ((desired.ground_mode == 2) != airborne()) return false;
    if (!desired.projectile.empty() && slots <= 0) return false;
    if (target >= 48 && target <= 58 && !(target & 1) && !finishing) return false;
    if (target == 88 && super_meter != 24) return false;
    if (move_id == 72 || move_id == 73) return false;
    if (move()->action_type > 1 && seq_pos > 1 && !combo_cancel) return false;
    if (((robot_id == 4 && target == 87) || ((robot_id == 5 || robot_id == 20) && target == 80))
        && pressed_strength != 30) return false;
    if (target == 89 && (!opponent || (opponent->robot_id != 6 && opponent->robot_id != 18) ||
        opponent->health > 30 || std::abs(x-opponent->x) > 140 || airborne() || opponent->airborne())) return false;
    if (target >= 90 && !(stolen_moves & (1 << (target-90)))) return false;
    return true;
}

bool Fighter::has_action(int target) const {
    if (!mvs || target < 0 || target >= (int)mvs->moves.size()) return false;
    const auto& m = mvs->moves[target];
    if (!m.projectile.empty()) return true;
    for (const auto& s : m.attached) if (!s.empty()) return true;
    for (const auto& s : m.sequences) {
        if (s.size()>2) return true;
        for (const auto& e : s)
            if (!e.end && cl2 && e.image>=0 && e.image<(int)cl2->frames.size() &&
                !cl2->frames[e.image].attack_boxes.empty()) return true;
    }
    return false;
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

int Fighter::step(uint16_t inputs, const Fighter* opponent, bool finishing, int free_projectiles) {
    advanced = false;
    const MvsMove* mv = move();
    if (!mv) return -1;
    const int old_move = move_id;
    sample_inputs(inputs);
    constexpr uint16_t attacks = IN_PUNCH | IN_KICK;
    uint16_t effective = (inputs & 0x1e) | (pressed_inputs & attacks);
    if (effective & attacks) input_strength = pressed_strength;
    // One jump per press; direction and crouch continue to use held inputs.
    if (!(pressed_inputs & IN_UP) && !airborne()) effective &= ~IN_UP;
    // FUN_197b6 swaps horizontal bits when the fighter faces left.
    if (facing < 0)
        effective = (effective & ~(IN_LEFT | IN_RIGHT)) |
                    ((effective & IN_LEFT) >> 1) | ((effective & IN_RIGHT) << 1);

    auto relative = [&](int mask) {
        return facing < 0 ? (mask & ~6) | ((mask & 4) >> 1) | ((mask & 2) << 1) : mask;
    };
    if (history_lock > 0) {
        --history_lock;
        input_events.clear();
    } else {
        for (int event : input_events) remember(relative(event));
        input_events.clear();
        remember(effective);
        if (++history_age >= 5 && history[0] != 240) {
            std::move_backward(history.begin(), history.end()-1, history.end());
            history[0] = 240;
            history_age = 0;
        }
    }
    if (hit_pause > 0) { --hit_pause; return -1; }
    pressed_inputs = 0;
    advanced = true;

    bool changed = false;
    if (!(mv->state_flags & 0x20) && (move_id > 79 || move_id == 42 || (move_id & 15) < 10)) {
        for (const auto& command : mvs->commands) {
            if (command.inputs.size() > history.size()) continue;
            bool matched = true;
            for (size_t i = 0; i < command.inputs.size(); ++i)
                if (command.inputs[i] != 254 && command.inputs[i] != history[i]) matched = false;
            if (!matched || !command_allowed(command.target, opponent, finishing, free_projectiles)) continue;
            attack_strength = input_strength;
            if (command.target == 88) super_meter = 0;
            if (command.target >= 90) stolen_moves &= ~(1 << (command.target-90));
            enter_move(command.target);
            history.fill(240); history_lock = 8; history_age = 0;
            changed = true; mv = move();
            break;
        }
    }
    for (const auto& t : mv->transitions) {
        if (changed) break;
        uint16_t cur = effective;
        // FUN_234fa compares attack masks independently of held directions.
        if (t.mask & attacks) cur &= attacks;
        if (cur == t.mask && t.target != move_id && t.target < mvs->moves.size()) {
            attack_strength = input_strength;
            int target = t.target;
            // FUN_234fa: forward + punch/kick at close range selects the grab.
            if ((target == 8 || target == 9) && opponent && !airborne() &&
                !opponent->airborne() && (effective & IN_RIGHT) &&
                std::abs(x-opponent->x) < 61 && (opponent->move_id & 15) < 10 &&
                target + 64 < (int)mvs->moves.size()) target += 64;
            enter_move(target);
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
                if (mv->state_flags & 0x80) facing = -facing;
                if (mv->flags & 4) input_strength = 0;
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
                if (mv->state_flags & 0x80) facing = -facing;
                if (mv->flags & 4) input_strength = 0;
                playing = false;
            } else {
                enter_move(0);
            }
        }
    }
    update_vertical();
    // FUN_23138: MVS bit 0x20 allows held horizontal steering, including air attacks.
    if (move()->flags & 0x20) {
        if (effective & IN_RIGHT) x += 8*facing;
        else if (effective & IN_LEFT) x -= 8*facing;
    }
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
