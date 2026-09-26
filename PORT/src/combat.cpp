#include "combat.h"
#include <algorithm>
#include <cmath>

void ScriptPlayer::start(const EffectScript* data, int px, int py, int direction) {
    script = data && !data->empty() ? data : nullptr;
    cursor = 0; image = 1000; flags = 0; x = px; y = py; facing = direction;
    advance();
}

void ScriptPlayer::advance() {
    // Bound jumps even for a malformed self-loop. Valid loops reach a frame.
    for (int jumps = 0; script && jumps < 32; ++jumps) {
        if (cursor < 0 || cursor >= (int)script->size()) break;
        const auto& entry = (*script)[cursor];
        if (entry.image == -999) break;
        if (entry.image < 0) { cursor += entry.image; continue; }
        ++cursor;
        image = entry.image; flags = entry.flags;
        x += entry.dx * facing; y += entry.dy;
        return;
    }
    script = nullptr; image = 1000;
}

void Combat::reset() {
    phase_ = RoundPhase::Fighting; winner_ = -1; remaining_ = 0;
    death_animation_ = false;
    serial_.fill(~0u); attached_ = {}; projectiles_ = {}; impacts_ = {};
    push_.fill(0); push_direction_.fill(0); next_impact_ = 0; hits_.clear();
}

int Combat::slots(int side) const {
    return (int)std::count_if(projectiles_[side].begin(), projectiles_[side].end(),
                              [](const Projectile& p) { return !p.visual.active(); });
}

bool Combat::usable_finisher(const Fighter& fighter, int target) const {
    return target>=48 && target<=58 && !(target&1) && fighter.has_action(target);
}

void Combat::sync_move(Fighter& fighter, int side) {
    if (serial_[side] == fighter.move_serial) {
        if (fighter.advanced) attached_[side].advance();
        return;
    }
    serial_[side] = fighter.move_serial;
    const auto* move = fighter.move();
    attached_[side] = {};
    if (!move) return;
    const int visual_tier = std::clamp(fighter.attack_strength*3/123,0,2); // FUN_24819
    if (visual_tier < (int)move->attached.size())
        attached_[side].start(&move->attached[visual_tier], 0, 0, fighter.facing);
    if (!move->projectile.empty()) {
        for (auto& p : projectiles_[side]) {
            if (p.visual.active()) continue;
            p = {}; p.move = fighter.move_id; p.impact = &move->impact;
            p.visual.start(&move->projectile, fighter.x, fighter.y, fighter.facing);
            break;
        }
    }
}

static void movement(Fighter& f) {
    const auto* m = f.move();
    if (!f.advanced || !m || f.speed_level >= (int)m->movements.size() ||
        f.seq_pos >= (int)m->movements[f.speed_level].size()) return;
    f.displacement = m->movements[f.speed_level][f.seq_pos] * 2 * f.facing;
    f.x = std::clamp(f.x + f.displacement, 40, 600);
}

