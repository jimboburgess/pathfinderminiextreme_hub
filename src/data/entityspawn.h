#ifndef PATHFINDERMINIEXTREME_025_ENTITYSPAWN_H
#define PATHFINDERMINIEXTREME_025_ENTITYSPAWN_H

#include "entities.h"

Entity* findFreeEntity(
    Entity entities[],
    uint8_t entityCount);

Entity* spawnEntity(
    Entity entities[],
    uint8_t& entityCount,
    EntityType type,
    uint8_t x,
    uint8_t y);

Entity* spawnMonster(
    Entity* entities,
    uint8_t& entityCount,
    MonsterID monsterID,
    uint8_t x,
    uint8_t y);

void removeEntity(Entity& entity);

Entity* getEntityAt(
    Entity entities[],
    uint8_t entityCount,
    uint8_t x,
    uint8_t y);

Entity* getPlayerEntity(
    Entity entities[],
    uint8_t entityCount);

const Entity* getPlayerEntity(
    const Entity entities[],
    uint8_t entityCount);

const char* getEntityName(const Entity* entity);

void clearEntities(
    Entity entities[],
    uint8_t& entityCount);

#endif