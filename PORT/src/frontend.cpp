#include "frontend.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr SDL_Color blue{176, 202, 245, 255};
constexpr SDL_Color gold{255, 142, 76, 255};
constexpr SDL_Color grey{92, 101, 120, 255};
constexpr SDL_Color cyan{142, 222, 248, 255};
}

Frontend::Frontend(SDL_Renderer* renderer, Assets& assets, const std::string& assets_dir)
    : renderer_(renderer), font_(renderer, assets_dir),
      intro_(assets.load_video("LLOGO")), portraits_(assets.load_atlas("VSFACE")),
      title_(assets.load_ggf("MAINSCR")), versus_(assets.load_ggf("VS")) {
    if (intro_->frames.empty() || (portraits_->frames.size() != 28 && portraits_->frames.size() != 30))
        throw std::runtime_error("Incomplete intro ANI or VSFACE portraits");
    robot_count_ = static_cast<int>(portraits_->frames.size());
}

bool Frontend::take_match_request() {
    const bool requested = match_requested_;
    match_requested_ = false;
    return requested;
}

void Frontend::return_to_select() {
    screen_ = Screen::Select;
}

void Frontend::key(SDL_Keycode key) {
    if (screen_ == Screen::Intro) {
        screen_ = Screen::Title;
        return;
    }
    if (screen_ == Screen::Title) {
        if (key == SDLK_UP || key == SDLK_DOWN) title_choice_ = 1 - title_choice_;
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
            if (title_choice_ == 0) screen_ = Screen::Select;
            else quit_requested_ = true;
        } else if (key == SDLK_ESCAPE) quit_requested_ = true;
        else if (key == SDLK_i) {
            intro_frame_ = 0;
            intro_elapsed_ = 0.0;
            screen_ = Screen::Intro;
        }
        return;
    }
    if (screen_ == Screen::Select) {
        const int count = robot_count_;
        if (key == SDLK_LEFT) selected_[0] = (selected_[0] + count - 1) % count;
        else if (key == SDLK_RIGHT) selected_[0] = (selected_[0] + 1) % count;
        else if (key == SDLK_a || key == SDLK_q) selected_[1] = (selected_[1] + count - 1) % count;
        else if (key == SDLK_d) selected_[1] = (selected_[1] + 1) % count;
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
            screen_ = Screen::Fight;
            match_requested_ = true;
        } else if (key == SDLK_ESCAPE) screen_ = Screen::Title;
    }
}

double Frontend::intro_frame_duration() const {
    if (intro_frame_ == 0) return 1.5; // Legal card; the DOS palette faded in gradually.
    if (intro_frame_ < 12) return 0.22;
    if (intro_frame_ + 1 == static_cast<int>(intro_->frames.size())) return 0.8;
    return 0.045;
}

void Frontend::update(double elapsed_seconds) {
    if (screen_ != Screen::Intro) return;
    intro_elapsed_ += std::min(elapsed_seconds, 0.25);
    while (screen_ == Screen::Intro && intro_elapsed_ >= intro_frame_duration()) {
        intro_elapsed_ -= intro_frame_duration();
        ++intro_frame_;
        if (intro_frame_ >= static_cast<int>(intro_->frames.size())) screen_ = Screen::Title;
    }
}

void Frontend::draw_background(SDL_Texture* texture) const {
    int width = 0, height = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
    SDL_Rect source{(width - 640) / 2, 0, 640, 400};
    SDL_Rect target{0, 0, 640, 400};
    SDL_RenderCopy(renderer_, texture, &source, &target);
}

void Frontend::draw_title() const {
    draw_background(title_);
    const char* items[] = {"START", "SETUP", "HIGH SCORE", "CREDITS", "QUIT"};
    for (int i = 0; i < 5; ++i) {
        SDL_Color color = (i == 0 || i == 4)
            ? ((i == (title_choice_ == 0 ? 0 : 4)) ? gold : blue) : grey;
        font_.draw(items[i], 320, 250 + i * 29, color, 16, 21, true);
    }
}

void Frontend::draw_portrait(int player_index) const {
    const AtlasFrame& frame = portraits_->frames[portrait_index(selected_[player_index])];
    if (frame.empty) return;
    const int center_x = player_index == 0 ? 160 : 480;
    SDL_Rect panel{center_x - 145, 88, 290, 196};
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 105);
    SDL_RenderFillRect(renderer_, &panel);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    const double scale = std::min(280.0 / frame.rect_w, 190.0 / frame.rect_h);
    const int width = static_cast<int>(std::round(frame.rect_w * scale));
    const int height = static_cast<int>(std::round(frame.rect_h * scale));
    SDL_Rect source{frame.rect_x, frame.rect_y, frame.rect_w, frame.rect_h};
    SDL_Rect target{center_x - width / 2, 284 - height, width, height};
    const SDL_RendererFlip flip = player_index == 0 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    SDL_RenderCopyEx(renderer_, portraits_->pages[frame.page], &source, &target, 0, nullptr, flip);
    SDL_Color color = player_index == 0 ? cyan : gold;
    font_.draw(kRoster[selected_[player_index]].name, center_x, 292, color, 15, 20, true);
    std::string slot = std::string("ROBOT ") + kRoster[selected_[player_index]].slot;
    font_.draw(slot, center_x, 317, blue, 11, 15, true);
}

void Frontend::draw_select() const {
    draw_background(versus_);
    font_.draw("SELECT ROBOTS", 320, 6, blue, 15, 20, true);
    draw_portrait(0);
    draw_portrait(1);
    font_.draw("<", 14, 209, cyan, 18, 25);
    font_.draw(">", 610, 209, gold, 18, 25);
    font_.draw("P1 ARROWS    P2 A/D    ENTER FIGHT    ESC MENU",
               320, 369, blue, 9, 14, true);
}

void Frontend::render() const {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    if (screen_ == Screen::Intro) {
        SDL_Rect target{0, 0, 640, 400};
        SDL_RenderCopy(renderer_, intro_->frames[intro_frame_], nullptr, &target);
    } else if (screen_ == Screen::Title) draw_title();
    else if (screen_ == Screen::Select) draw_select();
}
