#ifndef PATHFINDERMINIEXTREME_025_AWARENESS_H
#define PATHFINDERMINIEXTREME_025_AWARENESS_H

#include "data/entities.h"

void updateAwareness();
void updateMonsterVisibility();
void resetAwarenessTimer();
int getStealthSituationModifier(const Entity& player, const Entity& observer);
bool tryMonsterDetectPlayer(Entity& monster, const Entity& player);
bool tryPlayerDetectMonster(Entity& player, Entity& monster);

inline bool monsterPerceptionBeatsStealth(
    int monsterPerception,
    int playerStealth)
{
    // Stealth wins ties.
    return monsterPerception > playerStealth;
}

inline bool applyMonsterDetectionResult(
    Entity& monster,
    bool detected)
{
    if (monster.awareOfPlayer)
        return true;

    if (!detected)
        return false;

    monster.awareOfPlayer = true;
    monster.revealedToPlayer = true;
    return true;
}

inline bool shouldMonsterBeVisible(
    bool combatParticipant,
    bool revealedToPlayer,
    bool hasCurrentLineOfSight)
{
    return hasCurrentLineOfSight &&
           (combatParticipant || revealedToPlayer);
}

inline bool combatParticipationGrantsVisibility(
    bool rosterMember,
    bool openingAttackInProgress,
    bool awareOfPlayer)
{
    return rosterMember &&
           (!openingAttackInProgress || awareOfPlayer);
}

#endif
