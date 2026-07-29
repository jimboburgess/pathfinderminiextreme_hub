#include "combat.h"
#include "dungeon.h"
#include "forest.h"
#include "data/game.h"
#include "audio/audio.h"
#include "data/dice.h"
#include "data/entityspawn.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"

Combat combat;


bool blocksSight(TileType tile)
{
    switch (tile)
    {
        case TILE_TREE:
            return true;

        default:
            return false;
    }
}

bool hasLineOfSight(int x1, int y1, int x2, int y2)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;

    int err = dx - dy;

    while (true)
    {
        // Skip the starting tile and ending tile.
        if (!(x1 == x2 && y1 == y2))
        {
            if (!(x1 == getForestPlayerX() &&
                  y1 == getForestPlayerY()))
            {
                if (blocksSight(getForestTile(x1, y1)))
                {
                    return false;
                }
            }
        }

        if (x1 == x2 && y1 == y2)
            break;

        int e2 = err * 2;

        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }

    return true;
}

void findCombatants()
{
    combat.combatantCount = 0;

    Entity* playerEntity = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    //--------------------------------------------------
    // Player always joins combat.
    //--------------------------------------------------

    combat.turnOrder[combat.combatantCount++] = playerEntity;

    //--------------------------------------------------
    // Find nearby monsters.
    //--------------------------------------------------

    for (uint8_t i = 0; i < forestEntityCount; i++)
    {
        Entity& entity = forestEntities[i];

        if (!entity.active)
            continue;

        if (entity.type != ENTITY_MONSTER)
            continue;

        if (entity.character.state != STATE_ALIVE)
            continue;

        int dx = abs(entity.x - playerEntity->x);
        int dy = abs(entity.y - playerEntity->y);

        int distance = std::max(dx, dy);

        if (distance > COMBAT_DETECTION_RANGE)
            continue;

        if (!hasLineOfSight(
                playerEntity->x,
                playerEntity->y,
                entity.x,
                entity.y))
        {
            continue;
        }

        combat.turnOrder[combat.combatantCount++] = &entity;
    }

    //--------------------------------------------------
    // Debug
    //--------------------------------------------------

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.turnOrder[i];

        if (entity->type == ENTITY_PLAYER)
            Serial.println("Player");
        else
            Serial.println("Monster");
    }

    Serial.println();
}

void checkForCombat()
{
    Serial.println("checkForCombat()");
    if (combat.active)
        return;

    Entity* playerEntity = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    if (playerEntity == nullptr)
        return;

    for (uint8_t i = 0; i < forestEntityCount; i++)
    {
        Entity& monster = forestEntities[i];

        if (monster.type != ENTITY_MONSTER)
            continue;

        int dx = abs(monster.x - playerEntity->x);
        int dy = abs(monster.y - playerEntity->y);

        Serial.print("Monster at ");
        Serial.print(monster.x);
        Serial.print(", ");
        Serial.print(monster.y);
        Serial.print("  Distance = ");
        Serial.println(std::max(dx, dy));

        int distance = std::max(dx, dy);

        if (distance > COMBAT_DETECTION_RANGE)
            continue;

        if (!hasLineOfSight(
                playerEntity->x,
                playerEntity->y,
                monster.x,
                monster.y))
        {
            continue;
        }

        setGameMessage("You hear something nearby.");

        startCombat();

        return;
    }
}

void rollInitiative()
{
    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.turnOrder[i];

        entity->character.initiative =
            rollDie(20) +
            getAbilityModifier(entity->character.abilities.dexterity);
    }
}

void sortInitiative()
{
    for (uint8_t i = 0; i < combat.combatantCount - 1; i++)
    {
        for (uint8_t j = i + 1; j < combat.combatantCount; j++)
        {
            if (combat.turnOrder[j]->character.initiative >
                combat.turnOrder[i]->character.initiative)
            {
                Entity* temp = combat.turnOrder[i];
                combat.turnOrder[i] = combat.turnOrder[j];
                combat.turnOrder[j] = temp;
            }
        }
    }
}

