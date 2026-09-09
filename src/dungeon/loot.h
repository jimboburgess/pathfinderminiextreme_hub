#ifndef PATHFINDERMINIEXTREME_025_LOOT_H
#define PATHFINDERMINIEXTREME_025_LOOT_H

#include <stdint.h>

#include "monsters.h"

struct Character;
struct Entity;

struct LootTable
{
    uint16_t minGold;
    uint16_t maxGold;
};

enum LootSource : uint8_t
{
    LOOT_SOURCE_NORMAL_MONSTER,
    LOOT_SOURCE_STRONG_MONSTER,
    LOOT_SOURCE_CHEST,
    LOOT_SOURCE_BOSS,
    LOOT_SOURCE_FINAL_TREASURE,
    LOOT_SOURCE_COUNT
};

enum LootQuality : uint8_t
{
    LOOT_QUALITY_MUNDANE,
    LOOT_QUALITY_MASTERWORK,
    LOOT_QUALITY_MAGIC_1,
    LOOT_QUALITY_MAGIC_2,
    LOOT_QUALITY_MAGIC_3,
    LOOT_QUALITY_MAGIC_4,
    LOOT_QUALITY_MAGIC_5,
    LOOT_QUALITY_COUNT
};

struct LootQualityWeights
{
    uint8_t weights[LOOT_QUALITY_COUNT];
};

// Deterministic/tunable quality API. Each returned row totals 100.
const LootQualityWeights& getLootQualityWeights(uint8_t characterLevel,
                                                LootSource source);
LootQuality selectLootQuality(uint8_t characterLevel,
                              LootSource source,
                              uint8_t percentileRoll);
uint8_t getEquipmentDropChance(LootSource source);
uint8_t getLootQualityEffectiveBonus(LootQuality quality);
ItemInstance createLootEquipment(ItemID baseItem,
                                 LootQuality quality,
                                 uint8_t desiredWeaponProperties =
                                     WEAPON_PROPERTY_NONE);

uint16_t rollLootGold(const LootTable& table);

// Rolls an entity's Monster::lootTable once. Repeated calls are safe and do
// not reroll or replace loot already stored on the corpse.
void generateCorpseLoot(Entity& corpse, uint8_t characterLevel);
void generateChestLoot(Entity& chest,
                       LootTableID table,
                       uint8_t characterLevel,
                       LootSource source = LOOT_SOURCE_CHEST);

bool corpseHasLoot(const Entity& corpse);

// Transfers the corpse's generated gold to the recipient exactly once.
uint16_t takeCorpseGold(Entity& corpse, Character& recipient);

// Moves one item from a selected corpse slot into the recipient inventory.
// Returns false without removing anything when the recipient cannot carry it.
bool takeCorpseLootItem(Entity& corpse,
                         uint8_t slotIndex,
                         Character& recipient);

// Transfers pending gold, then attempts every item on the corpse, leaving any
// items that do not fit. The return value remains the number of items taken.
uint16_t takeAllCorpseLoot(Entity& corpse, Character& recipient);

// Marks a fully emptied corpse as looted and removes its map entity.
void finishLootingCorpse(Entity& corpse);

#endif // PATHFINDERMINIEXTREME_025_LOOT_H