bool Combat::hit(Fighter& a, Fighter& v, int side, const std::vector<Cl2Box>& attacks,
                 const std::vector<Cl2Box>& bodies, int direction, Projectile* projectile) {
    if (phase_ == RoundPhase::Ending || phase_ == RoundPhase::Result ||
        (!projectile && (a.hit_move == a.move_id || a.hit_pause > 0)) ||
        (projectile && projectile->hit) || (v.move()->state_flags & 0x10 && a.move_id != 88)) return false;
    const Cl2Box *attack = nullptr, *body = nullptr;
    int depth = -1, px = 0, py = 0;
    for (const auto& ab : attacks) for (const auto& bb : bodies) {
        const int left = std::max(ab.x,bb.x), right = std::min(ab.x+ab.w,bb.x+bb.w);
        const int top = std::max(ab.y,bb.y), bottom = std::min(ab.y+ab.h,bb.y+bb.h);
        if (right >= left && bottom >= top && right-left > depth) {
            attack = &ab; body = &bb; depth = right-left;
            px = (left+right)/2; py = (top+bottom)/2;
        }
    }
    if (!attack) return false;
    int base = attack->damage_or_type, reaction = -1, strength = a.attack_strength;
    if (a.move_id >= 80) {
        strength = a.move()->state_flags & 0x40 ? 50 : 80;
        if (a.move_id == 88 && data_ && a.robot_id < (int)data_->super_strength.size())
            strength = data_->super_strength[a.robot_id];
        if (a.robot_id == 15 && a.move_id == 84) strength = 120;
    }
    if (base < 0) {
        reaction = -base;
        base = ((a.robot_id == 14 && a.move_id == 80) ||
                ((a.robot_id == 2 || a.robot_id == 11) && a.move_id == 82)) ? 12 : 4;
        strength = 50;
    }
    const int part = std::max(body->damage_or_type,1);
    // Source uses a signed 16-bit intermediate for a direct strike.
    int damage = projectile ? (base*part*40*256) >> 15 :
                              (int)(int16_t)(base*part*strength)*a.attack_stat >> 13;
    const bool blocked = projectile ? (v.move_id == 4 || v.move_id == 20) :
        ((v.move_id == 4 && body->damage_or_type == 0) ||
         (v.move_id == 20 && (a.move_id < 32 || a.move_id > 42)));
    if (blocked) damage = projectile ? (base*part*40*256) >> 17 : damage >> 3;
    damage = std::max(damage,1);
    v.health = std::max(0,v.health-damage); v.flash = 3;
    const bool cancel = !(a.move()->state_flags & 0x40);
    if (!projectile && cancel) { a.hit_move = a.move_id; a.combo_cancel = true; }
    if (a.hits_in_move == 0 && (projectile || a.move_id > 79) &&
        (projectile ? projectile->move : a.move_id) != 88)
        a.super_meter = std::min(24,a.super_meter+(blocked?2:4));
    ++a.hits_in_move;
    if (reaction < 0) {
        reaction = data_ && body->region >= 0 && body->region < (int)data_->reactions.size()
                   ? data_->reactions[body->region] : 11;
        if (v.move_id >= 16 && v.move_id < 32) reaction = 27;
        if (blocked) reaction = v.move_id;
        if (v.airborne()) reaction = 43;
        if (data_ && !data_->impacts.empty()) {
            const auto& s = data_->impacts[blocked && data_->impacts.size()>1 ? 1 : 0];
            impacts_[next_impact_++ % impacts_.size()].start(&s,px,py,-v.facing);
        }
    }
    const bool death = reaction >= 49 && reaction <= 59 && (reaction & 1);
    v.force_move(reaction);
    if (cancel && attack->damage_or_type >= 0 && !blocked) a.hit_pause = v.hit_pause = 4;
    push_[1-side] = projectile ? 1 : a.airborne() ? 5 : strength/10+4;
    push_direction_[1-side] = direction;
    if (projectile) {
        projectile->hit = true;
        projectile->visual.start(projectile->impact,projectile->visual.x,projectile->visual.y,
                                  projectile->visual.facing);
    }
    hits_.push_back({side,damage,px,py,blocked,projectile!=nullptr});
    if (phase_ == RoundPhase::Finishing && side == winner_) {
        death_animation_ |= death;
    } else if (phase_ == RoundPhase::FinishWindow && side == winner_) {
        end_round(side == 0 ? a : v,side == 0 ? v : a,false);
    }
    return true;
}

void Combat::end_round(Fighter& a, Fighter& b, bool finished) {
    phase_ = RoundPhase::Ending; remaining_ = finished ? 300 : 100;
    if (winner_ < 0) { a.force_move(15); b.force_move(15); return; }
    Fighter& winner = winner_ == 0 ? a : b;
    Fighter& loser = winner_ == 0 ? b : a;
    if (!finished) loser.force_move(15);
    winner.force_move(64);
}