Entity* getCurrentCombatant()
{
    if (!combat.active)
        return nullptr;

    if (combat.currentTurnIndex >= combat.combatantCount)
        return nullptr;

    return combat.turnOrder[combat.currentTurnIndex];
}

bool isPlayerTurn()
{
    Entity* current = getCurrentCombatant();

    if (current == nullptr)
        return false;

    return (current->type == ENTITY_PLAYER);
}

void startCombat()
{
    combat.active = true;
    findCombatants();
    rollInitiative();
    sortInitiative();
    combat.phase = COMBAT_INITIATIVE;
    combat.phaseStartTime = millis();
    combat.initiativeMessageShown = false;
    combat.currentTurnIndex = 0;

    combat.combatRound = 1;
    runCombatTurn(getCurrentCombatant());

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}


void runCombatTurn(Entity* entity)
{
    if (entity == nullptr)
        return;

    announceTurn(entity);

    if (entity->type == ENTITY_PLAYER)
    {
        combat.waitingForPlayer = true;

        runPlayerTurn(entity);
    }
    else
    {
        combat.waitingForPlayer = false;

        runMonsterTurn(entity);
    }
}

void runMonsterTurn(Entity* monster)
{
    if (monster == nullptr)
        return;

    runMonsterAI(monster);
}

void runMonsterAI(Entity* monster)
{
    performMovementPhase(monster);

    nextTurn();
}

void announceTurn(Entity* entity)
{
    entity->turn.movementRemaining =
    entity->character.speed;

    entity->turn.standardActionUsed = false;

    Serial.println("----- Start Turn -----");

    Serial.print("Type: ");
    Serial.println(
        entity->type == ENTITY_PLAYER ?
        "Player" : "Monster");

    Serial.print("Speed: ");
    Serial.println(entity->character.speed);
}

void runPlayerTurn(Entity* player)
{
    setGameMessage("Player Turn");

    needsRedraw = true;

    // Nothing else.
    // Wait for button input.
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
}
void performMovementPhase(Entity* entity)
{
    while (entity->turn.movementRemaining > 0)
    {
        int oldX = entity->x;
        int oldY = entity->y;

        moveMonsterTowardsPlayer(entity);

        entity->turn.movementRemaining--;

        //--------------------------------------------------
        // Stop if we couldn't move.
        //--------------------------------------------------

        if (entity->x == oldX &&
            entity->y == oldY)
        {
            break;
        }
    }
}

void nextTurn()
{
    combat.currentTurnIndex++;

    if (combat.currentTurnIndex >= combat.combatantCount)
    {
        combat.currentTurnIndex = 0;
        combat.combatRound++;
    }
    if (isPlayerTurn())
        setGameMessage("Player Turn");
    else
        setGameMessage("Monster Turn");
    needsRedraw = true;
}

void endPlayerTurn()
{
    combat.waitingForPlayer = false;
    playSound(SoundEffect::DEFEND);

    nextTurn();
}

void updateCombat()
{
    switch (combat.phase)
    {
        case COMBAT_NONE:
            break;

        case COMBAT_INITIATIVE:

            if (!combat.initiativeMessageShown &&
                millis() - combat.phaseStartTime >= 2000)
            {
                setGameMessage("Roll for Initiative!");

                combat.initiativeMessageShown = true;

                playSound(SoundEffect::MENU_SELECT);

                combat.phase = COMBAT_TURN;

                runCombatTurn(getCurrentCombatant());

                if (isPlayerTurn())
                    setGameMessage("Player Turn");
                else
                    setGameMessage("Monster Turn");
            }

            break;

        case COMBAT_TURN:
            break;

        case COMBAT_END:
            endCombat();
            break;
    }
}

void endCombat()
{
    combat.active = false;
    combat.phase = COMBAT_NONE;
}

bool isCombatActive()
{
    return combat.active;
}