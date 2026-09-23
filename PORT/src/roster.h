#pragma once

#include <array>

struct RobotInfo {
    char slot;             // suffixe de RBT?/R? et champ robot_slot de VSFACE
    const char* name;      // ordre de RISE.FRA, premiere section
};

constexpr std::array<RobotInfo, 30> kRoster{{
    {'0', "CYBORG"}, {'1', "LOADER"},
    {'A', "PRIME 8"}, {'B', "CRUSHER"}, {'C', "WAR"}, {'D', "ROOK"},
    {'E', "V1-HYPER"}, {'F', "DEADLIFT"}, {'G', "SUIKWAN"}, {'H', "DETAIN"},
    {'I', "CHROMAX"}, {'J', "STEPPENWOLF"}, {'K', "NECROBORG"}, {'L', "LOCKJAW"},
    {'M', "GRILLER"}, {'N', "VANDAL"}, {'O', "SALVO"}, {'P', "INSANE"},
    {'Q', "SUPERVISOR"}, {'R', "MAYHEM"}, {'S', "ASSAULT"}, {'T', "ANIL 8"},
    {'U', "NADEN"}, {'V', "RACK"}, {'W', "SANE"}, {'X', "VITRIOL"},
    {'Y', "SURPRESSOR"}, {'Z', "ARD ONE"},
    {'2', "SHEEPMAN"}, {'3', "BUNNYRABBIT"},
}};

inline int portrait_index(int robot_index) {
    return robot_index < 2 ? robot_index + 26 : robot_index < 28 ? robot_index - 2 : robot_index;
}
