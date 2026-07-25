//
// Created by james on 7/12/2026.
//

#include "dungeonplayer.h"
#include <Adafruit_ST7789.h>
#include "config.h"
#include <Arduino.h>

#include "combat.h"
#include "../audio/audio.h"
#include "forest.h"

extern Adafruit_ST7789 tft;



void drawMoveCursor(const Dungeon &dungeon)
{
    int x;
    int y;

    if (gameState == GAME_FOREST)
    {
        x = playerPosition.x + directionOffsets[moveDirection].dx;
        y = playerPosition.y + directionOffsets[moveDirection].dy;
    }
    else
    {
        const Entity* player = getPlayerEntity(
        dungeon.entities,
        dungeon.entityCount);

        if (player == nullptr)
            return;

        x = player->x + directionOffsets[moveDirection].dx;
        y = player->y + directionOffsets[moveDirection].dy;
    }

    tft.drawRect(
        x * TILE_SIZE,
        y * TILE_SIZE,
        TILE_SIZE,
        TILE_SIZE,
        ST77XX_WHITE);
}
bool tryMovePlayer(Dungeon &dungeon)
{
    Entity* player = getPlayerEntity(
    dungeon.entities,
    dungeon.entityCount);

    if (player == nullptr)
        return false;

    int targetX =
     player->x + directionOffsets[moveDirection].dx;

    int targetY =
        player->y + directionOffsets[moveDirection].dy;

    DungeonRoom &room = dungeon.rooms[dungeon.currentRoom];

    if (targetX < 0 || targetX >= ROOM_SIZE ||
        targetY < 0 || targetY >= ROOM_SIZE)
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    TileType tile = room.map.tiles[targetY][targetX];

    switch (tile)
    {
        case TILE_FLOOR:

            player->x = targetX;
            player->y = targetY;
            return true;

        case TILE_WALL:

            playSound(SoundEffect::BUMP);
            return false;

        case TILE_DOOR:
        {
            uint8_t nextRoom = 255;

            if (targetY == 0)
                nextRoom = room.north;
            else if (targetY == ROOM_SIZE - 1)
                nextRoom = room.south;
            else if (targetX == 0)
                nextRoom = room.west;
            else if (targetX == ROOM_SIZE - 1)
                nextRoom = room.east;

            if (nextRoom != 255)
            {
                dungeon.currentRoom = nextRoom;

                if (targetY == 0)
                    loadRoom(dungeon, ENTRY_SOUTH);
                else if (targetY == ROOM_SIZE - 1)
                    loadRoom(dungeon, ENTRY_NORTH);
                else if (targetX == 0)
                    loadRoom(dungeon, ENTRY_EAST);
                else if (targetX == ROOM_SIZE - 1)
                    loadRoom(dungeon, ENTRY_WEST);

                return true;
            }

            break;
        }

        default:
            return false;
    }

    return false;
}

bool tryMoveForestPlayer()
{
    if (combat.active)
    {
        if (combat.turn != TURN_PLAYER)
            return false;

        if (combat.movementRemaining == 0)
            return false;
    }

    previousPlayerPosition = playerPosition;
    previousMoveDirection = moveDirection;

    int targetX = playerPosition.x + directionOffsets[moveDirection].dx;
    int targetY = playerPosition.y + directionOffsets[moveDirection].dy;

    if (targetX < 0 || targetX >= FOREST_WIDTH ||
        targetY < 0 || targetY >= FOREST_HEIGHT)
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    TileType tile = getForestTile(targetX, targetY);

    if (tile == TILE_TREE)
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    Entity* entity = getEntityAt(
        forestEntities,
        forestEntityCount,
        targetX,
        targetY);

    if (entity && entity->type == ENTITY_ENEMY)
    {
        startCombat();
        playSound(SoundEffect::GOBLIN_ATTACK);
        return false;
    }

    playerPosition.x = targetX;
    playerPosition.y = targetY;

    if (combat.active && combat.turn == TURN_PLAYER)
    {
        if (combat.movementRemaining > 0)
        {
            combat.movementRemaining--;
        }
    }

    Entity* player = getPlayerEntity(
    forestEntities,
    forestEntityCount);

    if (player)
    {
        player->x = playerPosition.x;
        player->y = playerPosition.y;
    }

    return true;
}