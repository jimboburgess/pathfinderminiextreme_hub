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
#include "movement.h"
#include "turns.h"
#include "data/entityspawn.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"

extern Adafruit_ST7789 tft;

namespace
{
bool handlePlayerStandAttempt(Entity& player, bool& handled)
{
    StandForMovementResult result = tryStandForMovement(
        player, combat.active);
    handled = result != STAND_NOT_PRONE;

    if (result == STAND_NOT_PRONE)
        return true;

    if (result == STAND_NO_MOVEMENT)
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    setGameMessage("You stand up.");
    markEntityFootprintDirty(player);

    if (combat.active && player.turn.movementRemaining == 0)
    {
        player.turn.moveActionUsed = true;
        checkEndPlayerTurn();
    }

    return true;
}

void finishPlayerMovement(Entity& player, int targetX, int targetY)
{
    ConditionType enteredCondition = handleEnteredTile(
        player, targetX, targetY);

    if (combat.active)
    {
        spendMovementCost(player, targetX, targetY);

        if (player.turn.movementRemaining == 0)
        {
            player.turn.moveActionUsed = true;
            checkEndPlayerTurn();
        }
    }

    if (enteredCondition == CONDITION_PRONE)
        setGameMessage("You fall prone!");
}
}



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

//--------------------------------------------------
// TODO:
// tryMovePlayer() and tryMoveForestPlayer() now
// share most of their logic. Once combat movement
// is finalized, move the shared code into common
// helper functions to avoid duplication.
//--------------------------------------------------

bool tryMovePlayer(Dungeon &dungeon)
{
    //--------------------------------------------------
    // Find the player.
    //--------------------------------------------------

    Entity* player = getPlayerEntity(
        dungeon.entities,
        dungeon.entityCount);

    if (player == nullptr)
        return false;

    //--------------------------------------------------
    // Save current position.
    //--------------------------------------------------

    int oldX = player->x;
    int oldY = player->y;
    Direction oldDirection = moveDirection;

    int targetX =
        player->x + directionOffsets[moveDirection].dx;

    int targetY =
        player->y + directionOffsets[moveDirection].dy;

    if (combat.active &&
        (!combat.waitingForPlayer ||
         !canCharacterAct(player->character)))
    {
        return false;
    }

    bool standHandled = false;
    if (!handlePlayerStandAttempt(*player, standHandled))
        return false;

    if (standHandled)
        return true;

    DungeonRoom& room =
        dungeon.rooms[dungeon.currentRoom];

    //--------------------------------------------------
    // Stay inside the room.
    //--------------------------------------------------

    if (targetX < 0 || targetX >= ROOM_SIZE ||
        targetY < 0 || targetY >= ROOM_SIZE)
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    TileType tile =
        room.map.tiles[targetY][targetX];

    // Forest movement blocks an occupied monster square before the player
    // moves. Do the same in rooms, including every square of a large
    // creature's footprint.
    Entity* targetEntity = getEntityAt(
        dungeon.entities,
        dungeon.entityCount,
        targetX,
        targetY);

    if (targetEntity != nullptr && targetEntity != player &&
        targetEntity->type == ENTITY_MONSTER)
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    switch (tile)
    {
        //--------------------------------------------------
        // Floor
        //--------------------------------------------------

        case TILE_FLOOR:
        {
            //--------------------------------------------------
            // Combat movement restrictions.
            //--------------------------------------------------

            if (combat.active)
            {
                if (!combat.waitingForPlayer ||
                    !canCharacterAct(player->character))
                    return false;

                if (player->turn.movementRemaining == 0) {
                    playSound(SoundEffect::BUMP);
                    return false;
                }
            }

            if (combat.active &&
                !canAffordMovementCost(*player, targetX, targetY))
            {
                playSound(SoundEffect::BUMP);
                return false;
            }

            //--------------------------------------------------
            // Move the player.
            //--------------------------------------------------

            player->x = targetX;
            player->y = targetY;

            //--------------------------------------------------
            // Consume one square of movement.
            //--------------------------------------------------

            finishPlayerMovement(*player, targetX, targetY);

            //--------------------------------------------------
            // Redraw affected tiles.
            //--------------------------------------------------

            markTileDirty(oldX, oldY);
            markTileDirty(player->x, player->y);

            markTileDirty(
                oldX + directionOffsets[oldDirection].dx,
                oldY + directionOffsets[oldDirection].dy);

            markTileDirty(
                player->x + directionOffsets[moveDirection].dx,
                player->y + directionOffsets[moveDirection].dy);

            // This detector reads the active map, so the dungeon now uses
            // the same range and line-of-sight rules as the forest.
            checkForCombat();

            return true;
        }

        //--------------------------------------------------
        // Wall
        //--------------------------------------------------

        case TILE_WALL:

            playSound(SoundEffect::BUMP);
            return false;

        //--------------------------------------------------
        // Door
        //--------------------------------------------------

        case TILE_DOOR:
        {
            // Leaving the room while combat owns the current entity list
            // would discard its combatants. Forest exploration has no room
            // transition escape route, so keep dungeon combat locked to the
            // active map until it ends.
            if (combat.active)
            {
                playSound(SoundEffect::BUMP);
                return false;
            }

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
                // The player Character remains global while loadRoom() swaps
                // the active view to the destination room's persistent
                // occupants. Preserve all runtime character changes first.
                ::player = player->character;

                dungeon.currentRoom = nextRoom;

                if (targetY == 0)
                    loadRoom(dungeon, ENTRY_SOUTH);
                else if (targetY == ROOM_SIZE - 1)
                    loadRoom(dungeon, ENTRY_NORTH);
                else if (targetX == 0)
                    loadRoom(dungeon, ENTRY_EAST);
                else if (targetX == ROOM_SIZE - 1)
                    loadRoom(dungeon, ENTRY_WEST);

                // Repaint the destination map, otherwise the old player's
                // transparent sprite can remain visible beneath the room.
                backgroundNeedsRedraw = true;
                redrawType = REDRAW_FULL;
                needsRedraw = true;

                return true;
            }

            break;
        }

        //--------------------------------------------------
        // Unknown tile.
        //--------------------------------------------------

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

    if (combat.active &&
        (!combat.waitingForPlayer ||
         !canCharacterAct(player->character)))
    {
        return false;
    }

    bool standHandled = false;
    if (!handlePlayerStandAttempt(*player, standHandled))
        return false;

    if (standHandled)
        return true;

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
    // Combat movement restrictions.
    //--------------------------------------------------

    if (combat.active)
    {
        if (!combat.waitingForPlayer ||
            !canCharacterAct(player->character))
            return false;

        if (player->turn.movementRemaining == 0) {
            playSound(SoundEffect::BUMP);
            return false;
        }
    }

    if (combat.active &&
        !canAffordMovementCost(*player, targetX, targetY))
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    //--------------------------------------------------
    // Move player.
    //--------------------------------------------------

    player->x = targetX;
    player->y = targetY;

    //--------------------------------------------------
    // Consume one square of movement.
    //--------------------------------------------------

    finishPlayerMovement(*player, targetX, targetY);

    //--------------------------------------------------
    // Redraw tiles.
    //--------------------------------------------------

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
