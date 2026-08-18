#include "map/awareness.h"

#include <Arduino.h>

#include "characters/characters.h"
#include "data/dice.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "dungeon/combat.h"
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

void updateAwareness()
{
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
        if (!monster.revealedToPlayer &&
            hasLineOfSightBetweenFootprintsAt(*player, player->x, player->y, monster))
        {
            const int perception = rollDie(20) +
                getSkillBonus(player->character, SKILL_PERCEPTION) -
                (distance > 10 ? distance - 10 : 0);
            const int stealth = rollDie(20) + monster.monster->stealthBonus;
            if (perception > stealth)
            {
                monster.revealedToPlayer = true;
                markEntityFootprintDirty(monster);
            }
        }

        if (distance > COMBAT_DETECTION_RANGE || monster.awareOfPlayer)
            continue;

        const int playerStealth = rollDie(20) +
            getSkillBonus(player->character, SKILL_STEALTH) +
            getStealthSituationModifier(*player, monster);
        const int monsterPerception = rollDie(20) + monster.monster->perceptionBonus;
        Serial.printf("Player Stealth %d; %s Perception %d\n",
                      playerStealth, getEntityName(&monster), monsterPerception);
        if (monsterPerception > playerStealth)
        {
            monster.awareOfPlayer = true;
            monster.revealedToPlayer = true;
            markEntityFootprintDirty(monster);
            char message[64];
            snprintf(message, sizeof(message), "%s looks your way!", getEntityName(&monster));
            setGameMessage(message);
            startCombat();
            return;
        }
    }
}
