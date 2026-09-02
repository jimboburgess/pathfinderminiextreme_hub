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
    TILE_GIANT_SPIDER_START,
    TILE_SKELETON_MAGE_START,
    TILE_SKELETON_START,
    TILE_FOUNTAIN,
    TILE_GRASS,
    TILE_TREE,
    TILE_MUD,
    TILE_BRUSH,
    TILE_STONE,
    TILE_WATER,

    // Appended to preserve existing serialized tile values.
    TILE_RUBBLE,
    TILE_PILLAR,
    TILE_STATUE,
    TILE_BRAZIER,
    TILE_CRATE,
    TILE_BARREL
  };

inline bool isDungeonFloorTerrain(TileType tile)
{
    return tile == TILE_FLOOR || tile == TILE_RUBBLE;
}

inline bool isWallLikeDungeonTile(TileType tile)
{
    return tile == TILE_WALL || tile == TILE_PILLAR ||
           tile == TILE_STATUE;
}

inline bool isTileBlockingSight(TileType tile)
{
    return isWallLikeDungeonTile(tile) || tile == TILE_TREE;
}

TileType getForestTile(int x, int y);

extern const uint16_t grassTile[16 * 16];
extern const uint16_t treeTile[16 * 16];
extern const uint16_t dungeonWallTiles[3][16 * 16];
extern const uint16_t dungeonFloorTiles[3][16 * 16];
extern const uint16_t dungeonRubble16x16[16 * 16];
extern const uint16_t dungeonSpikes16x16[16 * 16];
extern const uint16_t dungeonPillar16x16[16 * 16];
extern const uint16_t dungeonStatue16x16[16 * 16];
extern const uint16_t dungeonBrazier16x16[16 * 16];
extern const uint16_t dungeonCrate16x16[16 * 16];
extern const uint16_t dungeonBarrel16x16[16 * 16];
extern const uint16_t dungeonDoor16x16[16 * 16];

extern const uint16_t chestclosed[16 * 16];
extern const uint16_t chestopenwith[16 * 16];
extern const uint16_t chestopenwithout[16 * 16];

#endif //PATHFINDERMINIEXTREME_025_TILES_H
