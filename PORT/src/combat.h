#pragma once
#include "fighter.h"
#include <array>

// MVS visual instructions (FUN_244eb/FUN_24926). Coordinates accumulate,
// negative images jump back by records, 1000 delays, -999 stops.
struct ScriptPlayer {
    const EffectScript* script = nullptr;
    int cursor = 0, image = 1000, flags = 0, x = 0, y = 0, facing = 1;
    void start(const EffectScript* data, int px, int py, int direction);
    void advance();
    bool active() const { return script != nullptr; }
};

struct Projectile {
    ScriptPlayer visual;
    const EffectScript* impact = nullptr;
    int move = 0;
    bool hit = false;
};

struct CombatHit { int attacker, damage, x, y; bool blocked, projectile; };
enum class RoundPhase { Fighting, FinishWindow, Finishing, Ending, Result };

class Combat {
public:
    Combat(const CombatData* data, const AtlasBank* extra, const Cl2Bank* extra_boxes)
        : data_(data), extra_(extra), extra_boxes_(extra_boxes) {}
    void reset();
    void tick(Fighter& a, Fighter& b, uint16_t input_a, uint16_t input_b);
    void render(SDL_Renderer* renderer, const Fighter& a, const Fighter& b, int camera_x) const;
    RoundPhase phase() const { return phase_; }
    int winner() const { return winner_; } // -1 for a simultaneous knockout
    int remaining_ticks() const { return remaining_; }
    const std::vector<CombatHit>& hits() const { return hits_; }
    const std::array<std::array<Projectile,3>,2>& projectiles() const { return projectiles_; }
    bool usable_finisher(const Fighter& fighter, int target) const;
    std::vector<int> finishers(const Fighter& fighter) const;
    int assisted_finishing_distance(const Fighter& fighter, const Fighter& victim, int target) const;
    void set_easy_finishings(bool enabled) { easy_finishings_=enabled; }
    // Raw configured attack buttons: punch light/medium/heavy, then kicks.
    void sample_attack_buttons(uint8_t a,uint8_t b);

private:
    const CombatData* data_;
    const AtlasBank* extra_;
    const Cl2Bank* extra_boxes_;
    RoundPhase phase_ = RoundPhase::Fighting;
    int winner_ = -1, remaining_ = 0;
    bool death_animation_ = false;
    bool easy_finishings_ = false;
    std::array<uint8_t,2> held_buttons_{},pressed_buttons_{};
    std::array<unsigned,2> serial_{{~0u,~0u}};
    std::array<ScriptPlayer,2> attached_;
    std::array<std::array<Projectile,3>,2> projectiles_;
    std::array<ScriptPlayer,4> impacts_;
    int next_impact_ = 0;
    std::array<int,2> push_{{0,0}}, push_direction_{{0,0}};
    std::vector<CombatHit> hits_;
    int slots(int side) const;
    void sync_move(Fighter& fighter, int side);
    void end_round(Fighter& a, Fighter& b, bool finished);
    bool hit(Fighter& attacker, Fighter& victim, int side,
             const std::vector<Cl2Box>& attacks, const std::vector<Cl2Box>& bodies,
             int direction, Projectile* projectile = nullptr);
};

std::string command_notation(const MoveCommand& command);
bool command_reachable(const MoveCommand& command);
bool command_shadowed(const MvsBank& bank, const MoveCommand& command);
