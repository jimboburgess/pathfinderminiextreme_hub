#ifndef PATHFINDERMINIEXTREME_025_ACTIVE_MAP_H
#define PATHFINDERMINIEXTREME_025_ACTIVE_MAP_H

#include <stdint.h>

#include "graphics/tiles.h"

struct Entity;

// The forest and dungeon expose different backing arrays, but combat and
// monster AI should always operate on whichever map is currently active.
Entity* getActiveMapEntities(uint8_t& entityCount);
Entity* getActiveMapPlayer();

int getActiveMapWidth();
int getActiveMapHeight();
bool isInsideActiveMap(int x, int y);
TileType getActiveMapTile(int x, int y);

bool hasLineOfSight(int startX, int startY, int endX, int endY);

// Checks every occupied square for large creatures, including a proposed
// attacker position used by monster pathfinding.
bool hasLineOfSightBetweenFootprintsAt(
    const Entity& attacker,
    int attackerX,
    int attackerY,
    const Entity& target);

#endif // PATHFINDERMINIEXTREME_025_ACTIVE_MAP_H
