//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_INTERACTION_H
#define PATHFINDERMINIEXTREME_025_INTERACTION_H

#include <stdint.h>

struct Entity;

constexpr int CHEST_LOCK_DC = 15;
constexpr int CHEST_FORCE_OPEN_DC = 15;

// Handles the context-sensitive map interaction at the square indicated by
// the normal movement cursor. Returns true only when it consumed A.
bool tryInteractWithFacingEntity();

// Called by the existing menu system while its locked-chest context menu is
// open. They only affect the chest selected by the preceding interaction.
void pickLockedChest();
void forceOpenLockedChest();
void pickRiddlemanDoorLock();

// Healing-fountain actions are selected through the same contextual menu as
// other dungeon interactables.
void drinkFromFountain();



#endif //PATHFINDERMINIEXTREME_025_INTERACTION_H
