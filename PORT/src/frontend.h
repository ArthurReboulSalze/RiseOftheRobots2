#pragma once

#include "assets.h"
#include "roster.h"
#include "ui.h"

enum class Screen { Intro, Title, Select, Fight };

class Frontend {
public:
    Frontend(SDL_Renderer* renderer, Assets& assets, const std::string& assets_dir);

    Screen screen() const { return screen_; }
    const RobotInfo& player(int index) const { return kRoster[selected_[index]]; }
    bool quit_requested() const { return quit_requested_; }
    bool take_match_request();
    void return_to_select();
    void key(SDL_Keycode key);
    void update(double elapsed_seconds);
    void render() const;

private:
    SDL_Renderer* renderer_;
    UiFont font_;
    const VideoBank* intro_;
    const AtlasBank* portraits_;
    SDL_Texture* title_;
    SDL_Texture* versus_;
    Screen screen_ = Screen::Intro;
    int intro_frame_ = 0;
    double intro_elapsed_ = 0.0;
    int title_choice_ = 0;
    int selected_[2] = {1, 7}; // LOADER / DEADLIFT pour le premier essai
    int robot_count_ = 28;
    bool match_requested_ = false;
    bool quit_requested_ = false;

    double intro_frame_duration() const;
    void draw_title() const;
    void draw_select() const;
    void draw_portrait(int player_index) const;
    void draw_background(SDL_Texture* texture) const;
};
