//
// Created by james on 7/12/2026.
//

#include "dungeon.h"
#include <Arduino.h>
#include "roomgen.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "graphics/messagelog.h"

Dungeon dungeon;

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
    generateDungeon(dungeon);

    gameState = GAME_DUNGEON;
    setGameMessage("Entered dungeon");

    Entity* player = getPlayerEntity(
        dungeon.entities,
        dungeon.entityCount);

    if (player)
    {
        previousPlayerPosition.x = player->x;
        previousPlayerPosition.y = player->y;
    }

    previousMoveDirection = moveDirection;

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

void generateDungeon(Dungeon& dungeon)
{
    dungeon.currentRoom = 0;
    dungeon.characterCount = 0;

    // Clear room connections
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        dungeon.rooms[i].north = NO_ROOM;
        dungeon.rooms[i].south = NO_ROOM;
        dungeon.rooms[i].east  = NO_ROOM;
        dungeon.rooms[i].west  = NO_ROOM;

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
        dungeon.rooms[i].shape = (RoomShape)random(3);
        generateRoom(dungeon.rooms[i]);
    }

    // Load the starting room
    loadRoom(dungeon, ENTRY_START);
}

void loadRoom(Dungeon& dungeon, RoomEntry entry)
{
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];

    // Remove any entities from the previous room.
    dungeon.entityCount = 0;

    // Scan the room and create entities.
    for (int y = 0; y < ROOM_SIZE; y++)
    {
        for (int x = 0; x < ROOM_SIZE; x++)
        {
            switch (room.map.tiles[y][x])
            {
                case TILE_ENEMY_START:
                {
                    spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_MONSTER,
                        x,
                        y);

                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;
                }

                case TILE_CHEST_SPAWN:
                {
                    spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_CHEST,
                        x,
                        y);

                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;
                }

                case TILE_LOOT_SPAWN:
                {
                    spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_LOOT,
                        x,
                        y);

                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;
                }

                case TILE_NPC_SPAWN:
                {
                    spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_NPC,
                        x,
                        y);

                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;
                }

                default:
                    break;
            }
        }
    }

    // Create the player.
    Entity* player = spawnEntity(
    dungeon.entities,
    dungeon.entityCount,
    ENTITY_PLAYER,0,0);

    if (player == nullptr)
        return;

    switch (entry)
    {
        case ENTRY_START:
            player->x = ROOM_SIZE / 2;
            player->y = ROOM_SIZE / 2;
            break;

        case ENTRY_NORTH:
            player->x = ROOM_SIZE / 2;
            player->y = 1;
            break;

        case ENTRY_EAST:
            player->x = ROOM_SIZE - 2;
            player->y = ROOM_SIZE / 2;
            break;

        case ENTRY_SOUTH:
            player->x = ROOM_SIZE / 2;
            player->y = ROOM_SIZE - 2;
            break;

        case ENTRY_WEST:
            player->x = 1;
            player->y = ROOM_SIZE / 2;
            break;
    }
}

void addCharacterToDungeon(Character* character)
{
    if (dungeon.characterCount >= MAX_DUNGEON_CHARACTERS)
        return;

    dungeon.characters[dungeon.characterCount++] = character;
}

