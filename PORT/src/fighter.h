// Fighter entity: position, state (MVS movement ID), and sequence position.
#pragma once
#include "assets.h"
#include "SDL.h"
#include <vector>

// Logical input bits (six bits; exact names require RISE2.CFG and playtesting).
enum InputBits : uint16_t {
    IN_B0 = 0x01, IN_B1 = 0x02, IN_B2 = 0x04, IN_B3 = 0x08, IN_B4 = 0x10, IN_B5 = 0x20,
};

struct Fighter {
    const AtlasBank* atlas = nullptr;
    const MvsBank* mvs = nullptr;

    int x = 0, y = 0;          // arena anchor (logical coordinates)
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
    int cl2_ref_x = 294;       // CL2 box center x in the reference canvas

    void set_banks(const AtlasBank* a, const MvsBank* m) { atlas = a; mvs = m; }

    // Advance one frame: input transition, otherwise next sequence step.
    // Return the target on a transition, or -1.
    int step(uint16_t inputs);

    // Current-frame boxes in screen coordinates (x relative to fighter center).
    void get_boxes(std::vector<Cl2Box>* attacks, std::vector<Cl2Box>* bodies) const;

    const MvsMove* move() const;
    int current_frame() const;        // MVS image / CL2 index; atlas = image + 1
    const AtlasFrame* current_atlas_frame() const;
    SDL_Rect frame_rect(const AtlasFrame& frame, int camera_x) const;
};
