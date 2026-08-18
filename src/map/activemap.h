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

// Base terrain and temporary overlays remain separate. No current tile is
// difficult yet, but this is the single extension point for rubble, mud,
// snow, and similar future terrain.
bool isBaseTerrainDifficultAt(int x, int y);

bool hasLineOfSight(int startX, int startY, int endX, int endY);

// Checks every occupied square for large creatures, including a proposed
// attacker position used by monster pathfinding.
bool hasLineOfSightBetweenFootprintsAt(
    const Entity& attacker,
    int attackerX,
    int attackerY,
    const Entity& target);

// Ground-target equivalent of the footprint-aware entity LOS check.
bool hasLineOfSightFromFootprintAt(
    const Entity& entity,
    int entityX,
    int entityY,
    int targetX,
    int targetY);

// Chebyshev grid distance between the nearest occupied squares of two
// entities. This matches weapon combat and supports large footprints.
int getEntityGridDistance(const Entity& first, const Entity& second);

// Chebyshev distance from the nearest occupied square to one map tile.
int getEntityGridDistanceToTile(
    const Entity& entity,
    int tileX,
    int tileY);

#endif // PATHFINDERMINIEXTREME_025_ACTIVE_MAP_H
