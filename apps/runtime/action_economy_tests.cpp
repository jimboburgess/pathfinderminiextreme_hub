#include "runtime.h"
#include "controller_slots.h"
#include "data/game.h"
#include "data/entityspawn.h"
#include "dungeon/combat.h"
#include "dungeon/combatpolicy.h"
#include "dungeon/dungeon.h"
#include "input/menu.h"
#include "map/activemap.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace pfme::runtime;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void wait(unsigned milliseconds)
{
    while (milliseconds != 0)
    {
        const unsigned step = std::min(milliseconds, 10u);
        advance(step);
        milliseconds -= step;
    }
}

void tap(const char* action)
{
    check(controllerInput(0, action, true), "input down");
    wait(30);
    check(controllerInput(0, action, false), "input up");
    wait(310);
}

void enterTestDungeon()
{
    tap("a");
    tap("a");
    tap("a");
    tap("a");
    tap("a");
    check(gameState == GAME_TOWN, "town");
    tap("cw");
    tap("cw");
    tap("a");
    if (menuState.isOpen) tap("a");
    check(gameState == GAME_DUNGEON && dungeon.runActive, "dungeon");
}

Direction openDirection(const Entity& actor)
{
    for (Direction direction : {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST})
    {
        const int x = actor.x + directionOffsets[direction].dx;
        const int y = actor.y + directionOffsets[direction].dy;
        if (x >= 0 && y >= 0 && x < ROOM_WIDTH && y < ROOM_HEIGHT &&
            dungeon.rooms[dungeon.currentRoom].map.tiles[y][x] == TILE_FLOOR &&
            getEntityAt(dungeon.entities, dungeon.entityCount, x, y) == nullptr)
            return direction;
    }
    throw std::runtime_error("open movement tile");
}
}

int main()
{
    try
    {
        initializeControllers(2);
        ControllerState state;
        check(bindController(0, state), "bind controller");
        enterTestDungeon();

        Entity* actor = getActiveMapPlayer();
        check(actor != nullptr, "active actor");
        combat = Combat{};
        combat.active = true;
        combat.phase = COMBAT_TURN;
        combat.waitingForPlayer = true;
        combat.combatantCount = 1;
        combat.currentTurnIndex = 0;
        combat.initiativeOrder[0] = actor;
        actor->turn = TurnState{};
        actor->turn.turnActive = true;
        actor->turn.movementRemaining = getEffectiveSpeed(actor->character);

        check(!isMenuItemVisible(MENU_DOUBLE_MOVE),
              "Double Move hidden before movement");
        check(isMenuItemVisible(MENU_ATTACK), "attack visible on fresh turn");

        const Direction direction = openDirection(*actor);
        while (moveDirection != direction) tap("cw");
        const int oldX = actor->x;
        const int oldY = actor->y;
        tap("encoder");
        check(actor->x != oldX || actor->y != oldY, "production movement");
        check(actor->turn.moveActionUsed, "first step commits move action");
        check(isMenuItemVisible(MENU_DOUBLE_MOVE),
              "Double Move visible after movement");
        check(isMenuItemVisible(MENU_ATTACK),
              "attack remains visible after movement");
        check(!canMakeFullAttack(*actor),
              "partial movement prevents a full attack");

        beginDoubleMove();
        const uint8_t effectiveSpeed = getEffectiveSpeed(actor->character);
        check(actor->turn.standardActionUsed,
              "Double Move consumes standard action");
        check(actor->turn.movementRemaining == effectiveSpeed,
              "Double Move grants one fresh speed allowance");
        check(!isMenuItemVisible(MENU_DOUBLE_MOVE),
              "Double Move hidden after use");
        check(!isMenuItemVisible(MENU_ATTACK),
              "attack hidden after Double Move");

        const uint8_t remaining = actor->turn.movementRemaining;
        beginDoubleMove();
        check(actor->turn.movementRemaining == remaining,
              "stale Double Move is rejected without mutation");

        actor->turn = TurnState{};
        actor->turn.turnActive = true;
        actor->turn.standardActionUsed = true;
        actor->turn.movementRemaining = effectiveSpeed;
        check(!isMenuItemVisible(MENU_DOUBLE_MOVE),
              "Double Move hidden after attack");
        check(!isMenuItemVisible(MENU_ATTACK),
              "second standard attack hidden");

        std::cout << "Production action economy tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
