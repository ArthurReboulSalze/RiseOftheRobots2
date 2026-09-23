// Port asset loaders: extracted sprite atlases and MVS banks.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "SDL.h"

struct AtlasFrame {
    int id;                 // bank index (the "image" referenced by MVS sequences)
    int origin_x, origin_y; // bounding-box position in the source canvas
    int rect_x, rect_y, rect_w, rect_h; // position in the atlas
    int page;               // atlas_XXX.png
    bool empty;
};

struct AtlasBank {
    std::string name;
    int source_w = 0, source_h = 0;   // 320x200 (RB4) or 640x400 (RBT)
    int frame_count = 0;
    std::vector<AtlasFrame> frames;
    std::vector<SDL_Texture*> pages;  // RGBA, already colorized
};

struct VideoBank {
    int width = 0, height = 0;
    std::vector<SDL_Texture*> frames;
};

// MVS movement bank converted to JSON.
struct MvsTransition { uint16_t mask, target; };
struct MvsSeqEntry { int image; int ctrl; bool end; };
struct MvsMove {
    int index;
    std::vector<std::vector<MvsSeqEntry>> sequences; // three speed levels
    std::vector<std::vector<int16_t>> movements;     // three displacement streams (per step)
    std::vector<MvsTransition> transitions;
    uint8_t auto_move, resume_index, param;
    uint8_t flags;                     // byte +0x1c of the MVS descriptor
};

struct MvsBank {
    std::vector<MvsMove> moves;
};

// CL2 collision boxes: attack 5 bytes, body 6 bytes, single 5 bytes
struct Cl2Box { int x, y, w, h, damage_or_type; };
struct Cl2Frame {
    std::vector<Cl2Box> attack_boxes;   // attack boxes (x, y, w, h, damage)
    std::vector<Cl2Box> body_boxes;     // body boxes
    std::vector<Cl2Box> single_box;     // single box (push/footprint)
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
