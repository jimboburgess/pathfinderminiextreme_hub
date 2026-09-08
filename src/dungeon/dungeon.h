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
#include "fountain.h"
#include "furniture.h"
#include "bellpuzzle.h"
#include "numbertilepuzzle.h"
#include "entitypersistence.h"

constexpr uint8_t ROOM_WIDTH = 15;
constexpr uint8_t ROOM_HEIGHT = 14;
constexpr uint8_t TILE_SIZE = 16;
constexpr uint16_t DUNGEON_PIXEL_WIDTH = ROOM_WIDTH * TILE_SIZE;
constexpr uint16_t DUNGEON_PIXEL_HEIGHT = ROOM_HEIGHT * TILE_SIZE;
constexpr uint8_t NO_ROOM = 255;
constexpr int8_t NO_DUNGEON_COORDINATE = 127;

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
    SHAPE_CAVE,
    SHAPE_ENTRANCE
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
    TileType tiles[ROOM_HEIGHT][ROOM_WIDTH];
};

constexpr uint8_t MAX_ROOM_CONNECTIONS = 4;
constexpr uint8_t ROOM_CONNECTION_MIN = 2;
constexpr uint8_t ROOM_HORIZONTAL_CONNECTION_MAX = ROOM_WIDTH - 3;
constexpr uint8_t ROOM_VERTICAL_CONNECTION_MAX = ROOM_HEIGHT - 3;

static_assert(
    ROOM_CONNECTION_MIN <= ROOM_HORIZONTAL_CONNECTION_MAX &&
    ROOM_CONNECTION_MIN <= ROOM_VERTICAL_CONNECTION_MAX,
    "Dungeon room dimensions are too small for safe room connections");

struct RoomConnection
{
    Direction direction;
    uint8_t x;
    uint8_t y;
};

struct DungeonRoom {
    RoomType type;
    DungeonPuzzleType puzzleType = PUZZLE_NONE;
    EncounterTheme encounterTheme = ENCOUNTER_NONE;
    RoomShape shape;


    bool discovered;
    bool completed;

    uint8_t north = NO_ROOM;
    uint8_t south = NO_ROOM;
    uint8_t east = NO_ROOM;
    uint8_t west = NO_ROOM;

    // Logical graph coordinates. These locate rooms relative to one another;
    // they are intentionally independent from the room's 15x14 tile map.
    int8_t dungeonX = NO_DUNGEON_COORDINATE;
    int8_t dungeonY = NO_DUNGEON_COORDINATE;

    RoomConnection connections[MAX_ROOM_CONNECTIONS] = {};
    uint8_t connectionCount = 0;

    RoomMap map;
    // Physical mechanisms and environmental clues are deliberately separate:
    // a clue can be a warning for a trap or harmless dungeon dressing.
    TrapInstance traps[MAX_TRAPS_PER_ROOM] = {};
    SuspicionInstance suspicions[MAX_SUSPICIONS_PER_ROOM] = {};
    DungeonFurnitureInstance furniture[MAX_FURNITURE_PER_ROOM] = {};
    HealingFountain fountain;
    DungeonNPCSpawn npcSpawn;
    BellPuzzleState bellPuzzle;
    NumberTilePuzzleState numberPuzzle;
};


constexpr uint8_t MIN_DUNGEON_ROOMS = 7;
constexpr uint8_t MAX_DUNGEON_ROOMS = 12;
constexpr uint8_t MAX_ROOMS = MAX_DUNGEON_ROOMS;
constexpr uint8_t MIN_BOSS_GRAPH_DISTANCE = 4;
constexpr uint8_t DUNGEON_LOOP_CHANCE_PERCENT = 25;
constexpr uint8_t FIRST_MIDDLE_ROOM_INDEX = 1;
constexpr uint8_t MIDDLE_ROOM_COUNT = 3;
constexpr uint8_t DUNGEON_RUBBLE_THEME_CHANCE_PERCENT = 70;
constexpr uint8_t OPTIONAL_RUBBLE_ROOM_CHANCE_PERCENT = 60;
constexpr uint8_t MAX_COMBATANTS = 16;

struct CompactDungeonRoomEntityStorage
{
    PersistentEntity persistentEntities[MAX_ENTITIES];
    // Stage 2 keeps transaction scratch inside the memory formerly reserved
    // by the room's full Entity array. Stage 3 can replace this with one shared
    // scratch buffer when the legacy compatibility member is removed.
    PersistentEntity transactionScratch[MAX_ENTITIES];
};

