#include "fighter.h"

const MvsMove* Fighter::move() const {
    if (!mvs || move_id < 0 || move_id >= (int)mvs->moves.size()) return nullptr;
    return &mvs->moves[move_id];
}

int Fighter::current_frame() const {
    const MvsMove* mv = move();
    if (!mv || speed_level < 0 || speed_level >= (int)mv->sequences.size()) return -1;
    const auto& seq = mv->sequences[speed_level];
    if (seq_pos < 0 || seq_pos >= (int)seq.size() || seq[seq_pos].end) return -1;
    return seq[seq_pos].image;
}

const AtlasFrame* Fighter::current_atlas_frame() const {
    int img = current_frame();
    // Les atlas ANL/ANR ont une entree vide initiale ; l'image MVS 0 est l'atlas 1.
    if (!atlas || img < 0 || img + 1 >= (int)atlas->frames.size()) return nullptr;
    return &atlas->frames[img + 1];
}

SDL_Rect Fighter::frame_rect(const AtlasFrame& frame, int camera_x) const {
    if (!atlas || atlas->frames.size() < 2) return {0, 0, 0, 0};
    // Les images sont recadrees depuis un meme canevas 640x400. Leur largeur
    // et leur hauteur variables ne doivent pas deplacer l'ancre du personnage.
    const AtlasFrame& reference = atlas->frames[1];
    const int anchor_x = reference.origin_x + reference.rect_w / 2;
    const int anchor_y = reference.origin_y + reference.rect_h;
    const int screen_x = x - camera_x;
    const int left = facing >= 0
        ? screen_x + frame.origin_x - anchor_x
        : screen_x + anchor_x - frame.origin_x - frame.rect_w;
    return {left, y + frame.origin_y - anchor_y, frame.rect_w, frame.rect_h};
}

int Fighter::step(uint16_t inputs) {
    const MvsMove* mv = move();
    if (!mv) return -1;
    int state_change = -1;
    // Les etats de marche bouclent sans transition de sortie. Leur cible est
    // indiquee par les transitions avant/arriere de l'etat neutre.
    if (move_id != 0 && !mvs->moves.empty()) {
        for (const auto& t : mvs->moves[0].transitions) {
            if ((t.mask == IN_B0 || t.mask == IN_B5) && move_id == t.target &&
                !(inputs & t.mask)) {
                move_id = 0;
                seq_pos = 0;
                displacement = 0;
                playing = true;
                state_change = 0;
                mv = move();
                break;
            }
        }
    }
    // 1) transitions — règle exacte de fn_234fa : si le masque contient des bits de
    //    marche (0x21), la comparaison est restreinte à ces bits.
    for (auto& t : mv->transitions) {
        uint16_t cur = inputs;
        if (t.mask & 0x21) cur = cur & 0x21;
        if (cur == t.mask) {
            seq_pos = 0;
            displacement = 0;
            move_id = t.target;
            playing = true;
            return t.target;
        }
    }
    if (state_change >= 0) return state_change;
    // 2) avance la séquence
    if (speed_level < 0 || speed_level >= (int)mv->sequences.size()) return -1;
    const auto& seq = mv->sequences[speed_level];
    if (seq.empty()) return -1;
    const int frame_count = (int)seq.size() - (seq.back().end ? 1 : 0);
    if (frame_count == 0) return -1;
    if (seq_pos < 0 || seq_pos >= frame_count) seq_pos = 0;
    if (!playing) return -1;
    // applique le déplacement du pas courant (déjà fait par l'appelant via movement())
    seq_pos++;
    if (seq_pos >= frame_count) {
        // Le marqueur de fin n'est pas une image à afficher.
        if (seq.back().end && mv->flags != 0 && mv->resume_index < frame_count) {
            seq_pos = mv->resume_index;
        } else {
            seq_pos = frame_count - 1;
            playing = false;
        }
    }
    return -1;
}
void Fighter::get_boxes(std::vector<Cl2Box>* attacks, std::vector<Cl2Box>* bodies) const {
    if (!cl2 || current_frame() < 0 || current_frame() >= (int)cl2->frames.size()) return;
    const Cl2Frame& fr = cl2->frames[current_frame()];
    auto place = [&](const Cl2Box& b) {
        Cl2Box s = b;
        // x : CL2 x4, ancré au centre du combattant (miroir si facing gauche)
        // y : CL2 y2 = y absolu du canevas (déjà calé sur le sol)
        int bx = b.x * 4 - cl2_ref_x;
        if (facing < 0) {
            s.x = x - bx - b.w * 4;
            s.w = b.w * 4;
        } else {
            s.x = x + bx;
            s.w = b.w * 4;
        }
        s.y = b.y * 2;
        s.h = b.h * 2;
        return s;
    };
    for (auto& b : fr.attack_boxes) if (attacks) attacks->push_back(place(b));
    for (auto& b : fr.body_boxes) if (bodies) bodies->push_back(place(b));
}
