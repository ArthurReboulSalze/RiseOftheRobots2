#pragma once

#include "assets.h"
#include "roster.h"
#include "ui.h"
#include "settings.h"
#include <vector>

enum class Screen { Intro, Title, Options, KeyMapping, HighScore, Credits, Select, Fight, Player, Playback };

// Écran KEY MAPPING : lecture/écriture de EXTRACTED/ui/rise2.cfg (46 x u16, mots 1-10 = P1,
// 11-20 = P2 : up/down/left/right/01/02/03/P1/P2/P3, valeurs = scancodes DOS set 1).
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
    void draw_pause_overlay(int choice) const;   // menu pause du combat (Echap)

    // Entrées configurées (scancodes DOS par entrée ; voir dos_key_name / sdl_to_dos).
    uint16_t key_for(int player_index, int entry) const { return keymap_[player_index][entry]; }
    const Settings& settings() const { return settings_; }
    int animation_frame() const { return screen_==Screen::Playback ? movie_frame_ : intro_frame_; }
    int movie_count() const { return (int)movies_.size(); }
    bool movie_paused() const { return movie_paused_; }
    const std::string& playing_movie() const { return movie_name_; }

private:
    SDL_Renderer* renderer_;
    Assets& assets_;
    UiFont font_;
    const VideoBank* intro_;
    const AtlasBank* portraits_;
    SDL_Texture* title_;
    SDL_Texture* back_;      // fond de l'ecran de selection (le hangar, BACK.GGF)
    SDL_Texture* hs_;        // fond de l'ecran HIGH SCORE
    SDL_Texture* credits_;   // fond de l'ecran CREDITS
    SDL_Texture* versus_;
    SDL_Texture* options_background_;
    Settings settings_;
    std::string options_path_;
    int options_choice_ = 0;
    Screen screen_ = Screen::Intro;
    int intro_frame_ = 0;
    double intro_elapsed_ = 0.0;
    int title_choice_ = 0;
    std::vector<MovieInfo> movies_;
    std::vector<int> movie_rows_;
    int movie_choice_=0, movie_category_=0, movie_resolution_=0;
    const VideoBank* movie_=nullptr;
    std::string movie_name_, movie_next_, movie_error_;
    int movie_frame_=0;
    double movie_elapsed_=0;
    bool movie_paused_=false;

    // KEY MAPPING
    uint16_t keymap_[2][10] = {};
    int kmap_player_ = 0;             // colonne éditée (0/1)
    int kmap_entry_ = 0;              // ligne éditée (0..9)
    bool kmap_editing_ = false;
    std::string cfg_path_;

    // HIGH SCORE (HISCORE.DAT : 12 x (nom 3 lettres + score u32))
    struct Hiscore { std::string name; uint32_t score; };
    std::vector<Hiscore> hiscores_;

    int selected_[2] = {2, 7};        // PRIME 8 / DEADLIFT (0..19 visible, 20+ hidden)
    int visible_robots_ = 20;
    bool match_requested_ = false;
    bool quit_requested_ = false;

    double intro_frame_duration() const;
    void load_keymap();
    void save_keymap() const;
    void load_hiscores();
    void draw_title() const;
    void filter_movies();
    void start_movie(const std::string& name, const std::string& next="");
    void stop_movie();
    double movie_frame_duration() const;
    void draw_player() const;
    void draw_movie() const;
    void draw_keymapping() const;
    void draw_options() const;
    void draw_highscore() const;
    void draw_credits() const;
    void draw_select() const;
    void draw_portrait(int player_index) const;
    void draw_big_robot(int player_index) const;
    void draw_grid() const;
    void draw_background(SDL_Texture* texture) const;
};

// Noms des scancodes DOS set 1 (pour l'affichage) ; chaîne vide si inconnu.
const char* dos_key_name(uint16_t dos_scancode);
// Conversion SDL -> scancode DOS (0 si non mappé).
uint16_t sdl_to_dos(SDL_Keycode key);
// Conversion inverse scancode DOS -> SDL_Keycode (SDLK_UNKNOWN si non mappé).
SDL_Keycode dos_to_sdl(uint16_t dos_scancode);
