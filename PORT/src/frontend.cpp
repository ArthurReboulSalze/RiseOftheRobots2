#include "frontend.h"
#include "SDL.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
constexpr SDL_Color blue{176, 202, 245, 255};
constexpr SDL_Color gold{255, 142, 76, 255};
constexpr SDL_Color grey{92, 101, 120, 255};
constexpr SDL_Color cyan{142, 222, 248, 255};
constexpr SDL_Color red{255, 60, 40, 255};

constexpr const char* kTitleItems[5] = {"START", "KEY MAPPING", "HIGH SCORE", "CREDITS", "QUIT"};
constexpr const char* kEntryNames[10] = {
    "UP", "DOWN", "LEFT", "RIGHT", "01", "02", "03", "P1", "P2", "P3"};

// Table scancodes DOS set 1 -> nom affichable.
struct DosKeyName { uint16_t code; const char* name; };
constexpr DosKeyName kDosKeyNames[] = {
    {0x01, "ESC"}, {0x02, "1"}, {0x03, "2"}, {0x04, "3"}, {0x05, "4"}, {0x06, "5"},
    {0x07, "6"}, {0x08, "7"}, {0x09, "8"}, {0x0A, "9"}, {0x0B, "0"}, {0x0C, "-"},
    {0x0D, "="}, {0x0E, "BKSP"}, {0x0F, "TAB"}, {0x10, "Q"}, {0x11, "W"}, {0x12, "E"},
    {0x13, "R"}, {0x14, "T"}, {0x15, "Y"}, {0x16, "U"}, {0x17, "I"}, {0x18, "O"},
    {0x19, "P"}, {0x1A, "["}, {0x1B, "]"}, {0x1C, "ENTER"}, {0x1D, "CTRL"},
    {0x1E, "A"}, {0x1F, "S"}, {0x20, "D"}, {0x21, "F"}, {0x22, "G"}, {0x23, "H"},
    {0x24, "J"}, {0x25, "K"}, {0x26, "L"}, {0x27, ";"}, {0x28, "'"}, {0x29, "`"},
    {0x2A, "LSHIFT"}, {0x2B, "BACKSLASH"}, {0x2C, "Z"}, {0x2D, "X"}, {0x2E, "C"},
    {0x2F, "V"}, {0x30, "B"}, {0x31, "N"}, {0x32, "M"}, {0x33, ","}, {0x34, "."},
    {0x35, "/"}, {0x36, "RSHIFT"}, {0x38, "ALT"}, {0x39, "SPACE"},
    {0x3A, "F1"}, {0x3B, "F2"}, {0x3C, "F3"}, {0x3D, "F4"}, {0x3E, "F5"}, {0x3F, "F6"},
    {0x40, "F7"}, {0x41, "F8"}, {0x42, "F9"}, {0x43, "F10"}, {0x44, "F11"}, {0x45, "F12"},
    {0x47, "HOME"}, {0x48, "UP"}, {0x49, "PGUP"}, {0x4B, "LEFT"}, {0x4C, "KP5"},
    {0x4D, "RIGHT"}, {0x4F, "END"}, {0x50, "DOWN"}, {0x51, "PGDN"},
    {0x52, "INS"}, {0x53, "DEL"},
};

