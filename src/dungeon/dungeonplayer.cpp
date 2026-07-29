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
#include "turns.h"
#include "data/entityspawn.h"
#include "graphics/display.h"

extern Adafruit_ST7789 tft;



void drawMoveCursor(const Dungeon &dungeon)
{
    int x;
    int y;

    if (gameState == GAME_FOREST)
    {
        Entity* player = getPlayerEntity(
            forestEntities,
            forestEntityCount);

        if (player == nullptr)
            return;

        x = player->x + directionOffsets[moveDirection].dx;
        y = player->y + directionOffsets[moveDirection].dy;
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

    int oldX = player->x;
    int oldY = player->y;
    Direction oldDirection = moveDirection;

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

            if (combat.active)
            {
                if (!combat.waitingForPlayer)
                    return false;

                if (player->turn.movementRemaining == 0)
                    return false;
            }

            player->x = targetX;
            player->y = targetY;

            markTileDirty(oldX, oldY);
            markTileDirty(player->x, player->y);

            markTileDirty(
                oldX + directionOffsets[oldDirection].dx,
                oldY + directionOffsets[oldDirection].dy);

            markTileDirty(
                player->x + directionOffsets[moveDirection].dx,
                player->y + directionOffsets[moveDirection].dy);

            if (combat.active)
            {
                if (player->turn.movementRemaining > 0)
                {
                    player->turn.movementRemaining--;
                }
            }

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

                redrawType = REDRAW_FULL;

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
    Entity* player = nullptr;

    if (gameState == GAME_FOREST)
    {
        player = getPlayerEntity(
            forestEntities,
            forestEntityCount);
    }
    else if (gameState == GAME_DUNGEON)
    {
        player = getPlayerEntity(
            dungeon.entities,
            dungeon.entityCount);
    }

    if (player == nullptr)
        return false;

    //--------------------------------------------------
    // Combat movement restrictions.
    //--------------------------------------------------

    if (combat.active)
    {
        if (!combat.waitingForPlayer)
            return false;

        if (player->turn.movementRemaining == 0)
            return false;
    }

    //--------------------------------------------------
    // Redraw facing tiles.
    //--------------------------------------------------

    markTileDirty(
        player->x + directionOffsets[previousMoveDirection].dx,
        player->y + directionOffsets[previousMoveDirection].dy);

    markTileDirty(
        player->x + directionOffsets[moveDirection].dx,
        player->y + directionOffsets[moveDirection].dy);

    int oldX = player->x;
    int oldY = player->y;
    Direction oldDirection = moveDirection;

    int targetX =
        player->x + directionOffsets[moveDirection].dx;

    int targetY =
        player->y + directionOffsets[moveDirection].dy;

    //--------------------------------------------------
    // Stay inside the map.
    //--------------------------------------------------

    if (targetX < 0 || targetX >= FOREST_WIDTH ||
        targetY < 0 || targetY >= FOREST_HEIGHT)
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    //--------------------------------------------------
    // Collision check.
    //--------------------------------------------------

    if (!canPlayerMoveTo(targetX, targetY))
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    //--------------------------------------------------
    // Move player.
    //--------------------------------------------------

    player->x = targetX;
    player->y = targetY;

    markTileDirty(oldX, oldY);
    markTileDirty(player->x, player->y);

    markTileDirty(
        oldX + directionOffsets[oldDirection].dx,
        oldY + directionOffsets[oldDirection].dy);

    markTileDirty(
        player->x + directionOffsets[moveDirection].dx,
        player->y + directionOffsets[moveDirection].dy);

    Serial.print("Player moved to: ");
    Serial.print(player->x);
    Serial.print(", ");
    Serial.println(player->y);

    //--------------------------------------------------
    // Enter combat if enemies are nearby.
    //--------------------------------------------------

    checkForCombat();

    //--------------------------------------------------
    // Consume movement during combat.
    //--------------------------------------------------

    if (combat.active)
    {
        if (player->turn.movementRemaining > 0)
        {
            player->turn.movementRemaining--;
        }
    }

    return true;
}

bool canPlayerMoveTo(int x, int y)
{
    //--------------------------------------------------
    // Stay inside the map.
    //--------------------------------------------------

    if (x < 0 || x >= FOREST_WIDTH ||
        y < 0 || y >= FOREST_HEIGHT)
    {
        return false;
    }

    //--------------------------------------------------
    // Trees block movement.
    //--------------------------------------------------

    if (getForestTile(x, y) == TILE_TREE)
    {
        return false;
    }

    //--------------------------------------------------
    // Monsters block movement.
    //--------------------------------------------------

    Entity* entity = getEntityAt(
    forestEntities,
    forestEntityCount,
    x,
    y);

    if (entity != nullptr &&
        entity->type == ENTITY_MONSTER)
    {
        return false;
    }

    return true;
}