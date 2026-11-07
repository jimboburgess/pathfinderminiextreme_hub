#ifndef PATHFINDERMINIEXTREME_025_ITEMDROPVISUAL_H
#define PATHFINDERMINIEXTREME_025_ITEMDROPVISUAL_H

#include <stdint.h>

#include "characters/items.h"

struct Entity;

constexpr uint16_t HEALING_POTION_DROP_COLOR = 0x867F;
constexpr uint16_t MANA_POTION_DROP_COLOR = 0x0013;

uint16_t getDroppedPotionColor(ItemID itemID);
void drawDroppedItem(const Entity& entity);

#endif
