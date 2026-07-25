//
// Created by james on 7/20/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_TILES_H
#define PATHFINDERMINIEXTREME_025_TILES_H

#include <Arduino.h>

enum TileType : uint8_t {
    TILE_VOID,  // outside map
    TILE_WALL,
    TILE_FLOOR,
    TILE_DOOR,
    TILE_CHEST_SPAWN,
    TILE_LOOT_SPAWN,
    TILE_NPC_SPAWN,
    TILE_TRAP,
    TILE_EXIT,
    TILE_PLAYER_START,
    TILE_ENEMY_START,
    TILE_GRASS,
    TILE_TREE,
    TILE_MUD,
    TILE_BRUSH,
    TILE_STONE,
    TILE_WATER
  };

TileType getForestTile(int x, int y);

extern const uint16_t grassTile[16 * 16];
extern const uint16_t treeTile[16 * 16];

#endif //PATHFINDERMINIEXTREME_025_TILES_H
