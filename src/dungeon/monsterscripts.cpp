//
// Created by james on 8/1/2026.
//

#include "monsterScripts.h"
#include "monsters.h"
#include "graphics/messagelog.h"
#include "combat.h"
#include "monsters.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "graphics/display.h"

void runMonsterScript(Entity* monster)
{
    switch (monster->monster->script)
    {
        case SCRIPT_MELEE:
            runMeleeScript(monster);
            break;

        case SCRIPT_RANGED:
            runRangedScript(monster);
            break;

        case SCRIPT_COWARD:
            runCowardScript(monster);
            break;

        case SCRIPT_GUARD:
            runGuardScript(monster);
            break;

        default:
            runMeleeScript(monster);
            break;

            // case SCRIPT_WANDER:
            //     runWanderScript(monster);
            //     break;

            // case SCRIPT_SUPPORT:
            //     runSupportScript(monster);
            //     break;

            // case SCRIPT_SPELLCASTER:
            //     runSpellcasterScript(monster);
            //     break;

            // case SCRIPT_DEBUG:
            //     runDebugScript(monster);
            //     break;
    }
}

void runMeleeScript(Entity* monster)
{
    chooseTarget(monster);

    performMovementPhase(monster);

    performStandardAction(monster);
}

void runRangedScript(Entity* monster)
{
    runMeleeScript(monster);
}

void runCowardScript(Entity* monster)
{
    runMeleeScript(monster);
}

void runGuardScript(Entity* monster)
{
    runMeleeScript(monster);
}

Entity* chooseTarget(Entity* monster)
{
    return getPlayerEntity(
        forestEntities,
        forestEntityCount);
}

void performStandardAction(Entity* monster)
{
    Entity* target = chooseTarget(monster);

    if (target == nullptr)
        return;

    if (isAdjacent(monster, target))
    {
        beginMonsterAttack(monster, target);
    }
}


bool isAdjacent(const Entity* a, const Entity* b)
{
    int dx = abs(a->x - b->x);
    int dy = abs(a->y - b->y);

    return (dx <= 1 &&
            dy <= 1 &&
            !(dx == 0 && dy == 0));
}

bool canMonsterMoveTo(Entity* monster, int x, int y)
{
    //--------------------------------------------------
    // Stay inside the forest.
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
    // Don't move onto another entity.
    //--------------------------------------------------

    Entity* entity = getEntityAt(
        forestEntities,
        forestEntityCount,
        x,
        y);

    if (entity != nullptr)
    {
        return false;
    }

    return true;
}

void moveMonsterTowardsPlayer(Entity* monster)
{
    Serial.print("Moving from ");
    Serial.print(monster->x);
    Serial.print(",");
    Serial.print(monster->y);
    Serial.print(" -> ");
    Entity* player = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    if (player == nullptr)
        return;


    //--------------------------------------------------
    // Already next to the player?
    //--------------------------------------------------

    if (isAdjacent(monster, player))
    {
        setGameMessage("Goblin attacks!");
        needsRedraw = true;
        return;
    }

    int oldX = monster->x;
    int oldY = monster->y;

    char message[32];

    snprintf(
        message,
        sizeof(message),
        "%s advances.",
        getEntityName(monster));

    setGameMessage(message);

    int newX = monster->x;
    int newY = monster->y;

    int dx = player->x - monster->x;
    int dy = player->y - monster->y;

    //--------------------------------------------------
    // Move one tile toward the player.
    //--------------------------------------------------

    if (dx != 0)
        newX += (dx > 0) ? 1 : -1;

    if (dy != 0)
        newY += (dy > 0) ? 1 : -1;

    //--------------------------------------------------
    // Only move if the destination is valid.
    //--------------------------------------------------

    if (canMonsterMoveTo(monster, newX, newY))
    {
        monster->x = newX;
        monster->y = newY;

        markTileDirty(oldX, oldY);
        markTileDirty(monster->x, monster->y);
    }
    Serial.print(monster->x);
    Serial.print(",");
    Serial.println(monster->y);
}
//add this once you need monsters to move around trees
//bool moved = moveMonsterTowardsPlayer(entity);

void performMovementPhase(Entity* entity)
{
    if (entity->turn.movementRemaining == 0)
        return;

    moveMonsterTowardsPlayer(entity);

    entity->turn.movementRemaining--;
}