union DungeonRoomEntityStorage
{
    // Inactive Stage 2 compatibility member. Normal runtime code never makes
    // this member active; it preserves the old layout/fallback type until the
    // compact path has been verified and Stage 3 removes it.
    Entity legacyEntities[MAX_ENTITIES];
    CompactDungeonRoomEntityStorage compact;

    DungeonRoomEntityStorage() : compact{} {}
    DungeonRoomEntityStorage(const DungeonRoomEntityStorage& other)
        : compact(other.compact) {}
    DungeonRoomEntityStorage(DungeonRoomEntityStorage&& other)
        : compact(other.compact) {}
    DungeonRoomEntityStorage& operator=(
        const DungeonRoomEntityStorage& other)
    {
        compact = other.compact;
        return *this;
    }
    DungeonRoomEntityStorage& operator=(DungeonRoomEntityStorage&& other)
    {
        compact = other.compact;
        return *this;
    }
    ~DungeonRoomEntityStorage() { compact.~CompactDungeonRoomEntityStorage(); }
};

static_assert(sizeof(CompactDungeonRoomEntityStorage) <=
              sizeof(Entity) * MAX_ENTITIES,
              "Stage 2 compact and transaction storage must fit legacy room storage");

// Room-owned occupants are compact while inactive. Full Entity objects exist
// only in Dungeon::activeDungeonEntities for the currently loaded room.
struct DungeonRoomRuntime
{
    DungeonRoomEntityStorage entityStorage;
    uint8_t persistentEntityCount = 0;
    bool initialized = false;
    bool persistenceReady = false;
    // Only the currently matched prefix is retained; wrong input resets it.
    uint8_t bellEnteredCount = 0;
    bool numberCrossingActive = false;
    uint8_t numberCurrentStep = 0;
    uint8_t numberPreviousDigit = 0;
    uint8_t numberCurrentDigit = 0;
};

struct Dungeon {
    DungeonRoom rooms[MAX_ROOMS];
    DungeonRoomRuntime roomRuntime[MAX_ROOMS];

    // The only full dungeon Entity collection used by active gameplay.
    Entity activeDungeonEntities[MAX_ENTITIES];

    // Compatibility view used throughout combat/rendering/movement. It points
    // to activeDungeonEntities whenever a dungeon room is loaded.
    Entity* entities = nullptr;
    uint8_t roomCount = 0;
    uint8_t currentRoom = 0;
    uint8_t bossRoom = NO_ROOM;
    uint8_t treasureRoom = NO_ROOM;
    uint8_t riddleRoom = NO_ROOM;
    uint8_t entityCount = 0;
    uint8_t loadedRoom = NO_ROOM;
    bool runActive = false;
    // Rolled once when a new dungeon is generated. Room terrain itself stores
    // the selected room/patch layout for the lifetime of the run.
    bool hasRubbleTheme = false;
    bool finalEncounterCleared = false;
    bool finalTreasureLooted = false;
    bool completed = false;
    };

struct DungeonRubblePlan
{
    bool enabled = false;
    bool middleRooms[MIDDLE_ROOM_COUNT] = {};
};

extern Dungeon dungeon;

const char* roomTypeName(RoomType type);
DungeonRubblePlan createDungeonRubblePlan(
    uint8_t themeRoll,
    uint8_t guaranteedMiddleRoomRoll,
    uint8_t optionalRoomChanceRoll,
    uint8_t optionalMiddleRoomRoll);
void enterDungeon();
void generateDungeon(Dungeon& dungeon);
void generateRoom(DungeonRoom& room);
bool loadRoom(Dungeon& dungeon, RoomEntry entry);
bool persistActiveDungeonRoom(Dungeon& dungeon);
bool suspendDungeonRun(Dungeon& dungeon);
void resetDungeonRun(Dungeon& dungeon);
void updateCurrentDungeonRoomCompletion(Dungeon& dungeon);
bool isDungeonRunComplete(const Dungeon& dungeon);
bool hasResumableDungeon(const Dungeon& dungeon);
void markDungeonCompletedOnTownReturn(Dungeon& dungeon);

#endif //PATHFINDERMINIEXTREME_025_DUNGEON_H
