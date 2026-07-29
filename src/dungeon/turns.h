//
// Created by james on 7/26/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_TURNS_H
#define PATHFINDERMINIEXTREME_025_TURNS_H

#include <stdint.h>

struct TurnState
{
    uint8_t movementRemaining;
    bool standardActionUsed;
    bool turnActive;
    bool fullDefense;
    bool fiveFootStepUsed;
    bool delayTurn;
};

#endif // PATHFINDERMINIEXTREME_025_TURNS_H