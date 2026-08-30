#ifndef PATHFINDERMINIEXTREME_025_ROOMDRAW_H
#define PATHFINDERMINIEXTREME_025_ROOMDRAW_H

#include "dungeon.h"

void drawRoom(const DungeonRoom& room);
void drawTile(int tileX, int tileY, TileType tile);

// Draws persistent room dressing over the base terrain. Suspicion clues are
// deliberately independent of traps, so this renders harmless clues too.
void drawRoomTile(const DungeonRoom& room, int tileX, int tileY);

#endif // PATHFINDERMINIEXTREME_025_ROOMDRAW_H
