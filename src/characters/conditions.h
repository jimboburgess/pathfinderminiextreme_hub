//
// Created by james on 7/25/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_CONDITIONS_H
#define PATHFINDERMINIEXTREME_025_CONDITIONS_H

#include <stdint.h>

enum ConditionType
{
    CONDITION_NONE,

    // Combat
    CONDITION_POISONED,
    CONDITION_BURNING,
    CONDITION_SLEEPING,
    CONDITION_STUNNED,
    CONDITION_PARALYZED,
    CONDITION_BLINDED,
    CONDITION_CONFUSED,

    // Buffs
    CONDITION_BLESSED,
    CONDITION_HASTED,
    CONDITION_BLUR,
    CONDITION_MAGE_ARMOR,
    CONDITION_BARKSKIN,

    // Long-term
    CONDITION_CURSED,
    CONDITION_DISEASED,

    CONDITION_MAX
};

struct Condition
{
    ConditionType type;

    int value;

    int roundsRemaining;
};

constexpr uint8_t MAX_CONDITIONS = 16;

extern Condition conditions[MAX_CONDITIONS];
extern int conditionCount;


#endif //PATHFINDERMINIEXTREME_025_CONDITIONS_H
