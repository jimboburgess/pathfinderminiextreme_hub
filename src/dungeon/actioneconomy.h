#ifndef PATHFINDERMINIEXTREME_025_ACTIONECONOMY_H
#define PATHFINDERMINIEXTREME_025_ACTIONECONOMY_H

#include "dungeon/turns.h"

// PF1e: a normal turn provides one standard and one move action.
// The standard action may be exchanged for a second move action.
inline bool canUseMoveAction(const TurnState& turn)
{
    return !turn.moveActionUsed;
}

inline bool canUseStandardAction(const TurnState& turn)
{
    return !turn.standardActionUsed;
}

inline bool canUseDoubleMove(bool moveActionUsed, bool standardActionUsed)
{
    return moveActionUsed && !standardActionUsed;
}

inline bool canUseDoubleMove(const TurnState& turn)
{
    return canUseDoubleMove(turn.moveActionUsed, turn.standardActionUsed);
}

inline bool tryBeginDoubleMove(
    bool moveActionUsed,
    bool& standardActionUsed,
    uint8_t& movementRemaining,
    uint8_t effectiveSpeed)
{
    if (!canUseDoubleMove(moveActionUsed, standardActionUsed))
        return false;

    standardActionUsed = true;
    const uint16_t combined =
        static_cast<uint16_t>(movementRemaining) + effectiveSpeed;
    movementRemaining = combined > UINT8_MAX
        ? UINT8_MAX : static_cast<uint8_t>(combined);
    return true;
}

inline bool tryBeginDoubleMove(TurnState& turn, uint8_t effectiveSpeed)
{
    return tryBeginDoubleMove(
        turn.moveActionUsed,
        turn.standardActionUsed,
        turn.movementRemaining,
        effectiveSpeed);
}

inline bool shouldAdvanceAfterStandardAction(bool moveActionUsed)
{
    return moveActionUsed;
}

inline bool shouldAdvanceAfterMovement(
    bool moveActionUsed,
    bool standardActionUsed,
    uint8_t movementRemaining)
{
    return moveActionUsed && standardActionUsed && movementRemaining == 0;
}

#endif
