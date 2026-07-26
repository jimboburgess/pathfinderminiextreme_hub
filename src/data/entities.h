//
// Created by james on 7/22/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_ENTITIES_H
#define PATHFINDERMINIEXTREME_025_ENTITIES_H

#include "../characters/characters.h"
#include "../dungeon/monsters.h"

//==================================================
// Entities
//==================================================
enum EntityType : uint8_t {
    ENTITY_NONE,
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_CHEST,
    ENTITY_LOOT,
    ENTITY_NPC
  };

struct Entity
{
    EntityType type;

    uint8_t x;
    uint8_t y;

    bool active = false;

    MonsterID monsterID = MONSTER_NONE;

    Character* character = nullptr;

    const uint16_t* sprite = nullptr;
};

void drawEntities(
    Entity entities[],
    uint8_t entityCount,
    uint8_t tileSize);

Entity* spawnEntity(
    Entity entities[],
    uint8_t& entityCount,
    EntityType type,
    uint8_t x,
    uint8_t y);

void removeEntity(Entity& entity);

Entity* getEntityAt(
    Entity entities[],
    uint8_t entityCount,
    uint8_t x,
    uint8_t y);

Entity* findFreeEntity(
    Entity entities[],
    uint8_t entityCount);

Entity* getPlayerEntity(
    Entity entities[],
    uint8_t entityCount);

const Entity* getPlayerEntity(
    const Entity entities[],
    uint8_t entityCount);

void clearEntities(
    Entity entities[],
    uint8_t& entityCount);

constexpr uint8_t MAX_ENTITIES = 16;

#endif //PATHFINDERMINIEXTREME_025_ENTITIES_H
