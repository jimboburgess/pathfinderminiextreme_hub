//
// Created by james on 7/12/2026.
//

#include "dungeon.h"
#include <Arduino.h>
#include "roomgen.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "combat.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "mapeffects.h"

Dungeon dungeon;

namespace
{
void resetRoomTurnState(DungeonRoomRuntime& runtime)
{
    for (uint8_t i = 0; i < runtime.entityCount; i++)
        runtime.entities[i].turn = TurnState{};
}

void updateRoomCompletion(Dungeon& dungeon, uint8_t roomIndex)
{
    if (roomIndex >= MAX_ROOMS)
        return;

    DungeonRoomRuntime& runtime = dungeon.roomRuntime[roomIndex];

    if (!runtime.initialized)
        return;

    bool hasLivingMonster = false;

    for (uint8_t i = 0; i < runtime.entityCount; i++)
    {
        const Entity& entity = runtime.entities[i];

        if (entity.active && entity.type == ENTITY_MONSTER &&
            entity.character.team == TEAM_MONSTER &&
            entity.character.state != STATE_DEAD &&
            entity.character.state != STATE_LOOTED)
        {
            hasLivingMonster = true;
            break;
        }
    }

    dungeon.rooms[roomIndex].completed = !hasLivingMonster;
}

void detachLoadedDungeonPlayer(Dungeon& dungeon)
{
    if (dungeon.loadedRoom >= MAX_ROOMS || dungeon.entities == nullptr)
        return;

    DungeonRoomRuntime& runtime =
        dungeon.roomRuntime[dungeon.loadedRoom];
    runtime.entityCount = dungeon.entityCount;

    Entity* playerEntity = nullptr;

    if (runtime.playerSlot < runtime.entityCount)
    {
        Entity& reservedPlayer = runtime.entities[runtime.playerSlot];

        if (reservedPlayer.active && reservedPlayer.type == ENTITY_PLAYER)
            playerEntity = &reservedPlayer;
    }

    // Defensive compatibility for a room created before the reserved-slot
    // convention was established.
    if (playerEntity == nullptr)
        playerEntity = getPlayerEntity(runtime.entities, runtime.entityCount);

    if (playerEntity != nullptr)
    {
        player = playerEntity->character;
        *playerEntity = Entity{};
    }

    resetRoomTurnState(runtime);
    updateRoomCompletion(dungeon, dungeon.loadedRoom);
}

void initializeRoomEntities(
    Dungeon& dungeon,
    DungeonRoom& room,
    DungeonRoomRuntime& runtime)
{
    runtime.entityCount = 0;
    runtime.playerSlot = NO_ENTITY_SLOT;

    for (uint8_t i = 0; i < MAX_ENTITIES; i++)
        runtime.entities[i] = Entity{};

    dungeon.entityCount = 0;

    // Marker tiles are consumed exactly once for this dungeon run. Subsequent
    // visits bind the same runtime array instead of reconstructing monsters.
    for (int y = 0; y < ROOM_SIZE; y++)
    {
        for (int x = 0; x < ROOM_SIZE; x++)
        {
            switch (room.map.tiles[y][x])
            {
                case TILE_ENEMY_START:
                    spawnMonster(
                        dungeon.entities,
                        dungeon.entityCount,
                        MONSTER_GOBLIN_SCIMITAR,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_GIANT_SPIDER_START:
                    spawnMonster(
                        dungeon.entities,
                        dungeon.entityCount,
                        MONSTER_GIANT_SPIDER,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_CHEST_SPAWN:
                    spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_CHEST,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_LOOT_SPAWN:
                    spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_LOOT,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_NPC_SPAWN:
                    spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_NPC,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                default:
                    break;
            }
        }
    }

    runtime.entityCount = dungeon.entityCount;
    runtime.initialized = true;
}

Entity* attachDungeonPlayer(
    Dungeon& dungeon,
    DungeonRoomRuntime& runtime)
{
    if (runtime.playerSlot == NO_ENTITY_SLOT)
    {
        if (dungeon.entityCount >= MAX_ENTITIES)
            return nullptr;

        runtime.playerSlot = dungeon.entityCount++;
    }

    if (runtime.playerSlot >= MAX_ENTITIES)
        return nullptr;

    if (runtime.playerSlot >= dungeon.entityCount)
        dungeon.entityCount = runtime.playerSlot + 1;

    Entity& playerEntity = dungeon.entities[runtime.playerSlot];
    playerEntity = Entity{};
    playerEntity.active = true;
    playerEntity.type = ENTITY_PLAYER;
    playerEntity.character = player;
    playerEntity.sprite = getPlayerSprite(
        playerEntity.character.characterClass);

    runtime.entityCount = dungeon.entityCount;
    return &playerEntity;
}

bool findSafePlayerEntry(
    const Dungeon& dungeon,
    const DungeonRoom& room,
    uint8_t requestedX,
    uint8_t requestedY,
    uint8_t& resultX,
    uint8_t& resultY)
{
    int bestDistance = ROOM_SIZE * 2;
    bool found = false;

    for (uint8_t y = 0; y < ROOM_SIZE; y++)
    {
        for (uint8_t x = 0; x < ROOM_SIZE; x++)
        {
            if (room.map.tiles[y][x] != TILE_FLOOR ||
                getEntityAt(
                    dungeon.entities,
                    dungeon.entityCount,
                    x,
                    y) != nullptr)
            {
                continue;
            }

            const int distance =
                abs(static_cast<int>(x) - requestedX) +
                abs(static_cast<int>(y) - requestedY);

            if (!found || distance < bestDistance)
            {
                found = true;
                bestDistance = distance;
                resultX = x;
                resultY = y;
            }
        }
    }

    return found;
}
}

const char* roomTypeName(RoomType type)
{
    switch (type)
    {
        case ROOM_ENTRANCE:     return "Entrance";
        case ROOM_COMBAT:       return "Combat";
        case ROOM_PUZZLE:       return "Puzzle";
        case ROOM_TRAP:         return "Trap";
        case ROOM_AMBUSH:       return "Ambush";
        case ROOM_LOCKED_DOOR:  return "Locked Door";
        case ROOM_BOSS:         return "Boss";
        case ROOM_TREASURE:     return "Treasure";
    }

    return "Unknown";
}

void enterDungeon()
{
    // Entering a map must never inherit a stale encounter. Return to Town is
    // the primary cleanup path; this is the requested defensive safety net.
    abortCombat();

    const bool resumingRun = dungeon.runActive;

    if (resumingRun)
    {
        dungeon.currentRoom = 0;
        loadRoom(dungeon, ENTRY_START);
    }
    else
    {
        generateDungeon(dungeon);
    }

    gameState = GAME_DUNGEON;
    setGameMessage(resumingRun ? "Resumed dungeon" : "Entered dungeon");

    Entity* player = getPlayerEntity(
        dungeon.entities,
        dungeon.entityCount);

    if (player)
    {
        previousPlayerPosition.x = player->x;
        previousPlayerPosition.y = player->y;
    }

    previousMoveDirection = moveDirection;

    backgroundNeedsRedraw = true;

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

void generateDungeon(Dungeon& dungeon)
{
    resetDungeonRun(dungeon);
    dungeon.currentRoom = 0;

    // Clear room connections
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        dungeon.rooms[i].north = NO_ROOM;
        dungeon.rooms[i].south = NO_ROOM;
        dungeon.rooms[i].east  = NO_ROOM;
        dungeon.rooms[i].west  = NO_ROOM;

        clearRoomConnections(dungeon.rooms[i]);

        dungeon.rooms[i].discovered = false;
        dungeon.rooms[i].completed = false;
    }

    // Connect the rooms
    for (int i = 0; i < MAX_ROOMS - 1; i++)
    {
        dungeon.rooms[i].east = i + 1;
        dungeon.rooms[i + 1].west = i;
    }

    dungeon.rooms[0].type = ROOM_ENTRANCE;

    dungeon.rooms[1].type = random(2) ? ROOM_COMBAT : ROOM_PUZZLE;

    switch (random(2))
    {
        case 0:
            dungeon.rooms[2].type = ROOM_TRAP;
            break;

        case 1:
            dungeon.rooms[2].type = ROOM_AMBUSH;
            break;
    }

    dungeon.rooms[3].type = ROOM_BOSS;
    dungeon.rooms[4].type = ROOM_TREASURE;

    dungeon.rooms[0].discovered = true;

    // Generate every room
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        populateRoomConnections(dungeon.rooms[i]);
        dungeon.rooms[i].shape =
            randomProductionRoomShape(dungeon.rooms[i]);
        generateRoom(dungeon.rooms[i]);
    }

    // Keep the temporary Giant Spider encounter out of the entrance. Prefer
    // the deepest room, then walk backward through later rooms if its current
    // content leaves no valid 2x2 floor footprint.
    for (int roomIndex = GIANT_SPIDER_TEST_ROOM_INDEX;
         roomIndex > 0;
         roomIndex--)
    {
        if (placeGiantSpiderEncounter(dungeon.rooms[roomIndex]))
            break;
    }

    dungeon.runActive = true;

    // Load the starting room
    loadRoom(dungeon, ENTRY_START);
}

void loadRoom(Dungeon& dungeon, RoomEntry entry)
{
    clearMapEffects();
    detachLoadedDungeonPlayer(dungeon);

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    DungeonRoomRuntime& runtime =
        dungeon.roomRuntime[dungeon.currentRoom];

    dungeon.entities = runtime.entities;
    dungeon.entityCount = runtime.entityCount;

    if (!runtime.initialized)
        initializeRoomEntities(dungeon, room, runtime);

    resetRoomTurnState(runtime);

    uint8_t entryX = ROOM_SIZE / 2;
    uint8_t entryY = ROOM_SIZE / 2;

    // Cardinal entries use the destination room's connection. The centered
    // values above remain only as a defensive fallback for corrupted data.
    getRoomEntryPosition(room, entry, entryX, entryY);

    // A surviving monster may have moved onto a doorway before the player
    // fled. Resume at the requested entrance when it is free, otherwise use
    // the nearest connected floor tile instead of overlapping an entity.
    if (room.map.tiles[entryY][entryX] != TILE_FLOOR ||
        getEntityAt(
            dungeon.entities,
            dungeon.entityCount,
            entryX,
            entryY) != nullptr)
    {
        if (!findSafePlayerEntry(
                dungeon, room, entryX, entryY, entryX, entryY))
        {
            return;
        }
    }

    Entity* playerEntity = attachDungeonPlayer(dungeon, runtime);

    if (playerEntity == nullptr)
        return;

    playerEntity->x = entryX;
    playerEntity->y = entryY;

    room.discovered = true;
    dungeon.loadedRoom = dungeon.currentRoom;
    runtime.entityCount = dungeon.entityCount;
}

void suspendDungeonRun(Dungeon& dungeon)
{
    if (!dungeon.runActive)
        return;

    // Remove the transient map copy of the player now, before town rest,
    // shopping, or loading can change the authoritative global Character.
    // Monsters and other room occupants remain exactly where they are.
    detachLoadedDungeonPlayer(dungeon);
}

void resetDungeonRun(Dungeon& dungeon)
{
    dungeon.runActive = false;
    dungeon.entities = nullptr;
    dungeon.entityCount = 0;
    dungeon.loadedRoom = NO_ROOM;
    dungeon.currentRoom = 0;
    dungeon.characterCount = 0;

    for (uint8_t roomIndex = 0; roomIndex < MAX_ROOMS; roomIndex++)
    {
        DungeonRoomRuntime& runtime = dungeon.roomRuntime[roomIndex];
        runtime.entityCount = 0;
        runtime.playerSlot = NO_ENTITY_SLOT;
        runtime.initialized = false;

        for (uint8_t entityIndex = 0;
             entityIndex < MAX_ENTITIES;
             entityIndex++)
        {
            runtime.entities[entityIndex] = Entity{};
        }

        dungeon.rooms[roomIndex].discovered = false;
        dungeon.rooms[roomIndex].completed = false;
    }

    for (uint8_t i = 0; i < MAX_DUNGEON_CHARACTERS; i++)
        dungeon.characters[i] = nullptr;
}

void updateCurrentDungeonRoomCompletion(Dungeon& dungeon)
{
    if (!dungeon.runActive || dungeon.loadedRoom >= MAX_ROOMS)
        return;

    DungeonRoomRuntime& runtime =
        dungeon.roomRuntime[dungeon.loadedRoom];
    runtime.entityCount = dungeon.entityCount;
    updateRoomCompletion(dungeon, dungeon.loadedRoom);
}

bool isDungeonRunComplete(const Dungeon& dungeon)
{
    if (!dungeon.runActive)
        return false;

    for (uint8_t roomIndex = 0; roomIndex < MAX_ROOMS; roomIndex++)
    {
        const DungeonRoom& room = dungeon.rooms[roomIndex];
        const DungeonRoomRuntime& runtime =
            dungeon.roomRuntime[roomIndex];

        if (!room.discovered || !room.completed || !runtime.initialized)
            return false;

        // An unlooted corpse remains an active monster entity. Keep the run
        // resumable until every corpse has been dealt with, so town travel can
        // never discard loot that was legitimately generated before fleeing.
        for (uint8_t i = 0; i < runtime.entityCount; i++)
        {
            const Entity& entity = runtime.entities[i];

            if (entity.active && entity.type == ENTITY_MONSTER &&
                entity.character.state != STATE_LOOTED)
            {
                return false;
            }
        }
    }

    return true;
}

void addCharacterToDungeon(Character* character)
{
    if (dungeon.characterCount >= MAX_DUNGEON_CHARACTERS)
        return;

    dungeon.characters[dungeon.characterCount++] = character;
}

