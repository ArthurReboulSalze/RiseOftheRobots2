#include "assets.h"
#include "json.hpp"
#include "SDL_image.h"
#include <cstdio>
#include <stdexcept>

using nlohmann::json;

static std::string read_file(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) throw std::runtime_error("File not found: " + path);
    std::string text;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) text.append(buf, n);
    fclose(fp);
    return text;
}

Assets::Assets(SDL_Renderer* renderer, const std::string& extracted_dir)
    : m_renderer(renderer), m_dir(extracted_dir) {}

Assets::~Assets() {
    for (auto& item : m_atlases)
        for (SDL_Texture* texture : item.second.pages) SDL_DestroyTexture(texture);
    for (auto& item : m_videos)
        for (SDL_Texture* texture : item.second.frames) SDL_DestroyTexture(texture);
    for (auto& item : m_ggf) SDL_DestroyTexture(item.second);
}

SDL_Texture* Assets::load_ggf(const std::string& name) {
    auto it = m_ggf.find(name);
    if (it != m_ggf.end()) return it->second;
    const std::string path = m_dir + "/ggf/" + name + ".png";
    SDL_Texture* texture = IMG_LoadTexture(m_renderer, path.c_str());
    if (!texture) throw std::runtime_error("GGF introuvable: " + path);
    m_ggf.emplace(name, texture);
    return texture;
}

const VideoBank* Assets::load_video(const std::string& name) {
    auto it = m_videos.find(name);
    if (it != m_videos.end()) return &it->second;
    json manifest = json::parse(read_file(m_dir + "/video/" + name + "/manifest.json"));
    VideoBank video;
    video.width = manifest["size"][0];
    video.height = manifest["size"][1];
    for (auto& frame : manifest["frames"]) {
        const std::string path = m_dir + "/video/" + name + "/" + frame.get<std::string>();
        SDL_Texture* texture = IMG_LoadTexture(m_renderer, path.c_str());
        if (!texture) {
            for (SDL_Texture* loaded : video.frames) SDL_DestroyTexture(loaded);
            throw std::runtime_error("frame ANI introuvable: " + path);
        }
        video.frames.push_back(texture);
    }
    auto inserted = m_videos.emplace(name, std::move(video));
    return &inserted.first->second;
}

const AtlasBank* Assets::load_atlas(const std::string& bank) {
    auto it = m_atlases.find(bank);
    if (it != m_atlases.end()) return &it->second;

    json m = json::parse(read_file(m_dir + "/sprites/" + bank + "/manifest.json"));

    AtlasBank ab;
    ab.name = m["bank"].get<std::string>();
    ab.source_w = m["source_size"][0];
    ab.source_h = m["source_size"][1];
    ab.frame_count = m["frame_count"];
    for (auto& f : m["frames"]) {
        AtlasFrame af;
        af.id = f["id"];
        af.origin_x = f["origin"][0];
        af.origin_y = f["origin"][1];
        af.rect_x = f["rect"][0];
        af.rect_y = f["rect"][1];
        af.rect_w = f["rect"][2];
        af.rect_h = f["rect"][3];
        af.page = f["page"];
        af.empty = f["empty"];
        ab.frames.push_back(af);
    }
    for (auto& p : m["pages"]) {
        std::string png = m_dir + "/sprites/" + bank + "/" + p.get<std::string>();
        SDL_Texture* tex = IMG_LoadTexture(m_renderer, png.c_str());
        if (!tex) throw std::runtime_error("atlas introuvable: " + png);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        ab.pages.push_back(tex);
    }
    auto ins = m_atlases.emplace(bank, std::move(ab));
    return &ins.first->second;
}

const MvsBank* Assets::load_mvs(const std::string& bank) {
    auto it = m_mvs.find(bank);
    if (it != m_mvs.end()) return &it->second;

    json m = json::parse(read_file(m_dir + "/data/mvs/" + bank + ".json"));
    MvsBank mb;
    for (auto& mv : m["moves"]) {
        MvsMove mo;
        mo.index = mv["index"];
        mo.flags = (uint8_t)strtoul(mv["flags"].get<std::string>().c_str(), nullptr, 16);
        mo.auto_move = mv["auto_move"];
        mo.resume_index = mv["resume_index"];
        mo.param = mv["param"];
        for (auto& s : mv["sequences"]) {
            std::vector<MvsSeqEntry> seq;
            for (auto& e : s) {
                MvsSeqEntry se;
                se.end = e.value("end", false);
                se.image = e.value("image", -1);
                se.ctrl = e.value("ctrl", 0);
                seq.push_back(se);
            }
            mo.sequences.push_back(std::move(seq));
        }
        // movements: three int16 arrays, one step per sequence image
        mo.movements.clear();
        for (auto& arr : mv["movements"]) {
            std::vector<int16_t> steps;
            for (auto& v : arr) steps.push_back((int16_t)v);
            mo.movements.push_back(std::move(steps));
        }
        for (auto& t : mv["transitions"]) {
            mo.transitions.push_back({ t["mask"], t["target"] });
        }
        mb.moves.push_back(std::move(mo));
    }
    auto ins = m_mvs.emplace(bank, std::move(mb));
    return &ins.first->second;
}
const Cl2Bank* Assets::load_cl2(const std::string& robot_letter) {
    auto it = m_cl2.find(robot_letter);
    if (it != m_cl2.end()) return &it->second;
    json m = json::parse(read_file(m_dir + "/data/cl2/" + robot_letter + ".json"));
    Cl2Bank cb;
    cb.name = robot_letter;
    cb.count = m["count"];
    for (auto& r : m["records"]) {
        Cl2Frame fr;
        for (auto& b : r["attack_boxes"])
            fr.attack_boxes.push_back({ b["x"], b["y"], b["w"], b["h"], b["damage_or_type"] });
        for (auto& b : r["body_boxes"])
            fr.body_boxes.push_back({ b["x"], b["y"], b["w"], b["h"], b["part"] });
        for (auto& b : r["single_box"])
            fr.single_box.push_back({ b["x"], b["y"], b["w"], b["h"], b["tag"] });
        cb.frames.push_back(std::move(fr));
    }
    auto ins = m_cl2.emplace(robot_letter, std::move(cb));
    return &ins.first->second;
}
