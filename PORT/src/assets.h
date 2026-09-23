// Chargeurs d'assets pour le port : atlas de sprites (sorties d'extraction) + banques MVS.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "SDL.h"

struct AtlasFrame {
    int id;                 // index dans la banque (= l'"image" des séquences MVS)
    int origin_x, origin_y; // position de la bbox dans le canevas source
    int rect_x, rect_y, rect_w, rect_h; // emplacement dans l'atlas
    int page;               // atlas_XXX.png
    bool empty;
};

struct AtlasBank {
    std::string name;
    int source_w = 0, source_h = 0;   // 320x200 (RB4) ou 640x400 (RBT)
    int frame_count = 0;
    std::vector<AtlasFrame> frames;
    std::vector<SDL_Texture*> pages;  // RGBA déjà coloré
};

struct VideoBank {
    int width = 0, height = 0;
    std::vector<SDL_Texture*> frames;
};

// Banque de mouvements MVS convertie en JSON
struct MvsTransition { uint16_t mask, target; };
struct MvsSeqEntry { int image; int ctrl; bool end; };
struct MvsMove {
    int index;
    std::vector<std::vector<MvsSeqEntry>> sequences; // 3 vitesses
    std::vector<std::vector<int16_t>> movements;     // 3 déplacements (par pas)
    std::vector<MvsTransition> transitions;
    uint8_t auto_move, resume_index, param;
    uint8_t flags;                     // octet +0x1C du descripteur MVS
};

struct MvsBank {
    std::vector<MvsMove> moves;
};

// Boîtes de collision CL2 (sémantique validée : attaque 5 o / corps 6 o / unique 5 o)
struct Cl2Box { int x, y, w, h, damage_or_type; };
struct Cl2Frame {
    std::vector<Cl2Box> attack_boxes;   // boîtes d'attaque (x,y,w,h,dégât)
    std::vector<Cl2Box> body_boxes;     // boîtes de corps
    std::vector<Cl2Box> single_box;     // boîte unique (poussée/empreinte)
};
struct Cl2Bank {
    std::string name;
    int count = 0;
    std::vector<Cl2Frame> frames;
};

class Assets {
public:
    Assets(SDL_Renderer* renderer, const std::string& extracted_dir);
    ~Assets();
    const AtlasBank* load_atlas(const std::string& bank);
    const VideoBank* load_video(const std::string& name);
    SDL_Texture* load_ggf(const std::string& name);
    const MvsBank* load_mvs(const std::string& bank);
    const Cl2Bank* load_cl2(const std::string& robot_letter);
    const std::string& dir() const { return m_dir; }

private:
    SDL_Renderer* m_renderer;
    std::string m_dir;
    std::unordered_map<std::string, AtlasBank> m_atlases;
    std::unordered_map<std::string, VideoBank> m_videos;
    std::unordered_map<std::string, SDL_Texture*> m_ggf;
    std::unordered_map<std::string, MvsBank> m_mvs;
    std::unordered_map<std::string, Cl2Bank> m_cl2;
};
