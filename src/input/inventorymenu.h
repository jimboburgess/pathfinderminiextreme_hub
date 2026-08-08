//
// Compact, selectable inventory and corpse-loot screens.
//

#ifndef PATHFINDERMINIEXTREME_025_INVENTORYMENU_H
#define PATHFINDERMINIEXTREME_025_INVENTORYMENU_H

struct Entity;

bool isInventoryMenuOpen();

// Opens the active player's carried inventory. It is usable both from the
// exploration Character menu and the combat Use Item menu.
void openPlayerInventoryMenu();

// Opens the fixed-size item collection stored on a dead monster entity.
void openCorpseLootMenu(Entity& corpse);

void closeInventoryMenu();
void handleInventoryMenuButtons();
void drawInventoryMenu();

#endif // PATHFINDERMINIEXTREME_025_INVENTORYMENU_H
