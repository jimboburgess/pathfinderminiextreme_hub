#ifndef PATHFINDERMINIEXTREME_025_MONSTER_PRESENTATION_H
#define PATHFINDERMINIEXTREME_025_MONSTER_PRESENTATION_H

#include <stdint.h>

constexpr unsigned long MONSTER_STEP_PRESENTATION_MS = 200;
constexpr unsigned long MONSTER_TURN_MESSAGE_MS = 350;
constexpr unsigned long MONSTER_THINKING_MIN_MS = 300;
constexpr unsigned long MONSTER_THINKING_MAX_MS = 600;

inline uint8_t getMonsterThinkingChance(uint8_t intelligence)
{
    if (intelligence <= 3) return 35;
    if (intelligence <= 7) return 25;
    if (intelligence <= 11) return 15;
    return 8;
}

inline bool shouldMonsterThink(uint8_t intelligence, uint8_t rollPercent)
{
    return rollPercent < getMonsterThinkingChance(intelligence);
}

inline unsigned long getMonsterThinkingDuration(uint16_t cosmeticValue)
{
    return MONSTER_THINKING_MIN_MS +
        cosmeticValue %
            (MONSTER_THINKING_MAX_MS - MONSTER_THINKING_MIN_MS + 1);
}

inline uint32_t mixMonsterPresentationValue(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    return value ^ (value >> 16);
}

inline uint32_t getMonsterPresentationValue(
    uint8_t monsterID,
    uint8_t x,
    uint8_t y,
    uint8_t combatRound,
    uint8_t turnIndex)
{
    const uint32_t packed =
        static_cast<uint32_t>(monsterID) |
        (static_cast<uint32_t>(x) << 8) |
        (static_cast<uint32_t>(y) << 16) |
        (static_cast<uint32_t>(combatRound) << 24);
    return mixMonsterPresentationValue(
        packed ^ (static_cast<uint32_t>(turnIndex) * 0x9e3779b9U));
}

#endif
