// Fighter entity: position, state (MVS movement ID), and sequence position.
#pragma once
#include "assets.h"
#include "SDL.h"
#include <vector>

// Public inputs use screen directions. MVS masks use forward/back after facing.
enum InputBits : uint16_t {
    IN_PUNCH = 0x01, IN_RIGHT = 0x02, IN_LEFT = 0x04,
    IN_UP = 0x08, IN_DOWN = 0x10, IN_KICK = 0x20,
};

struct Fighter {
    const AtlasBank* atlas = nullptr;
    const MvsBank* mvs = nullptr;

    int x = 0, y = 312;        // arena anchor (logical coordinates)
    int ground_y = 312;
    int facing = 1;            // 1 faces right, -1 faces left
    int move_id = 0;           // current movement ID (state in the MVS table)
    int seq_pos = 0;           // position in the current sequence
    int speed_level = 0;       // 0..2 (three sequences per movement)
    int displacement = 0;      // accumulated current-step displacement (for verification)
    bool playing = true;

    // Combat (fn_38b72).
    int health = 120;          // HP (maximum 120)
    int super_meter = 0;       // super meter, maximum 24 (state 0x58)
    int hit_move = -1;         // move ID at last hit (re-arm on state change)
    int flash = 0;             // remaining hit-flash frames
    const Cl2Bank* cl2 = nullptr;
    double sprite_scale = 1.0;   // 2.0 pour les banques RB4 (320->640), <1 pour un repli RBT
    int cl2_ref_x = 294;       // CL2 box center x in the reference canvas

    void set_banks(const AtlasBank* a, const MvsBank* m) { atlas = a; mvs = m; }

    // Sample every render iteration so a tap between simulation ticks survives.
    void sample_inputs(uint16_t inputs);
    bool airborne() const { return y < ground_y || vertical_velocity != 0; }

    // Advance one frame: input transition, otherwise next sequence step.
    // Return the target on a transition, or -1.
    int step(uint16_t inputs);

    // Current-frame boxes in screen coordinates (x relative to fighter center).
    void get_boxes(std::vector<Cl2Box>* attacks, std::vector<Cl2Box>* bodies) const;

    const MvsMove* move() const;
    int current_frame() const;        // MVS image / CL2 index; atlas = image + 1
    const AtlasFrame* current_atlas_frame() const;
    SDL_Rect frame_rect(const AtlasFrame& frame, int camera_x) const;

private:
    uint16_t held_inputs = 0, pressed_inputs = 0;
    int vertical_velocity = 0; // signed 8.8 DOS velocity
    int vertical_fraction = 0, vertical_gravity = 0;
    int repeat_count = 0;
    bool move_started = false;
    void enter_move(int target, int first_frame = 0);
    void update_vertical();
    void turn_to(int direction);
    friend void update_facing(Fighter& a, Fighter& b);
};

// FUN_25615: update once at the start of the combat tick, before inputs/motion.
void update_facing(Fighter& a, Fighter& b);
