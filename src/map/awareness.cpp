#include "map/awareness.h"

#include <Arduino.h>

#include "characters/characters.h"
#include "data/dice.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/entitytraits.h"
#include "dungeon/combat.h"
#include "dungeon/traps.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "map/activemap.h"

namespace
{
unsigned long lastAwarenessCheck = 0;
constexpr unsigned long AWARENESS_INTERVAL_MS = 6000;

bool isHostileLivingMonster(const Entity& entity)
{
    return entity.active && entity.type == ENTITY_MONSTER &&
           entity.character.team == TEAM_MONSTER &&
           entity.character.state == STATE_ALIVE;
}
}

void resetAwarenessTimer()
{
    lastAwarenessCheck = millis();
}

int getStealthSituationModifier(const Entity& player, const Entity& observer)
{
    // Future shadow/light modifiers belong here.
    return hasLineOfSightBetweenFootprintsAt(
        observer, observer.x, observer.y, player) ? 0 : 5;
}

bool tryMonsterDetectPlayer(Entity& monster, const Entity& player)
{
    if (monster.awareOfPlayer)
        return true;

    if (!isHostileLivingMonster(monster) || monster.monster == nullptr ||
        getEntityGridDistance(player, monster) > COMBAT_DETECTION_RANGE)
    {
        return false;
    }

    const int playerStealth = rollDie(20) +
        getSkillBonus(player.character, SKILL_STEALTH) +
        getStealthSituationModifier(player, monster);
    const int monsterPerception =
        rollDie(20) + monster.monster->perceptionBonus;

    Serial.printf("Player Stealth %d; %s Perception %d\n",
                  playerStealth, getEntityName(&monster), monsterPerception);

    if (!applyMonsterDetectionResult(
            monster,
            monsterPerceptionBeatsStealth(
                monsterPerception, playerStealth)))
    {
        return false;
    }

    markEntityFootprintDirty(monster);
    return true;
}

bool tryPlayerDetectMonster(Entity& player, Entity& monster)
{
    if (!isHostileLivingMonster(monster) || monster.monster == nullptr ||
        monster.revealedToPlayer ||
        !hasLineOfSightBetweenFootprintsAt(
            player, player.x, player.y, monster))
    {
        return false;
    }

    const int distance = getEntityGridDistance(player, monster);
    const int perception = rollDie(20) +
        getSkillBonus(player.character, SKILL_PERCEPTION) -
        (distance > 10 ? distance - 10 : 0);
    const int stealth = rollDie(20) + monster.monster->stealthBonus;

    Serial.printf("Player Perception %d; %s Stealth %d\n",
                  perception, getEntityName(&monster), stealth);

    if (perception <= stealth)
        return false;

    monster.revealedToPlayer = true;
    markEntityFootprintDirty(monster);
    return true;
}

void updateMonsterVisibility()
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    Entity* player = getActiveMapPlayer();

    if (entities == nullptr || player == nullptr)
        return;

    const bool playerCanSee = canSee(*player);

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& monster = entities[i];

        if (!monster.active || monster.type != ENTITY_MONSTER ||
            monster.character.state != STATE_ALIVE)
        {
            continue;
        }

        const bool hasCurrentLineOfSight = playerCanSee &&
            hasLineOfSightBetweenFootprintsAt(
                *player, player->x, player->y, monster);
        const bool combatParticipant =
            combatParticipationGrantsVisibility(
                isCombatParticipant(monster),
                combat.openingAttackInProgress,
                monster.awareOfPlayer);
        const bool visible = shouldMonsterBeVisible(
            combatParticipant,
            monster.revealedToPlayer,
            hasCurrentLineOfSight);

        if (hasCurrentLineOfSight)
        {
            monster.lastKnownX = monster.x;
            monster.lastKnownY = monster.y;
            monster.hasLastKnownPosition = true;
        }

        if (combatParticipant && hasCurrentLineOfSight &&
            !monster.revealedToPlayer)
        {
            // Seeing a room combatant corrects stale pre-combat discovery
            // state, but LOS loss never changes awareness or discovery back.
            monster.revealedToPlayer = true;
        }

        const bool liveVisible = playerCanSee && visible;
        const bool renderLos = playerCanSee && hasCurrentLineOfSight;
        const bool renderChanged =
            monster.playerHasLineOfSight != renderLos;
        const bool visibilityChanged =
            monster.visibleToPlayer != liveVisible;
        if (!renderChanged && !visibilityChanged)
            continue;

        monster.visibleToPlayer = liveVisible;
        monster.playerHasLineOfSight = renderLos;
        markEntityFootprintDirty(monster);
    }
}

void updateAwareness()
{
    // Rogue trapfinding is event-gated by persistent per-trap attempt state.
    // Calling this from the normal awareness update makes newly loaded or
    // newly observed room geometry eligible immediately without tying the
    // roll to the six-second monster-awareness cadence.
    Entity* activePlayer = getActiveMapPlayer();
    if (activePlayer != nullptr)
        updateRogueTrapAwareness(*activePlayer);

    if (combat.active || millis() - lastAwarenessCheck < AWARENESS_INTERVAL_MS)
        return;
    lastAwarenessCheck = millis();

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    Entity* player = getActiveMapPlayer();
    if (entities == nullptr || player == nullptr || player->character.state != STATE_ALIVE)
        return;

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& monster = entities[i];
        if (!isHostileLivingMonster(monster) || monster.monster == nullptr)
            continue;

        const int distance = getEntityGridDistance(*player, monster);
        tryPlayerDetectMonster(*player, monster);

        if (distance > COMBAT_DETECTION_RANGE || monster.awareOfPlayer)
            continue;

        if (tryMonsterDetectPlayer(monster, *player))
        {
            char message[64];
            snprintf(message, sizeof(message), "%s looks your way!", getEntityName(&monster));
            setGameMessage(message);
            startCombat();
            return;
        }
    }
}
