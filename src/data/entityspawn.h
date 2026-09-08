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

// Applies only definition-derived monster state. It deliberately does not
// roll HP or grant mutable starting inventory, so persistence inflation can
// restore those values without consuming RNG.
bool initializeMonsterDefinitionState(Entity& entity, MonsterID monsterID);

Entity* spawnNPC(
    Entity* entities,
    uint8_t& entityCount,
    NPCID npcID,
    uint8_t x,
    uint8_t y);


// Applies definition-derived NPC identity, team, and rendering state.
bool initializeNPCDefinitionState(Entity& entity, NPCID npcID);

void removeEntity(Entity& entity);

uint8_t getEntityTileWidth(const Entity& entity);
uint8_t getEntityTileHeight(const Entity& entity);
bool entityOccupiesTile(const Entity& entity, int tileX, int tileY);

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