void Combat::tick(Fighter& a, Fighter& b, uint16_t input_a, uint16_t input_b) {
    hits_.clear();
    if (phase_ == RoundPhase::Result) return;
    for (auto& i : impacts_) i.advance();
    Fighter* fighters[] = {&a,&b};
    uint16_t inputs[] = {input_a,input_b};
    if (phase_ == RoundPhase::Fighting || phase_ == RoundPhase::FinishWindow) update_facing(a,b);
    for (int side = 0; side < 2; ++side) {
        auto& f = *fighters[side];
        if (f.flash > 0) --f.flash;
        for (auto& p : projectiles_[side]) {
            p.visual.advance();
            if (p.visual.active() && ((!p.hit && !(p.visual.flags & 1) && f.move_id < 80 &&
                 !(f.move_id >= 48 && f.move_id <= 58)) || p.visual.x < -60 || p.visual.x > 700)) p = {};
        }
        if (phase_ == RoundPhase::Fighting && !f.airborne()) {
            const auto& enemy = *fighters[1-side];
            const bool back = (inputs[side] & (f.facing > 0 ? IN_LEFT : IN_RIGHT)) != 0;
            const bool threat = (enemy.move()->action_type > 1 && std::abs(f.x-enemy.x) < 141) || slots(1-side) < 3;
            if (back && threat && (f.move_id & 15) < 10 && f.move()->action_type < 2 && !f.hit_pause) {
                const int guard = inputs[side] & IN_DOWN ? 20 : 4;
                if (f.move_id != guard) f.force_move(guard);
            } else if (!back && (f.move_id == 4 || f.move_id == 20)) f.force_move(f.move_id == 20 ? 16 : 0);
        }
        if ((phase_ == RoundPhase::FinishWindow && side != winner_) ||
            phase_ == RoundPhase::Finishing || phase_ == RoundPhase::Ending) inputs[side] = 0;
        const bool finishing = phase_ == RoundPhase::FinishWindow && side == winner_;
        f.step(inputs[side],fighters[1-side],finishing,slots(side));
        movement(f);
        f.x = std::clamp(f.x,40,600); // steering also applies to moves without a displacement stream
        if (push_[side] > 0 && f.advanced) {
            f.x = std::clamp(f.x+push_direction_[side]*(push_[side]*2-1),40,600);
            --push_[side];
        }
        sync_move(f,side);
        if (finishing && usable_finisher(f,f.move_id)) {
            phase_ = RoundPhase::Finishing; remaining_ = 300;
        }
    }
    std::vector<Cl2Box> attack[2], body[2];
    a.get_boxes(&attack[0],&body[0]); b.get_boxes(&attack[1],&body[1]);
    for (int side = 0; side < 2; ++side) {
        auto& f = *fighters[side]; auto& v = *fighters[1-side];
        // During the post-round demonstration, allow separated attack phases
        // of one finishing sequence to reach their later signed CL2 reactions.
        if (phase_ == RoundPhase::Finishing && side == winner_ && attack[side].empty())
            f.hit_move = -1;
        if (phase_ == RoundPhase::Fighting ||
            ((phase_ == RoundPhase::FinishWindow || phase_ == RoundPhase::Finishing) && side == winner_)) {
            hit(f,v,side,attack[side],body[1-side],f.facing);
            for (auto& p : projectiles_[side]) {
                auto& s = p.visual;
                const Cl2Bank* boxes = s.flags & 2 ? extra_boxes_ : f.cl2;
                if (!s.active() || s.image == 1000 || !boxes || s.image >= (int)boxes->frames.size()) continue;
                std::vector<Cl2Box> placed;
                for (const auto& raw : boxes->frames[s.image].attack_boxes) {
                    Cl2Box box = raw;
                    const int bx = raw.x*4 - (s.flags & 2 ? 320 : f.cl2_ref_x);
                    box.x = s.x+(s.facing>0 ? bx : -bx-raw.w*4);
                    box.y = raw.y*2+s.y-(s.flags & 2 ? 200 : f.ground_y);
                    box.w *= 4; box.h *= 2; placed.push_back(box);
                }
                hit(f,v,side,placed,body[1-side],s.facing,&p);
            }
        }
    }
    if (phase_ == RoundPhase::Fighting && (a.health == 0 || b.health == 0)) {
        winner_ = a.health == 0 ? (b.health == 0 ? -1 : 1) : 0;
        if (winner_ < 0) { end_round(a,b,false); return; }
        auto& loser = *fighters[1-winner_]; auto& winner = *fighters[winner_];
        loser.stop_vertical(); loser.force_move(75); loser.hit_pause = 0;
        winner.hit_pause = 0; winner.force_move(0);
        push_.fill(0); projectiles_ = {}; attached_ = {};
        phase_ = RoundPhase::FinishWindow; remaining_ = 200;
    } else if (phase_ != RoundPhase::Fighting) {
        --remaining_;
        if (phase_ == RoundPhase::FinishWindow && remaining_ <= 0) end_round(a,b,false);
        else if (phase_ == RoundPhase::Finishing && (remaining_ <= 0 ||
                 (slots(winner_) == 3 && !fighters[winner_]->playing &&
                  (!death_animation_ || !fighters[1-winner_]->playing)) ||
                 (slots(winner_) == 3 && fighters[winner_]->move_id == 0 &&
                  (!death_animation_ || !fighters[1-winner_]->playing)))) end_round(a,b,death_animation_);
        else if (phase_ == RoundPhase::Ending && (remaining_ <= 0 || (!a.playing && !b.playing)))
            phase_ = RoundPhase::Result;
    }
}

