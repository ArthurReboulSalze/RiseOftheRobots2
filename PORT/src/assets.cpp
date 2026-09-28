#include "assets.h"
#include "json.hpp"
#include "SDL_image.h"
#include <cstdio>
#include <algorithm>
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
    : m_renderer(renderer), m_dir(extracted_dir) {
    const std::string path=m_dir+"/ui/initial_power_icons.json";
    FILE* file=fopen(path.c_str(),"rb");
    if (file) {
        fclose(file);
        const auto data=json::parse(read_file(path));
        for (auto it=data["initial_power_icons"].begin();it!=data["initial_power_icons"].end();++it) {
            if (it.key().size()!=1 || !it.value().is_number_integer())
                throw std::runtime_error("Invalid power-icon mapping: "+path);
            const int index=it.value().get<int>();
            if (index<0 || index>5) throw std::runtime_error("Invalid power-icon index: "+path);
            m_initial_power_icons.emplace(it.key()[0],index);
        }
    }
}

int Assets::initial_power_icon(char slot) const {
    const auto found=m_initial_power_icons.find(slot);
    return found==m_initial_power_icons.end() ? -1 : found->second;
}

const CombatData* Assets::load_combat() {
    if (m_combat_loaded) return &m_combat;
    const auto data = json::parse(read_file(m_dir + "/data/combat.json"));
    for (const auto& frames : data["impacts"]) {
        EffectScript script;
        for (const auto& f : frames) script.push_back({f["image"], f["dx"], f["dy"], f["flags"]});
        m_combat.impacts.push_back(std::move(script));
    }
    m_combat.particles = data["particles"].get<std::vector<std::vector<int>>>();
    m_combat.super_strength = data["super_strength"].get<std::vector<int>>();
    m_combat.reactions = data["reactions"].get<std::vector<int>>();
    if (data.contains("finishing_distance_hints"))
        m_combat.finishing_distance_hints=data["finishing_distance_hints"].get<std::vector<std::vector<int>>>();
    m_combat_loaded = true;
    return &m_combat;
}

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

const VideoBank* Assets::load_video(const std::string& name, bool preload) {
    auto it = m_videos.find(name);
    if (it != m_videos.end()) return &it->second;
    json manifest = json::parse(read_file(m_dir + "/video/" + name + "/manifest.json"));
    VideoBank video;
    video.streamed=!preload;
    video.width = manifest["size"][0];
    video.height = manifest["size"][1];
    video.playable_frames=manifest.value("playable_frame_count",(int)manifest["frames"].size());
    if (manifest.contains("frame_ticks")) {
        const double hz=manifest.value("timing_hz",100.0);
        if (hz<=0) throw std::runtime_error("Invalid video clock: "+name);
        for (double ticks : manifest["frame_ticks"])
            video.frame_seconds.push_back(std::max(ticks,1.0)/hz);
    }
    for (auto& frame : manifest["frames"]) {
        const std::string path = m_dir + "/video/" + name + "/" + frame.get<std::string>();
        video.frame_paths.push_back(path);
        SDL_Texture* texture = preload ? IMG_LoadTexture(m_renderer, path.c_str()):nullptr;
        if (preload && !texture) {
            for (SDL_Texture* loaded : video.frames) SDL_DestroyTexture(loaded);
            throw std::runtime_error("frame ANI introuvable: " + path);
        }
        video.frames.push_back(texture);
    }
    video.playable_frames=std::min(video.playable_frames,(int)video.frames.size());
    if (video.playable_frames<=0) throw std::runtime_error("Empty video: "+name);
    auto inserted = m_videos.emplace(name, std::move(video));
    return &inserted.first->second;
}

SDL_Texture* Assets::video_frame(const std::string& name, int index) {
    auto it=m_videos.find(name);
    if (it==m_videos.end() || index<0 || index>=(int)it->second.frames.size()) return nullptr;
    auto& movie=it->second;
    if (!movie.streamed) return movie.frames[index];
    if (movie.loaded_frame==index) return movie.frames[index];
    SDL_Texture* next=IMG_LoadTexture(m_renderer,movie.frame_paths[index].c_str());
    if (!next) throw std::runtime_error("Missing video frame: "+movie.frame_paths[index]);
    if (movie.loaded_frame>=0) {
        SDL_DestroyTexture(movie.frames[movie.loaded_frame]);movie.frames[movie.loaded_frame]=nullptr;
    }
    movie.loaded_frame=index;movie.frames[index]=next;
    return next;
}

void Assets::unload_video(const std::string& name) {
    auto it=m_videos.find(name);
    if (it==m_videos.end()) return;
    for (SDL_Texture* frame : it->second.frames) SDL_DestroyTexture(frame);
    m_videos.erase(it);
}

std::vector<MovieInfo> Assets::movie_catalog() const {
    std::vector<MovieInfo> out;
    FILE* fp=fopen((m_dir+"/video/catalog.json").c_str(),"rb");
    if (!fp) return out;
    fclose(fp);
    const auto catalog=json::parse(read_file(m_dir+"/video/catalog.json"));
    for (const auto& movie : catalog["movies"]) {
        const std::string name=movie["name"];
        // Names become file paths; do not allow an imported catalogue to escape.
        if (name.empty() || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")!=std::string::npos)
            throw std::runtime_error("Invalid movie name");
        out.push_back({name,movie.value("kind",std::string("other")),movie.value("robot_slot",std::string()),
            movie["size"][0],movie["size"][1],movie.value("playable_frame_count",movie.value("frame_count",0)),
            movie.value("placeholder",false),movie.value("title",name)});
    }
    return out;
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
    auto script = [](const json& frames) {
        EffectScript out;
        for (const auto& f : frames) out.push_back({f["image"], f["dx"], f["dy"], f["flags"]});
        return out;
    };
    if (m.contains("commands"))
        for (const auto& c : m["commands"])
            mb.commands.push_back({c["inputs"].get<std::vector<int>>(), c["target"]});
    for (auto& mv : m["moves"]) {
        MvsMove mo;
        mo.index = mv["index"];
        mo.flags = (uint8_t)strtoul(mv["flags"].get<std::string>().c_str(), nullptr, 16);
        mo.auto_move = mv["auto_move"];
        mo.resume_index = mv["resume_index"];
        mo.param = mv["param"];
        if (mv.contains("effects")) {
            const auto& e = mv["effects"];
            for (const auto& s : e["attached"]) mo.attached.push_back(script(s));
            mo.projectile = script(e["projectile"]);
            mo.impact = script(e["impact"]);
        }
        if (mv.contains("state")) {
            const auto& state = mv["state"];
            mo.ground_mode = state["ground_mode"];
            mo.action_type = state["action_type"];
            mo.gravity = state["gravity"];
            mo.state_flags = state["flags"];
        }
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
            fr.attack_boxes.push_back({ b["x"], b["y"], b["w"], b["h"], (int)(int8_t)b["damage_or_type"].get<int>() });
        for (auto& b : r["body_boxes"])
            fr.body_boxes.push_back({ b["x"], b["y"], b["w"], b["h"], b["part"], b["p4"] });
        for (auto& b : r["single_box"])
            fr.single_box.push_back({ b["x"], b["y"], b["w"], b["h"], b["tag"] });
        cb.frames.push_back(std::move(fr));
    }
    auto ins = m_cl2.emplace(robot_letter, std::move(cb));
    return &ins.first->second;
}
