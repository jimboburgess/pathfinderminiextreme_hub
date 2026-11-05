#include <cstdlib>
#include <iostream>

#include "dungeon/actioneconomy.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    TurnState turn{};
    turn.movementRemaining = 6;

    require(canUseMoveAction(turn), "fresh turn permits normal movement");
    require(canUseStandardAction(turn), "fresh turn permits an attack");
    require(!canUseDoubleMove(turn),
            "fresh turn does not offer Double Move");

    turn.moveActionUsed = true;
    turn.movementRemaining = 3;
    require(canUseStandardAction(turn),
            "partial first movement preserves the standard action");
    require(canUseDoubleMove(turn),
            "movement followed by Double Move is legal");

    require(tryBeginDoubleMove(turn, 6),
            "authoritative Double Move transition is accepted");
    require(turn.standardActionUsed && turn.movementRemaining == 9,
            "Double Move spends standard and adds one fresh allowance");

    require(!canUseStandardAction(turn),
            "Double Move consumes the standard action");
    require(!canUseDoubleMove(turn),
            "a second Double Move is illegal");

    turn = TurnState{};
    turn.standardActionUsed = true;
    require(canUseMoveAction(turn),
            "attack first preserves the normal move action");
    require(!canUseDoubleMove(turn),
            "attack followed by Double Move is illegal");
    require(!shouldAdvanceAfterStandardAction(turn.moveActionUsed),
            "attack first retains the turn for movement");
    const uint8_t staleMovement = turn.movementRemaining;
    require(!tryBeginDoubleMove(turn, 6),
            "stale Double Move execution is rejected");
    require(turn.movementRemaining == staleMovement,
            "rejected Double Move does not mutate movement");

    turn = TurnState{};
    turn.moveActionUsed = true;
    turn.movementRemaining = 4;
    require(tryBeginDoubleMove(turn, 6) && turn.movementRemaining == 10,
            "moving two of six then Double Move leaves ten squares");

    turn = TurnState{};
    turn.moveActionUsed = true;
    require(tryBeginDoubleMove(turn, 6) && turn.movementRemaining == 6,
            "an exhausted first move gains exactly one full allowance");

    turn.moveActionUsed = true;
    require(!canUseMoveAction(turn),
            "attack followed by movement spends both main actions");
    require(shouldAdvanceAfterStandardAction(turn.moveActionUsed),
            "move followed by attack advances the authoritative turn");
    require(shouldAdvanceAfterMovement(true, true, 0),
            "exhausting movement after an attack advances the turn");
    require(!shouldAdvanceAfterMovement(true, true, 2),
            "an active movement allowance may still be completed");

    std::cout << "PF1e action economy tests passed\n";
    return 0;
}
