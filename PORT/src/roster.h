#pragma once

#include <array>

struct RobotInfo {
    char slot;             // RBT?/R? suffix and VSFACE robot_slot field
    const char* name;      // RISE.FRA order, first section
};

constexpr std::array<RobotInfo, 30> kRoster{{
    {'A', "CYBORG"}, {'B', "LOADER"}, {'C', "PRIME 8"}, {'D', "CRUSHER"},
    {'E', "WAR"}, {'F', "ROOK"}, {'G', "V1-HYPER"}, {'H', "DEADLIFT"},
    {'I', "SUIKWAN"}, {'J', "DETAIN"}, {'K', "CHROMAX"}, {'L', "STEPPENWOLF"},
    {'M', "NECROBORG"}, {'N', "LOCKJAW"}, {'O', "GRILLER"}, {'P', "VANDAL"},
    {'Q', "SALVO"}, {'R', "INSANE"}, {'S', "SUPERVISOR"}, {'T', "MAYHEM"},
    {'U', "ASSAULT"}, {'V', "ANIL 8"}, {'W', "NADEN"}, {'X', "RACK"},
    {'Y', "SANE"}, {'Z', "VITRIOL"}, {'0', "SURPRESSOR"}, {'1', "ARD ONE"},
    {'2', "SHEEPMAN"}, {'3', "BUNNYRABBIT"},
}};

inline int portrait_index(int robot_index) {
    return robot_index;
}
