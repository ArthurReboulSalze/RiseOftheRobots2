#include "announcer.h"
#include "audio.h"
#include "json.hpp"
#include "SDL_mixer.h"
#include <fstream>
#include <cstdio>
#include <filesystem>

Announcer::Announcer(const std::string& assets_dir):root_(assets_dir+"/audio/") {
    for (char slot : std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123")) {
        const std::string bank=std::string("R")+slot;
        const std::string win="mrw/"+bank+"/"+bank+"_14.wav";
        if (std::filesystem::exists(root_+win)) victories_[slot]=win;
    }
    std::ifstream stream(root_+"voices/manifest.json");
    if (!stream) return;
    try {
        const auto manifest=nlohmann::json::parse(stream);
        for (const auto& robot : manifest.at("robots").items()) {
            if (robot.key().size()!=1) continue;
            const char slot=robot.key()[0];
            const auto name=robot.value().value("name",std::string());
            const auto win=robot.value().value("victory",std::string());
            if (!name.empty()) names_[slot]="voices/"+name;
            if (!win.empty()) victories_[slot]="voices/"+win;
        }
        for (const auto& event : manifest.at("events").items()) events_[event.key()]="voices/"+event.value().get<std::string>();
    } catch (const std::exception& error) {
        names_.clear();events_.clear();
        fprintf(stderr,"Optional announcer: %s\n",error.what());
    }
}

Announcer::~Announcer() {
    clear();
    for (auto& chunk : chunks_) if (chunk.second) Mix_FreeChunk(chunk.second);
}

void Announcer::clear() {
    queue_.clear();Mix_HaltChannel(0);
}

void Announcer::enqueue(const std::string& file) {
    if (!file.empty() && file[0]!='/' && file.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./") == std::string::npos &&
        file.find("..") == std::string::npos) queue_.push_back(file);
}

void Announcer::select(char slot) {
    clear();enqueue(names_[slot]);
}

void Announcer::fight() {
    clear();enqueue(events_["fight"]);
}

void Announcer::victory(char slot) {
    clear();
    if (!victories_[slot].empty()) enqueue(victories_[slot]);
    else {
        // Keep whole original recordings. Do not synthesize/cut an unverified
        // '<robot> wins' phrase: the supplied fallback says 'You win'.
        enqueue(names_[slot]);enqueue(events_["victory"]);
    }
}

void Announcer::update() {
    if (Mix_Playing(0)) return;
    while (!queue_.empty()) {
        const std::string file=queue_.front();queue_.pop_front();
        auto it=chunks_.find(file);
        if (it==chunks_.end()) it=chunks_.emplace(file,load_effect_wav((root_+file).c_str())).first;
        if (it->second && Mix_PlayChannel(0,it->second,0)>=0) return;
    }
}
