#include "runtime.h"
#include "controller_slots.h"
#include "data/game.h"
#include "dungeon/dungeon.h"
#include "dungeon/puzzleinstructions.h"
#include "graphics/messagelog.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

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
    check(gameState == GAME_CHARACTER_CREATION, "character creation");
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

void configurePuzzle(DungeonRoom& room, DungeonPuzzleType puzzleType)
{
    room.type = ROOM_PUZZLE;
    room.puzzleType = puzzleType;
    room.completed = false;
    room.bellPuzzle.progress = puzzleType == PUZZLE_BELLS
        ? BELL_PUZZLE_UNSOLVED : BELL_PUZZLE_NONE;
    room.numberPuzzle.progress = puzzleType == PUZZLE_NUMBER_TILES
        ? NUMBER_PUZZLE_UNSOLVED : NUMBER_PUZZLE_NONE;
    room.brazierPuzzle.progress = puzzleType == PUZZLE_BRAZIERS
        ? BRAZIER_PUZZLE_UNSOLVED : BRAZIER_PUZZLE_NONE;
    room.npcSpawn.puzzleState = puzzleType == PUZZLE_RIDDLEMAN
        ? RIDDLE_ROOM_UNSOLVED : RIDDLE_ROOM_NONE;
}

bool messageStartsWith(const char* expected)
{
    return std::string(getGameMessage()).find(expected) == 0;
}

void enterRoom(uint8_t roomIndex)
{
    dungeon.currentRoom = roomIndex;
    check(loadRoom(dungeon, ENTRY_START), "room entry");
    wait(20);
}
}

int main(int argc, char** argv)
{
    try
    {
        check(argc == 2, "puzzle argument");
        DungeonPuzzleType puzzleType = PUZZLE_NONE;
        const char* visiblePrefix = nullptr;
        const std::string argument = argv[1];
        if (argument == "bell")
        {
            puzzleType = PUZZLE_BELLS;
            visiblePrefix = "Match the tones.";
        }
        else if (argument == "number")
        {
            puzzleType = PUZZLE_NUMBER_TILES;
            visiblePrefix = "Follow the rule.";
        }
        else if (argument == "brazier")
        {
            puzzleType = PUZZLE_BRAZIERS;
            visiblePrefix = "Light all the braziers";
        }
        else if (argument == "riddleman")
        {
            puzzleType = PUZZLE_RIDDLEMAN;
        }
        else
        {
            throw std::runtime_error("unknown puzzle");
        }

        initializeControllers(2);
        ControllerState state;
        check(bindController(0, state), "bind controller");
        enterTestDungeon();
        check(dungeon.roomCount > 1, "destination room");
        configurePuzzle(dungeon.rooms[1], puzzleType);

        setGameMessage("entry sentinel");
        enterRoom(1);
        if (puzzleType == PUZZLE_RIDDLEMAN)
        {
            check(std::string(getGameMessage()) == "entry sentinel",
                  "Bertram entry preserves presentation");
        }
        else
        {
            check(messageStartsWith(visiblePrefix), "entry instruction visible");
            for (uint8_t frame = 0; frame < 5; ++frame) wait(100);
            check(messageStartsWith(visiblePrefix), "normal frames do not replace instruction");

            enterRoom(0);
            setGameMessage("reentry sentinel");
            enterRoom(1);
            check(messageStartsWith(visiblePrefix), "unsolved reentry repeats instruction");

            if (puzzleType == PUZZLE_BELLS)
                dungeon.rooms[1].bellPuzzle.progress = BELL_PUZZLE_COMPLETE;
            else if (puzzleType == PUZZLE_NUMBER_TILES)
                dungeon.rooms[1].numberPuzzle.progress = NUMBER_PUZZLE_COMPLETE;
            else
                dungeon.rooms[1].brazierPuzzle.progress = BRAZIER_PUZZLE_COMPLETE;
            enterRoom(0);
            setGameMessage("solved sentinel");
            enterRoom(1);
            check(std::string(getGameMessage()) == "solved sentinel",
                  "solved reentry suppresses instruction");
        }

        std::cout << "Puzzle entry runtime check passed: " << argument << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << "; " << diagnostic() << '\n';
        return 1;
    }
}
