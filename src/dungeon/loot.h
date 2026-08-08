#ifndef PATHFINDERMINIEXTREME_025_LOOT_H
#define PATHFINDERMINIEXTREME_025_LOOT_H

#include <stdint.h>

struct Character;
struct Entity;

// Rolls an entity's Monster::lootTable once. Repeated calls are safe and do
// not reroll or replace loot already stored on the corpse.
void generateCorpseLoot(Entity& corpse);

bool corpseHasLoot(const Entity& corpse);

// Moves one item from a selected corpse slot into the recipient inventory.
// Returns false without removing anything when the recipient cannot carry it.
bool takeCorpseLootItem(Entity& corpse,
                         uint8_t slotIndex,
                         Character& recipient);

// Attempts every item on the corpse, leaving any items that do not fit.
uint16_t takeAllCorpseLoot(Entity& corpse, Character& recipient);

// Marks a fully emptied corpse as looted and removes its map entity.
void finishLootingCorpse(Entity& corpse);

#endif // PATHFINDERMINIEXTREME_025_LOOT_H
