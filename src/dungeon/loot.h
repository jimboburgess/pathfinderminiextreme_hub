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

uint16_t rollLootGold(const LootTable& table);

// Rolls an entity's Monster::lootTable once. Repeated calls are safe and do
// not reroll or replace loot already stored on the corpse.
void generateCorpseLoot(Entity& corpse);
void generateChestLoot(Entity& chest, LootTableID table);

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
