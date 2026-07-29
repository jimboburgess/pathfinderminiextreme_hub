//
// Created by james on 7/25/2026.
//

#include "entityspawn.h"
#include "../dungeon/monsters.h"

Entity* findFreeEntity(
    Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            return &entities[i];
    }

    return nullptr;
}

Entity* spawnEntity(
    Entity entities[],
    uint8_t& entityCount,
    EntityType type,
    uint8_t x,
    uint8_t y)
{
    Entity* entity = nullptr;

    entity = findFreeEntity(entities, entityCount);

    if (entity == nullptr)
    {
        if (entityCount >= MAX_ENTITIES)
            return nullptr;

        entity = &entities[entityCount++];
    }

    entity->active = true;

    entity->type = type;

    entity->x = x;
    entity->y = y;

    entity->monsterID = MONSTER_NONE;

    entity->sprite = nullptr;

    return entity;
}

Entity* spawnMonster(
    Entity entities[],
    uint8_t& entityCount,
    MonsterID monsterID,
    uint8_t x,
    uint8_t y)
{
    Entity* entity = spawnEntity(
        entities,
        entityCount,
        ENTITY_MONSTER,
        x,
        y);

    if (entity == nullptr)
        return nullptr;

    entity->monsterID = monsterID;

    const Monster* monster = getMonster(monsterID);

    if (monster == nullptr)
        return entity;

    entity->sprite = monster->sprite;

    entity->character.team = TEAM_MONSTER;
    entity->character.state = STATE_ALIVE;

    entity->character.abilities = monster->abilities;

    entity->character.equipment.equipped[SLOT_MELEE_WEAPON] = monster->weapon;
    entity->character.equipment.equipped[SLOT_ARMOR] = monster->armor;

    entity->character.health.maxHP = getMonsterMaxHP(*monster);
    entity->character.health.currentHP = entity->character.health.maxHP;

    return entity;
}

void removeEntity(Entity& entity)
{
    entity.active = false;
}

Entity* getEntityAt(
    Entity entities[],
    uint8_t entityCount,
    uint8_t x,
    uint8_t y)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            continue;

        if (entities[i].x != x)
            continue;

        if (entities[i].y != y)
            continue;

        return &entities[i];
    }

    return nullptr;
}

Entity* getPlayerEntity(
    Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            continue;

        if (entities[i].type == ENTITY_PLAYER)
            return &entities[i];
    }

    return nullptr;
}

const Entity* getPlayerEntity(
    const Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            continue;

        if (entities[i].type == ENTITY_PLAYER)
            return &entities[i];
    }

    return nullptr;
}

void clearEntities(
    Entity entities[],
    uint8_t& entityCount)
{
    entityCount = 0;
}

const char* getEntityName(const Entity* entity)
{
    if (entity == nullptr)
    {
        return "Unknown";
    }

    switch (entity->type)
    {
        case ENTITY_PLAYER:
            return "Player";

        case ENTITY_MONSTER:
        {
            const Monster* monster = getMonster(entity->monsterID);

            if (monster != nullptr)
            {
                return monster->name;
            }

            return "Monster";
        }

        default:
            return "Unknown";
    }
}