static void draw_script(SDL_Renderer* r, const ScriptPlayer& s, const Fighter& owner,
                        const AtlasBank* extra, int camera, bool attached) {
    if (!s.active() || s.image < 0 || s.image == 1000) return;
    const bool shared = (s.flags & 2) != 0;
    const auto* bank = shared ? extra : owner.atlas;
    const int index = s.image + (shared ? 0 : 1);
    if (!bank || index >= (int)bank->frames.size()) return;
    const auto& f = bank->frames[index];
    if (f.empty || f.page < 0 || f.page >= (int)bank->pages.size()) return;
    const int x = s.x + (attached ? owner.x : 0), y = s.y + (attached ? owner.y : 0);
    const int facing = attached ? owner.facing : s.facing;
    SDL_Rect src{f.rect_x,f.rect_y,f.rect_w,f.rect_h}, dst;
    if (shared) {
        dst = {x-camera+(facing>0 ? f.origin_x-320 : 320-f.origin_x-f.rect_w),
               y+f.origin_y-200,f.rect_w,f.rect_h};
    } else {
        Fighter position = owner; position.x = x; position.y = y; position.facing = facing;
        dst = position.frame_rect(f,camera);
    }
    SDL_RenderCopyEx(r,bank->pages[f.page],&src,&dst,0,nullptr,
                     facing>0 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
}

void Combat::render(SDL_Renderer* r, const Fighter& a, const Fighter& b, int camera) const {
    const Fighter* f[] = {&a,&b};
    for (int side = 0; side < 2; ++side) {
        draw_script(r,attached_[side],*f[side],extra_,camera,true);
        for (const auto& p : projectiles_[side]) draw_script(r,p.visual,*f[side],extra_,camera,false);
    }
    for (const auto& i : impacts_) draw_script(r,i,a,extra_,camera,false);
}

std::string command_notation(const MoveCommand& command) {
    std::string out;
    for (auto it = command.inputs.rbegin(); it != command.inputs.rend(); ++it) {
        if (!out.empty()) out += " ";
        switch (*it) {
        case 0: out += "N"; break; case 1: out += "P"; break; case 2: out += "F"; break;
        case 4: out += "B"; break; case 8: out += "U"; break; case 16: out += "D"; break;
        case 10: out += "UF"; break; case 12: out += "UB"; break; case 18: out += "DF"; break;
        case 20: out += "DB"; break; case 32: out += "K"; break; case 254: out += "*"; break;
        default: out += "?"; break;
        }
    }
    return out;
}

bool command_reachable(const MoveCommand& command) {
    for (int value : command.inputs)
        if (value != 254 && (value > 63 || ((value & 0x21) && (value & 0x1e)))) return false;
    return true;
}

bool command_shadowed(const MvsBank& bank, const MoveCommand& command) {
    for (const auto& earlier : bank.commands) {
        if (&earlier==&command) break;
        if (earlier.target>=88 || (earlier.target>=48 && earlier.target<=58) ||
            earlier.inputs.size()>command.inputs.size() || !command_reachable(earlier)) continue;
        if ((bank.moves[earlier.target].ground_mode==2)!=(bank.moves[command.target].ground_mode==2)) continue;
        bool matches=true;
        for (size_t i=0;i<earlier.inputs.size();++i)
            if (earlier.inputs[i]!=254 && earlier.inputs[i]!=(command.inputs[i]==254 ? 0 : command.inputs[i])) matches=false;
        if (matches) return true;
    }
    return false;
}
