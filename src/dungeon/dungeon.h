//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_DUNGEON_H
#define PATHFINDERMINIEXTREME_025_DUNGEON_H

#include <Arduino.h>
#include "../characters/characters.h"
#include "graphics/tiles.h"
#include "../data/entities.h"

constexpr uint8_t ROOM_SIZE = 15;
constexpr uint8_t TILE_SIZE = 16;
constexpr uint16_t SCREEN_SIZE = ROOM_SIZE * TILE_SIZE;
constexpr uint8_t NO_ROOM = 255;

//==================================================
// Rooms
//==================================================
enum RoomType : uint8_t {
    ROOM_ENTRANCE,
    ROOM_COMBAT,
    ROOM_PUZZLE,
    ROOM_TRAP,
    ROOM_AMBUSH,
    ROOM_LOCKED_DOOR,
    ROOM_BOSS,
    ROOM_TREASURE
  };

enum RoomShape : uint8_t {
    SHAPE_SQUARE,
    SHAPE_CROSS,
    SHAPE_CIRCLE
  };

enum RoomEntry {
    ENTRY_START,
    ENTRY_NORTH,
    ENTRY_EAST,
    ENTRY_SOUTH,
    ENTRY_WEST
  };

//==================================================
// Dungeon
//==================================================

struct DungeonSettings
{
    DungeonTheme theme;

    uint8_t partyStrength;

    //DungeonDifficulty difficulty;

    uint8_t roomCount;

    float encounterMultiplier;
    float treasureMultiplier;
    float bossMultiplier;
};

struct RoomMap {
    TileType tiles[ROOM_SIZE][ROOM_SIZE];
};

struct DungeonRoom {
    RoomType type;
    RoomShape shape;


    bool discovered;
    bool completed;

    uint8_t north;
    uint8_t south;
    uint8_t east;
    uint8_t west;

    RoomMap map;
};


constexpr uint8_t MAX_ROOMS = 5;
constexpr uint8_t MAX_DUNGEON_CHARACTERS = 16;

struct Dungeon {
    DungeonRoom rooms[MAX_ROOMS];
    Entity entities[MAX_ENTITIES];
    Character* characters[MAX_DUNGEON_CHARACTERS];
    uint8_t characterCount = 0;
    uint8_t currentRoom = 0;
    uint8_t entityCount = 0;
    };

extern Dungeon dungeon;

const char* roomTypeName(RoomType type);
void enterDungeon();
void generateDungeon(Dungeon& dungeon);
void generateRoom(DungeonRoom& room);
void loadRoom(Dungeon& dungeon, RoomEntry entry);
void addCharacterToDungeon(Character* character);

Character* getPlayerCharacter(Dungeon& dungeon);

#endif //PATHFINDERMINIEXTREME_025_DUNGEON_H