// SDL scancode -> scancode DOS (set 1).SDL_Keycode pour les lettres/chiffres = ASCII.
struct SdlDosPair { SDL_Keycode sdl; uint16_t dos; };
constexpr SdlDosPair kSdlToDos[] = {
    {SDLK_a, 0x1E}, {SDLK_b, 0x30}, {SDLK_c, 0x2E}, {SDLK_d, 0x20}, {SDLK_e, 0x12},
    {SDLK_f, 0x21}, {SDLK_g, 0x22}, {SDLK_h, 0x23}, {SDLK_i, 0x17}, {SDLK_j, 0x24},
    {SDLK_k, 0x25}, {SDLK_l, 0x26}, {SDLK_m, 0x32}, {SDLK_n, 0x31}, {SDLK_o, 0x18},
    {SDLK_p, 0x19}, {SDLK_q, 0x10}, {SDLK_r, 0x13}, {SDLK_s, 0x1F}, {SDLK_t, 0x14},
    {SDLK_u, 0x16}, {SDLK_v, 0x2F}, {SDLK_w, 0x11}, {SDLK_x, 0x2D}, {SDLK_y, 0x15},
    {SDLK_z, 0x2C}, {SDLK_1, 0x02}, {SDLK_2, 0x03}, {SDLK_3, 0x04}, {SDLK_4, 0x05},
    {SDLK_5, 0x06}, {SDLK_6, 0x07}, {SDLK_7, 0x08}, {SDLK_8, 0x09}, {SDLK_9, 0x0A},
    {SDLK_0, 0x0B}, {SDLK_RETURN, 0x1C}, {SDLK_KP_ENTER, 0x1C}, {SDLK_ESCAPE, 0x01},
    {SDLK_BACKSPACE, 0x0E}, {SDLK_TAB, 0x0F}, {SDLK_SPACE, 0x39}, {SDLK_MINUS, 0x0C},
    {SDLK_EQUALS, 0x0D}, {SDLK_LEFTBRACKET, 0x1A}, {SDLK_RIGHTBRACKET, 0x1B},
    {SDLK_BACKSLASH, 0x2B}, {SDLK_SEMICOLON, 0x27}, {SDLK_QUOTE, 0x28},
    {SDLK_BACKQUOTE, 0x29}, {SDLK_COMMA, 0x33}, {SDLK_PERIOD, 0x34}, {SDLK_SLASH, 0x35},
    {SDLK_F1, 0x3B}, {SDLK_F2, 0x3C}, {SDLK_F3, 0x3D}, {SDLK_F4, 0x3E}, {SDLK_F5, 0x3F},
    {SDLK_F6, 0x40}, {SDLK_F7, 0x41}, {SDLK_F8, 0x42}, {SDLK_F9, 0x43}, {SDLK_F10, 0x44},
    {SDLK_F11, 0x45}, {SDLK_F12, 0x46}, {SDLK_RIGHT, 0x4D}, {SDLK_LEFT, 0x4B},
    {SDLK_DOWN, 0x50}, {SDLK_UP, 0x48}, {SDLK_HOME, 0x47}, {SDLK_END, 0x4F},
    {SDLK_PAGEUP, 0x49}, {SDLK_PAGEDOWN, 0x51}, {SDLK_INSERT, 0x52}, {SDLK_DELETE, 0x53},
    {SDLK_KP_1, 0x4F}, {SDLK_KP_2, 0x50}, {SDLK_KP_3, 0x51}, {SDLK_KP_4, 0x4B},
    {SDLK_KP_5, 0x4C}, {SDLK_KP_6, 0x4D}, {SDLK_KP_7, 0x47}, {SDLK_KP_8, 0x48},
    {SDLK_KP_9, 0x49}, {SDLK_KP_0, 0x52}, {SDLK_KP_PERIOD, 0x53},
    {SDLK_LCTRL, 0x1D}, {SDLK_RCTRL, 0x1D}, {SDLK_LSHIFT, 0x2A}, {SDLK_RSHIFT, 0x36},
    {SDLK_LALT, 0x38}, {SDLK_RALT, 0x38},
};
} // namespace

const char* dos_key_name(uint16_t dos_scancode) {
    for (const auto& k : kDosKeyNames)
        if (k.code == dos_scancode) return k.name;
    return "";
}

uint16_t sdl_to_dos(SDL_Keycode key) {
    for (const auto& p : kSdlToDos)
        if (p.sdl == key) return p.dos;
    return 0;
}

SDL_Keycode dos_to_sdl(uint16_t dos_scancode) {
    for (const auto& p : kSdlToDos)
        if (p.dos == dos_scancode) return p.sdl;
    return SDLK_UNKNOWN;
}

Frontend::Frontend(SDL_Renderer* renderer, Assets& assets, const std::string& assets_dir)
    : renderer_(renderer), font_(renderer, assets_dir),
      intro_(assets.load_video("LLOGO")), portraits_(assets.load_atlas("VSFACE")),
      title_(assets.load_ggf("MAINSCR")),
      back_(assets.load_ggf("BACK")), hs_(assets.load_ggf("HS")),
      credits_(assets.load_ggf("CREDITS")), versus_(assets.load_ggf("VS")),
      cfg_path_(assets_dir + "/ui/rise2.cfg") {
    if (intro_->frames.empty() || (portraits_->frames.size() != 28 && portraits_->frames.size() != 30))
        throw std::runtime_error("Incomplete intro ANI or VSFACE portraits");
    load_keymap();
    load_hiscores();
}

bool Frontend::take_match_request() {
    const bool requested = match_requested_;
    match_requested_ = false;
    return requested;
}

void Frontend::return_to_select() {
    screen_ = Screen::Select;
}

