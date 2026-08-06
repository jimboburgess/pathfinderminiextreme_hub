//
// Created by james on 7/26/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_TURNS_H
#define PATHFINDERMINIEXTREME_025_TURNS_H

#include <stdint.h>

enum MonsterTurnState
{
    MONSTER_START,
    MONSTER_MOVE,
    MONSTER_ACTION,
    MONSTER_ATTACK,
    MONSTER_END
};

struct TurnState
{
    MonsterTurnState monsterState = MONSTER_START;
    uint8_t movementRemaining;
    bool standardActionUsed;
    bool turnActive;
    bool fullDefense;
    bool fiveFootStepUsed;
    bool delayTurn;
    bool moveActionUsed = false;

};



#endif // PATHFINDERMINIEXTREME_025_TURNS_H