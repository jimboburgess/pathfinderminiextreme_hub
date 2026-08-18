//
// Created by james on 7/12/2026.
//

#include "map/playermovement.h"
#include "dungeon/dungeon.h"
#include "audio/audio.h"
#include "data/game.h"
#include <Adafruit_ST7789.h>
#include "config.h"
#include <Arduino.h>

#include "dungeon/combat.h"
#include "../audio/audio.h"
#include "forest/forest.h"
#include "map/movement.h"
#include "dungeon/turns.h"
#include "data/dice.h"
#include "data/entityspawn.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"

extern Adafruit_ST7789 tft;

static bool tryMoveForestPlayer();

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

bool canPlayerTraverseEnemy(
    const Dungeon& dungeon,
    const Entity& player,
    int enemyX,
    int enemyY,
    int moveX,
    int moveY,
    int& beyondX,
    int& beyondY)
{
    // This is a two-square combat movement: into the occupied square and
    // immediately out the far side. Validate the landing square before any
    // Acrobatics roll so blocked destinations behave like normal movement.
    // A two-tile traversal at half speed costs four normal movement points.
    if (!combat.active || player.turn.movementRemaining < 4)
        return false;

    beyondX = enemyX + moveX;
    beyondY = enemyY + moveY;

    if (beyondX < 0 || beyondX >= ROOM_SIZE ||
        beyondY < 0 || beyondY >= ROOM_SIZE ||
        dungeon.rooms[dungeon.currentRoom].map.tiles[beyondY][beyondX] !=
            TILE_FLOOR)
    {
        return false;
    }

    Entity* occupant = getEntityAt(
        dungeon.entities, dungeon.entityCount, beyondX, beyondY);
    return occupant == nullptr || occupant == &player;
}

int getEnemyCombatManeuverDefense(const Entity& enemy)
{
    const int baseAttack = enemy.monster != nullptr
        ? enemy.monster->baseAttack
        : 0;

    return 10 + baseAttack +
        getAbilityModifier(enemy.character, ABILITY_STRENGTH) +
        getAbilityModifier(enemy.character, ABILITY_DEXTERITY);
}

bool tryPlayerAcrobaticsTraversal(
    Dungeon& dungeon,
    Entity& player,
    Entity& enemy,
    int oldX,
    int oldY,
    Direction oldDirection,
    int moveX,
    int moveY)
{
    int beyondX = 0;
    int beyondY = 0;

    if (!canPlayerTraverseEnemy(
            dungeon, player, enemy.x, enemy.y, moveX, moveY, beyondX, beyondY))
    {
        playSound(SoundEffect::BUMP);
        return false;
    }

    const int acrobaticsDC = getEnemyCombatManeuverDefense(enemy) + 5;
    const int acrobaticsRoll = rollDice(1, 20) +
        getSkillBonus(player.character, SKILL_ACROBATICS);

    if (acrobaticsRoll < acrobaticsDC)
    {
        player.turn.movementRemaining = 0;
        player.turn.moveActionUsed = true;
        checkEndPlayerTurn();

        // Use the normal monster melee resolver for the AoO. The player never
        // occupies the enemy square, so this remains a normal adjacent attack.
        beginMonsterAttack(&enemy, &player, COMBAT_ATTACK_MELEE);
        setGameMessage("Acrobatics check failed!");
        playSound(SoundEffect::ERROR);
        return false;
    }

    // A successful Acrobatics traversal avoids an attack of opportunity. The
    // project has no AoO system yet, so no combat reaction is triggered here.
    player.x = beyondX;
    player.y = beyondY;
    player.turn.movementRemaining -= 4;
    handleEnteredTile(player, beyondX, beyondY);

    if (player.turn.movementRemaining == 0)
    {
        player.turn.moveActionUsed = true;
        checkEndPlayerTurn();
    }

    markTileDirty(oldX, oldY);
    markTileDirty(enemy.x, enemy.y);
    markTileDirty(player.x, player.y);
    markTileDirty(oldX + directionOffsets[oldDirection].dx,
                  oldY + directionOffsets[oldDirection].dy);
    markTileDirty(player.x + directionOffsets[moveDirection].dx,
                  player.y + directionOffsets[moveDirection].dy);
    setGameMessage("Acrobatics successful!");
    checkForCombat();
    return true;
}
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
    if (gameState == GAME_FOREST)
        return tryMoveForestPlayer();

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
        targetEntity->type == ENTITY_MONSTER &&
        targetEntity->character.state == STATE_ALIVE)
    {
        if (!targetEntity->revealedToPlayer)
        {
            targetEntity->revealedToPlayer = true;
            targetEntity->awareOfPlayer = true;
            markEntityFootprintDirty(*targetEntity);
            startCombat();
            return false;
        }
        return tryPlayerAcrobaticsTraversal(
            dungeon,
            *player,
            *targetEntity,
            oldX,
            oldY,
            oldDirection,
            directionOffsets[moveDirection].dx,
            directionOffsets[moveDirection].dy);
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

static bool tryMoveForestPlayer()
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
