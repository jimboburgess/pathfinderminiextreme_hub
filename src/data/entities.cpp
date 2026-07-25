//
// Created by james on 7/22/2026.
//

#include "entities.h"

Entity* spawnEntity(
    Entity entities[],
    uint8_t& entityCount,
    EntityType type,
    uint8_t x,
    uint8_t y) {
    if (entityCount >= MAX_ENTITIES)
        return nullptr;

    Entity& entity = entities[entityCount++];

    entity = {};
    entity.type = type;
    entity.x = x;
    entity.y = y;
    entity.active = true;
    entity.monsterID = MONSTER_NONE;

    return &entity;
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
        if (entities[i].active &&
            entities[i].x == x &&
            entities[i].y == y)
        {
            return &entities[i];
        }
    }

    return nullptr;
}

Entity* findFreeEntity(
    Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
        {
            return &entities[i];
        }
    }

    return nullptr;
}

void clearEntities(
    Entity entities[],
    uint8_t& entityCount)
{
    for (uint8_t i = 0; i < MAX_ENTITIES; i++)
    {
        entities[i].active = false;
    }
    entityCount = 0;
}

Entity* getPlayerEntity(
    Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (entities[i].active &&
            entities[i].type == ENTITY_PLAYER)
        {
            return &entities[i];
        }
    }

    return nullptr;
}

const Entity* getPlayerEntity(
    const Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (entities[i].active &&
            entities[i].type == ENTITY_PLAYER)
        {
            return &entities[i];
        }
    }

    return nullptr;
}