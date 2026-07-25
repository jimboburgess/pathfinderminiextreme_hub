#ifndef PATHFINDERMINIEXTREME_025_ROOMDRAW_H
#define PATHFINDERMINIEXTREME_025_ROOMDRAW_H

#include "dungeon.h"

void drawRoom(const DungeonRoom& room);
void drawEntities(const Dungeon& dungeon);
void drawTile(int tileX, int tileY, TileType tile);
void printRoom(const DungeonRoom& room);

#endif // PATHFINDERMINIEXTREME_025_ROOMDRAW_H