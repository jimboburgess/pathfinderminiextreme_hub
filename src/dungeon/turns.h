//
// Created by james on 7/26/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_TURNS_H
#define PATHFINDERMINIEXTREME_025_TURNS_H

#include <stdint.h>

enum MonsterTurnState : uint8_t
{
    MONSTER_START,
    MONSTER_MOVE,
    MONSTER_ACTION,
    MONSTER_ATTACK,
    MONSTER_END,
    MONSTER_THINKING
};

constexpr uint8_t MONSTER_INITIAL_UNAWARE_TURN = 0x10;

struct TurnState
{
    MonsterTurnState monsterState = MONSTER_START;
    uint8_t movementRemaining;
    uint8_t bonusAttacksRemaining = 0;
    bool standardActionUsed;
    bool turnActive;
    bool fullDefense;
    bool fiveFootStepUsed;
    bool delayTurn;
    bool moveActionUsed = false;
    bool powerAttackActive = false;
    // Transient encounter state; reset when a new combat roster is built.
    bool oncePerCombatAbilityUsed = false;
    uint8_t monsterRuntimeFlags = 0;
    // Presentation-only ACTION plus prioritized WALK/HIT deadlines. The
    // byte-sized enum above uses prior padding, keeping TurnState at 16 bytes.
    uint32_t visualStateTiming = 0;

};

static_assert(sizeof(TurnState) <= 16,
              "TurnState must remain compact for fixed Entity arrays");

inline bool hasInitialUnawareTurn(const TurnState& turn)
{
    return (turn.monsterRuntimeFlags & MONSTER_INITIAL_UNAWARE_TURN) != 0;
}

inline void setInitialUnawareTurn(TurnState& turn, bool unaware)
{
    if (unaware)
        turn.monsterRuntimeFlags |= MONSTER_INITIAL_UNAWARE_TURN;
    else
        turn.monsterRuntimeFlags &=
            static_cast<uint8_t>(~MONSTER_INITIAL_UNAWARE_TURN);
}



#endif // PATHFINDERMINIEXTREME_025_TURNS_H