void Frontend::load_keymap() {
    FILE* fp = fopen(cfg_path_.c_str(), "rb");
    if (!fp) {
        fprintf(stderr, "rise2.cfg introuvable (%s) : defauts clavier\n", cfg_path_.c_str());
        return;
    }
    uint16_t words[46] = {};
    const size_t n = fread(words, 2, 46, fp);
    fclose(fp);
    if (n < 21) {
        fprintf(stderr, "rise2.cfg trop court (%zu mots)\n", n);
        return;
    }
    for (int p = 0; p < 2; ++p)
        for (int e = 0; e < 10; ++e)
            keymap_[p][e] = words[1 + p * 10 + e];
}

void Frontend::save_keymap() const {
    FILE* fp = fopen(cfg_path_.c_str(), "rb");
    uint16_t words[46] = {};
    if (fp) {
        if (fread(words, 2, 46, fp) < 21) {
            for (uint16_t& w : words) w = 0;
        }
        fclose(fp);
    }
    for (int p = 0; p < 2; ++p)
        for (int e = 0; e < 10; ++e)
            words[1 + p * 10 + e] = keymap_[p][e];
    fp = fopen(cfg_path_.c_str(), "wb");
    if (!fp) {
        fprintf(stderr, "Impossible d'ecrire %s\n", cfg_path_.c_str());
        return;
    }
    fwrite(words, 2, 46, fp);
    fclose(fp);
    fprintf(stderr, "KEYMAP saved (mot2=%u)\n", words[2]);
}

void Frontend::load_hiscores() {
    // HISCORE.DAT : 12 x (3 lettres + u32 score) ; cherche a cote de la cfg.
    const std::string path = cfg_path_.substr(0, cfg_path_.find_last_of("/\\")) + "/hiscore.dat";
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return;
    for (int i = 0; i < 12; ++i) {
        char name[4] = {};
        uint8_t score_bytes[4] = {};
        if (fread(name, 1, 3, fp) != 3 || fread(score_bytes, 1, 4, fp) != 4) break;
        Hiscore h;
        h.name = name;
        h.score = score_bytes[0] | (score_bytes[1] << 8) | (score_bytes[2] << 16) |
                  (static_cast<uint32_t>(score_bytes[3]) << 24);
        hiscores_.push_back(std::move(h));
    }
    fclose(fp);
}

void Frontend::key(SDL_Keycode key) {
    if (screen_ == Screen::Intro) {
        screen_ = Screen::Title;
        return;
    }
    if (screen_ == Screen::Title) {
        if (key == SDLK_UP) title_choice_ = (title_choice_ + 4) % 5;
        else if (key == SDLK_DOWN) title_choice_ = (title_choice_ + 1) % 5;
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
            switch (title_choice_) {
                case 0: screen_ = Screen::Select; break;
                case 1: kmap_player_ = 0; kmap_entry_ = 0; kmap_editing_ = false;
                        screen_ = Screen::KeyMapping; break;
                case 2: screen_ = Screen::HighScore; break;
                case 3: screen_ = Screen::Credits; break;
                default: quit_requested_ = true; break;
            }
        } else if (key == SDLK_ESCAPE) quit_requested_ = true;
        else if (key == SDLK_i) {
            intro_frame_ = 0;
            intro_elapsed_ = 0.0;
            screen_ = Screen::Intro;
        }
        return;
    }
    if (screen_ == Screen::KeyMapping) {
        fprintf(stderr, "KEYMAP key=%d editing=%d player=%d entry=%d\n", (int)key, (int)kmap_editing_, kmap_player_, kmap_entry_);
        if (kmap_editing_) {
            // n'importe quelle touche (sauf Esc) = affectation
            if (key == SDLK_ESCAPE) {
                kmap_editing_ = false;
            } else {
                const uint16_t dos = sdl_to_dos(key);
                if (dos != 0) {
                    keymap_[kmap_player_][kmap_entry_] = dos;
                    save_keymap();
                }
                kmap_editing_ = false;
            }
            return;
        }
        if (key == SDLK_UP) kmap_entry_ = (kmap_entry_ + 9) % 10;
        else if (key == SDLK_DOWN) kmap_entry_ = (kmap_entry_ + 1) % 10;
        else if (key == SDLK_TAB || key == SDLK_RIGHT || key == SDLK_LEFT)
            kmap_player_ = 1 - kmap_player_;
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE)
            kmap_editing_ = true;
        else if (key == SDLK_ESCAPE) screen_ = Screen::Title;
        return;
    }
    if (screen_ == Screen::HighScore || screen_ == Screen::Credits) {
        screen_ = Screen::Title;
        return;
    }
    if (screen_ == Screen::Select) {
        const int count = static_cast<int>(kRoster.size());
        const int step = (key == SDLK_LEFT || key == SDLK_a || key == SDLK_q) ? -1
                         : (key == SDLK_RIGHT || key == SDLK_d) ? 1 : 0;
        if (step != 0) {
            // debordement de la grille : 0..19 = visibles, 20..29 = masques (reveles en sortant)
            const int player = (key == SDLK_a || key == SDLK_q || key == SDLK_d) ? 1 : 0;
            int& sel = selected_[player];
            sel = (sel + step + count) % count;
        } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
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
    for (int i = 0; i < 5; ++i) {
        const SDL_Color color = (i == title_choice_) ? gold : blue;
        font_.draw(kTitleItems[i], 320, 250 + i * 29, color, 16, 21, true);
    }
}

