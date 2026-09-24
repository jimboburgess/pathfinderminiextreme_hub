//
// Created by james on 8/1/2026.
//

#ifndef MONSTER_SCRIPTS_H
#define MONSTER_SCRIPTS_H

#include "characters/abilities.h"

struct Entity;

inline bool isMonsterControlAbilityRedundant(
    const Ability& ability,
    const Character& target)
{
    bool hasConditionEffect = false;
    bool allConditionsAlreadyPresent = true;

    for (uint8_t index = 0; index < ability.effectCount; index++)
    {
        const ConditionType condition =
            ability.effects[index].conditionType;
        if (condition == CONDITION_NONE)
            continue;

        hasConditionEffect = true;
        if (!hasCondition(target, condition))
            allConditionsAlreadyPresent = false;
    }

    return hasConditionEffect &&
           (!canCharacterAct(target) || allConditionsAlreadyPresent);
}

inline bool shouldPlaceControlMapEffectAtTarget(bool alreadyPresent)
{
    return !alreadyPresent;
}

inline bool shouldUseWebCoverage(
    uint8_t priority,
    bool targetAlreadyWebbed,
    uint8_t newTileCount,
    bool adjacentGrappledTarget)
{
    if (priority == 0 || newTileCount == 0)
        return false;
    if (priority == 1)
        return !targetAlreadyWebbed;
    return !adjacentGrappledTarget && newTileCount >= 4;
}

void runMonsterScript(Entity* monster);

// Individual scripts

void runMeleeScript(Entity* monster);
void runRangedScript(Entity* monster);
void runCowardScript(Entity* monster);
void runGuardScript(Entity* monster);
void runWanderScript(Entity* monster);
void runSupportScript(Entity* monster);
void runSpellcasterScript(Entity* monster);
void runControlSpellcasterScript(Entity* monster);
void runDebugScript(Entity* monster);

Entity* chooseTarget(Entity* monster);

void performStandardAction(Entity* monster);
bool keepDistance(Entity* monster);
void performRangedAttack(Entity* monster);
bool isMonsterReadyForAction(Entity* monster);
bool findUsefulMonsterWebTarget(
    const Entity& monster,
    const Entity& target,
    int& targetX,
    int& targetY);

//==================================================
// Movement
//==================================================

void performMovementPhase(Entity* entity);
void moveMonsterTowardsPlayer(Entity* monster);
bool canMonsterMoveTo(Entity* monster, int x, int y);
bool isAdjacent(const Entity* a, const Entity* b);


#endif
