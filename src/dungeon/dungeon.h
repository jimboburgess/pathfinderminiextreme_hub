//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_DUNGEON_H
#define PATHFINDERMINIEXTREME_025_DUNGEON_H

#include <Arduino.h>
#include "../characters/characters.h"
#include "../data/game.h"
#include "graphics/tiles.h"
#include "../data/entities.h"
#include "traps.h"

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
    ROOM_AMBUSH,
    ROOM_PUZZLE,
    ROOM_TREASURE,
    ROOM_EMPTY,
    ROOM_BOSS
};

enum EncounterTheme : uint8_t {
    ENCOUNTER_NONE,
    ENCOUNTER_GOBLIN,
    ENCOUNTER_UNDEAD,
    ENCOUNTER_ABERRATION
};

enum RoomShape : uint8_t {
    SHAPE_SQUARE,
    SHAPE_CROSS,
    SHAPE_CIRCLE,
    SHAPE_SMALL_RECTANGLE,
    SHAPE_L,
    SHAPE_WINDING_CORRIDOR,
    SHAPE_CAVE
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

struct RoomMap {
    TileType tiles[ROOM_SIZE][ROOM_SIZE];
};

constexpr uint8_t MAX_ROOM_CONNECTIONS = 4;
constexpr uint8_t ROOM_CONNECTION_MIN = 2;
constexpr uint8_t ROOM_CONNECTION_MAX = ROOM_SIZE - 3;

static_assert(
    ROOM_CONNECTION_MIN <= ROOM_CONNECTION_MAX,
    "ROOM_SIZE is too small for safe room connections");

struct RoomConnection
{
    Direction direction;
    uint8_t x;
    uint8_t y;
};

struct DungeonRoom {
    RoomType type;
    EncounterTheme encounterTheme = ENCOUNTER_NONE;
    RoomShape shape;


    bool discovered;
    bool completed;

    uint8_t north;
    uint8_t south;
    uint8_t east;
    uint8_t west;

    RoomConnection connections[MAX_ROOM_CONNECTIONS] = {};
    uint8_t connectionCount = 0;

    RoomMap map;
    // Physical mechanisms and environmental clues are deliberately separate:
    // a clue can be a warning for a trap or harmless dungeon dressing.
    TrapInstance traps[MAX_TRAPS_PER_ROOM] = {};
    SuspicionInstance suspicions[MAX_SUSPICIONS_PER_ROOM] = {};
};


constexpr uint8_t MAX_ROOMS = 6;
constexpr uint8_t BOSS_ROOM_INDEX = 4;
constexpr uint8_t FINAL_DUNGEON_ROOM_INDEX = 5;
constexpr uint8_t GIANT_SPIDER_TEST_ROOM_INDEX = BOSS_ROOM_INDEX;
constexpr uint8_t MAX_COMBATANTS = 16;
constexpr uint8_t NO_ENTITY_SLOT = 255;

// Mutable room occupants live separately from the generated room map.  The
// active dungeon entity pointer below aliases one of these arrays, so there is
// only one authoritative copy of monster HP, conditions, corpse loot, and
// position for each room.
struct DungeonRoomRuntime
{
    Entity entities[MAX_ENTITIES];
    uint8_t entityCount = 0;
    uint8_t playerSlot = NO_ENTITY_SLOT;
    bool initialized = false;
};

struct Dungeon {
    DungeonRoom rooms[MAX_ROOMS];
    DungeonRoomRuntime roomRuntime[MAX_ROOMS];

    // Compatibility view of the currently loaded room.  This points directly
    // into roomRuntime[currentRoom]; it is not a second entity collection.
    Entity* entities = nullptr;
    uint8_t currentRoom = 0;
    uint8_t entityCount = 0;
    uint8_t loadedRoom = NO_ROOM;
    bool runActive = false;
    bool finalEncounterCleared = false;
    bool finalTreasureLooted = false;
    bool completed = false;
    };

extern Dungeon dungeon;

const char* roomTypeName(RoomType type);
void enterDungeon();
void generateDungeon(Dungeon& dungeon);
void generateRoom(DungeonRoom& room);
void loadRoom(Dungeon& dungeon, RoomEntry entry);
void suspendDungeonRun(Dungeon& dungeon);
void resetDungeonRun(Dungeon& dungeon);
void updateCurrentDungeonRoomCompletion(Dungeon& dungeon);
bool isDungeonRunComplete(const Dungeon& dungeon);
bool hasResumableDungeon(const Dungeon& dungeon);
void markDungeonCompletedOnTownReturn(Dungeon& dungeon);

#endif //PATHFINDERMINIEXTREME_025_DUNGEON_H