void Frontend::draw_keymapping() const {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    font_.draw("KEY MAPPING", 320, 14, gold, 16, 21, true);
    for (int p = 0; p < 2; ++p) {
        const int cx = p == 0 ? 170 : 470;
        const SDL_Color header = kmap_player_ == p ? gold : blue;
        std::string title = std::string("PLAYER ") + (p == 0 ? "1" : "2") + "  KEYBOARD";
        font_.draw(title, cx, 58, header, 14, 19, true);
        for (int e = 0; e < 10; ++e) {
            const int y = 96 + e * 26;
            const bool selected = kmap_player_ == p && kmap_entry_ == e;
            const SDL_Color label_color = selected ? gold : blue;
            font_.draw(kEntryNames[e], cx - 130, y, label_color, 12, 17, false);
            const uint16_t dos = keymap_[p][e];
            std::string value = kmap_editing_ && selected
                ? std::string("<PRESS A KEY>")
                : (dos_key_name(dos)[0] ? dos_key_name(dos) : std::to_string(dos));
            font_.draw(value, cx + 10, y, selected ? red : blue, 12, 17, false);
        }
    }
    font_.draw("ARROWS SELECT   ENTER ASSIGN   TAB PLAYER   ESC BACK",
               320, 372, blue, 9, 14, true);
}

void Frontend::draw_highscore() const {
    draw_background(hs_);
    font_.draw("HIGH SCORE", 320, 20, gold, 16, 21, true);
    if (hiscores_.empty()) {
        font_.draw("NO DATA", 320, 200, grey, 14, 19, true);
        return;
    }
    for (size_t i = 0; i < hiscores_.size(); ++i) {
        const int y = 70 + static_cast<int>(i) * 24;
        char score[16];
        snprintf(score, sizeof(score), "%08u", hiscores_[i].score);
        font_.draw(hiscores_[i].name, 240, y, blue, 14, 19, false);
        font_.draw(score, 330, y, i < 3 ? gold : blue, 14, 19, false);
    }
    font_.draw("ESC BACK", 320, 372, grey, 9, 14, true);
}

void Frontend::draw_credits() const {
    draw_background(credits_);
    font_.draw("CREDITS", 320, 16, gold, 16, 21, true);
    static const char* lines[] = {
        "RISE 2: RESURRECTION", "", "ORIGINAL GAME BY MIRAGE TECHNOLOGIES",
        "PUBLISHED BY ACCLAIM 1996", "", "PORT BY THE RISE 2 PORT CONTRIBUTORS",
        "ASSETS DECODED FROM THE ORIGINAL FILES", "", "THANK YOU FOR PLAYING",
    };
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i)
        font_.draw(lines[i], 320, 70 + static_cast<int>(i) * 28, blue, 11, 16, true);
    font_.draw("ESC BACK", 320, 372, grey, 9, 14, true);
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
    if (selected_[player_index] >= visible_robots_) {
        font_.draw("HIDDEN", center_x, 317, red, 11, 15, true);
    } else {
        std::string slot = std::string("ROBOT ") + kRoster[selected_[player_index]].slot;
        font_.draw(slot, center_x, 317, blue, 11, 15, true);
    }
}

