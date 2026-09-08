#ifndef PATHFINDERMINIEXTREME_025_ENTITYPERSISTENCE_H
#define PATHFINDERMINIEXTREME_025_ENTITYPERSISTENCE_H

#include <stdint.h>

#include "data/entities.h"

// Eight slots preserve every current monster inventory (at most two item
// types) while leaving deliberate room for future monster tools/consumables.
// Packing fails instead of discarding state when this capacity is exceeded.
constexpr uint8_t MAX_PERSISTENT_MONSTER_ITEMS = 8;

enum PersistentEntityFlag : uint8_t
{
    PERSISTENT_ENTITY_ACTIVE = 1 << 0,
    PERSISTENT_MONSTER_AWARE = 1 << 1,
    PERSISTENT_MONSTER_REVEALED = 1 << 2,
    PERSISTENT_MONSTER_HAS_LAST_KNOWN = 1 << 3
};

struct PersistentMonsterState
{
    MonsterID monsterID = MONSTER_NONE;
    CharacterState state = STATE_ALIVE;
    int currentHP = 0;
    int maxHP = 0;
    int currentMP = 0;
    ConditionData conditions{};
    InventorySlot items[MAX_PERSISTENT_MONSTER_ITEMS] = {};
    uint8_t itemCount = 0;
    LootData loot{};
    uint8_t lastKnownX = 0;
    uint8_t lastKnownY = 0;
    Direction idleDirection = DIR_NORTH;
    uint8_t idleStepsRemaining = 0;
    uint32_t nextIdleActionTime = 0;
};

struct PersistentChestState
{
    LootData loot{};
    bool locked = false;
    bool opened = false;
};

struct PersistentNPCState
{
    NPCID npcID = NPC_NONE;
};

union PersistentEntityPayload
{
    PersistentMonsterState monster;
    PersistentChestState chest;
    PersistentNPCState npc;

    PersistentEntityPayload() {}
    ~PersistentEntityPayload() {}
};

struct PersistentEntity
{
    EntityType type = ENTITY_NONE;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t flags = 0;
    PersistentEntityPayload payload{};
};

static_assert(sizeof(PersistentEntity) < sizeof(Entity),
              "Persistent entities must remain smaller than active entities");
static_assert(sizeof(PersistentEntity) <= 512,
              "Persistent entity exceeded the Stage 1 RAM budget");

bool packPersistentEntity(
    const Entity& source, PersistentEntity& destination);
bool inflatePersistentEntity(
    const PersistentEntity& source, Entity& destination);

#endif
