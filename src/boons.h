#pragma once
#include <cstdint>
// Boons that can be tracked. maxShown = value the gauge treats as "full" (intensity boons show stacks;
// duration-stacking boons show presence: 0 or 1). intensity = stacks tick down independently (timer mode applies).
constexpr int BOON_COUNT = 12;
struct BoonDef { uint32_t id; const char* name; int maxShown; bool intensity; };
inline constexpr BoonDef kBoons[BOON_COUNT] = {
    {1122, "Stability", 25, true},  {740, "Might", 25, true},       {725, "Fury", 1, false},
    {1187, "Quickness", 1, false},  {30328, "Alacrity", 1, false}, {717, "Protection", 1, false},
    {718, "Regeneration", 1, false},{719, "Swiftness", 1, false},   {726, "Vigor", 1, false},
    {743, "Aegis", 1, false},       {26980, "Resistance", 1, false},{873, "Resolution", 1, false}};
inline int boonIndex(uint32_t skillId) {
    for (int i = 0; i < BOON_COUNT; i++) if (kBoons[i].id == skillId) return i;
    return -1;
}
