#include "entitytraits.h"

#include "entities.h"
#include "characters/conditions.h"

uint8_t getEffectiveHitDice(const Entity& entity)
{
    if (entity.type == ENTITY_MONSTER && entity.monster != nullptr &&
        entity.monster->hitDice > 0)
    {
        return entity.monster->hitDice;
    }

    return entity.character.level > 0 ? entity.character.level : 1;
}

bool canSee(const Entity& entity)
{
    if (entity.type == ENTITY_MONSTER && entity.monster != nullptr &&
        entity.monster->sightless)
    {
        return false;
    }

    return !hasCondition(entity.character, CONDITION_BLINDED);
}

bool isImmuneToWeb(const Entity& entity)
{
    return entity.type == ENTITY_MONSTER &&
           entity.monsterID == MONSTER_GIANT_SPIDER;
}

bool isUndeadCreature(const Entity& entity)
{
    return isUndead(entity.character);
}

bool isUndead(const Character& character)
{
    return isUndeadCreatureType(character.creatureType);
}

bool isUndeadCreatureType(CreatureType creatureType)
{
    switch (creatureType)
    {
        case CREATURE_SKELETON:
        case CREATURE_ZOMBIE:
        case CREATURE_UNDEAD:
            return true;

        default:
            return false;
    }
}
