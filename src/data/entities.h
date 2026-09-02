//
// Created by james on 7/22/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_ENTITIES_H
#define PATHFINDERMINIEXTREME_025_ENTITIES_H

#include "../characters/characters.h"
#include "game.h"
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
    uint16_t gold = 0;
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
    bool awareOfPlayer = false;
    bool revealedToPlayer = false;
    bool visibleToPlayer = false;
    // Rendering-only LOS cache. Targeting continues to use visibleToPlayer
    // and the existing live LOS checks.
    bool playerHasLineOfSight = false;
    uint8_t lastKnownX = 0;
    uint8_t lastKnownY = 0;
    bool hasLastKnownPosition = false;
    // Zero for initial combatants. Reinforcements skip turns until combat has
    // advanced beyond the round in which they heard and joined the fight.
    uint8_t reinforcementJoinedRound = 0;

    // Exploration-only movement state. MonsterIdleBehavior remains static in
    // the monster definition; each spawned Entity owns its independent timer
    // and patrol segment.
    Direction idleDirection = DIR_NORTH;
    uint8_t idleStepsRemaining = 0;
    uint32_t nextIdleActionTime = 0;

    LootData loot;

    // Chest state is independent of generated loot and sprite selection so a
    // locked, unlocked, and opened chest can persist as distinct states.
    bool locked = false;
    bool opened = false;

    const uint16_t* sprite = nullptr;

    uint8_t spriteWidth = SPRITE_W;
    uint8_t spriteHeight = SPRITE_H;

    TurnState turn;
};

constexpr uint8_t MAX_ENTITIES = 16;

#endif //PATHFINDERMINIEXTREME_025_ENTITIES_H