void Frontend::draw_grid() const {
    // grille 2 x 10 (les 20 robots visibles) sur le mur de panneaux du hangar ; le robot
    // masque sous le curseur remplace la vignette de son emplacement.
    const int cell_w = 60, cell_h = 51;
    const int x0 = 16, y0 = 4;
    for (int p = 0; p < 2; ++p) {
        const int slot = std::min(selected_[p], visible_robots_ - 1);
        const int col = slot % 10, row = slot / 10;
        SDL_Color c = p == 0 ? SDL_Color{255, 40, 40, 255} : SDL_Color{40, 90, 255, 255};
        SDL_Rect frame{x0 + col * (cell_w + 1) - 1, y0 + row * (cell_h + 1) - 1,
                       cell_w + 3, cell_h + 3};
        SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, 255);
        SDL_RenderDrawRect(renderer_, &frame);
    }
    for (int i = 0; i < visible_robots_; ++i) {
        const int col = i % 10, row = i / 10;
        const int cx = x0 + col * (cell_w + 1), cy = y0 + row * (cell_h + 1);
        // vignette : portrait du robot i, ou du masque selectionne a cette place
        int robot = i;
        for (int p = 0; p < 2; ++p)
            if (selected_[p] >= visible_robots_ &&
                std::min(selected_[p], visible_robots_ - 1) == i)
                robot = selected_[p];
        const AtlasFrame& f = portraits_->frames[portrait_index(robot)];
        if (f.empty) continue;
        const double scale = std::min((cell_w - 4.0) / f.rect_w, (cell_h - 4.0) / f.rect_h);
        const int w = static_cast<int>(f.rect_w * scale);
        const int h = static_cast<int>(f.rect_h * scale);
        SDL_Rect src{f.rect_x, f.rect_y, f.rect_w, f.rect_h};
        SDL_Rect dst{cx + (cell_w - w) / 2, cy + (cell_h - h) / 2, w, h};
        SDL_RenderCopy(renderer_, portraits_->pages[f.page], &src, &dst);
    }
}

void Frontend::draw_select() const {
    // fond : noir pour l'instant (l'image du hangar sera reinjectee plus tard).
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    draw_grid();
    draw_big_robot(0);
    draw_big_robot(1);
    font_.draw("P1 ARROWS    P2 A/D    ENTER FIGHT    ESC MENU    OFF-GRID: HIDDEN ROBOTS",
               320, 386, blue, 9, 14, true);
}

// le robot selectionne, en grand sur sa plate-forme (ref. captures 03/04)
void Frontend::draw_big_robot(int player_index) const {
    const AtlasFrame& frame = portraits_->frames[portrait_index(selected_[player_index])];
    const int center_x = player_index == 0 ? 160 : 480;
    const int floor_y = 366;
    if (!frame.empty) {
        const double scale = std::min(240.0 / frame.rect_w, 200.0 / frame.rect_h);
        const int width = static_cast<int>(std::round(frame.rect_w * scale));
        const int height = static_cast<int>(std::round(frame.rect_h * scale));
        SDL_Rect source{frame.rect_x, frame.rect_y, frame.rect_w, frame.rect_h};
        SDL_Rect target{center_x - width / 2, floor_y - height, width, height};
        const SDL_RendererFlip flip = player_index == 0 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
        SDL_RenderCopyEx(renderer_, portraits_->pages[frame.page], &source, &target, 0, nullptr, flip);
    }
    const SDL_Color color = player_index == 0 ? cyan : red;
    font_.draw(kRoster[selected_[player_index]].name, center_x, 372, color, 13, 18, true);
}

void Frontend::draw_pause_overlay(int choice) const {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 140);
    SDL_Rect full{0, 0, 640, 400};
    SDL_RenderFillRect(renderer_, &full);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    static const char* items[] = {"CONTINUE MATCH", "F9  CALIBRATE JOYSTICKS", "F10 QUIT MATCH"};
    for (int i = 0; i < 3; ++i) {
        const SDL_Color color = i == choice ? SDL_Color{255, 60, 40, 255}
                                            : SDL_Color{176, 202, 245, 255};
        font_.draw(items[i], 320, 168 + i * 34, color, 16, 21, true);
    }
}

void Frontend::render() const {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    if (screen_ == Screen::Intro) {
        SDL_Rect target{0, 0, 640, 400};
        SDL_RenderCopy(renderer_, intro_->frames[intro_frame_], nullptr, &target);
    } else if (screen_ == Screen::Title) draw_title();
    else if (screen_ == Screen::KeyMapping) draw_keymapping();
    else if (screen_ == Screen::HighScore) draw_highscore();
    else if (screen_ == Screen::Credits) draw_credits();
    else if (screen_ == Screen::Select) draw_select();
}