// Entité combattant : position, état (ID de mouvement MVS), avancement de séquence.
#pragma once
#include "assets.h"
#include "SDL.h"
#include <vector>

// Bits d'entrée logiques (6 bits, à nommer précisément via RISE2.CFG + test)
enum InputBits : uint16_t {
    IN_B0 = 0x01, IN_B1 = 0x02, IN_B2 = 0x04, IN_B3 = 0x08, IN_B4 = 0x10, IN_B5 = 0x20,
};

struct Fighter {
    const AtlasBank* atlas = nullptr;
    const MvsBank* mvs = nullptr;

    int x = 0, y = 0;          // ancrage dans l'arène (coordonnées logiques)
    int facing = 1;            // 1 = vers la droite, -1 = vers la gauche
    int move_id = 0;           // ID du mouvement courant (= état, table MVS)
    int seq_pos = 0;           // index dans la séquence courante
    int speed_level = 0;       // 0..2 (3 séquences par mouvement)
    int displacement = 0;      // déplacement accumulé du pas courant (pour vérification)
    bool playing = true;

    // combat (fn_38b72)
    int health = 120;          // PV (max 120)
    int super_meter = 0;       // jauge de super, max 24 (état 0x58)
    int hit_move = -1;         // move_id au moment du dernier coup porté (re-arm par changement d'état)
    int flash = 0;             // frames de flash de hit
    const Cl2Bank* cl2 = nullptr;
    int cl2_ref_x = 294;       // centre x des boîtes CL2 dans le canevas de référence

    void set_banks(const AtlasBank* a, const MvsBank* m) { atlas = a; mvs = m; }

    // avancement d'une frame : entrées -> transition, sinon avance la séquence
    // retourne la cible si transition (ou -1)
    int step(uint16_t inputs);

    // boîtes de la frame courante en coordonnées écran (x relatif au centre du combattant)
    void get_boxes(std::vector<Cl2Box>* attacks, std::vector<Cl2Box>* bodies) const;

    const MvsMove* move() const;
    int current_frame() const;        // image MVS / index CL2 ; atlas = image + 1
    const AtlasFrame* current_atlas_frame() const;
    SDL_Rect frame_rect(const AtlasFrame& frame, int camera_x) const;
};
