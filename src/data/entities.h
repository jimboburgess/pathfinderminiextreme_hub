//
// Created by james on 7/22/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_ENTITIES_H
#define PATHFINDERMINIEXTREME_025_ENTITIES_H

#include "../characters/characters.h"
#include "dungeon/monsters.h"
#include "dungeon/turns.h"
#include "graphics/sprites.h"

//==================================================
// Entities
//==================================================
enum EntityType : uint8_t {
    ENTITY_NONE,
    ENTITY_PLAYER,
    ENTITY_MONSTER,
    ENTITY_CHEST,
    ENTITY_LOOT,
    ENTITY_NPC
  };

constexpr uint8_t MAX_CORPSE_LOOT_SLOTS = 8;

struct LootData
{
    InventorySlot slots[MAX_CORPSE_LOOT_SLOTS];
    uint8_t itemCount = 0;
    bool generated = false;
};

struct Entity
{
    EntityType type;

    uint8_t x;
    uint8_t y;

    bool active = false;

    Character character;

    MonsterID monsterID = MONSTER_NONE;
    const Monster* monster = nullptr;

    LootData loot;

    const uint16_t* sprite = nullptr;

    uint8_t spriteWidth = SPRITE_W;
    uint8_t spriteHeight = SPRITE_H;

    TurnState turn;
};

constexpr uint8_t MAX_ENTITIES = 16;

#endif //PATHFINDERMINIEXTREME_025_ENTITIES_H
