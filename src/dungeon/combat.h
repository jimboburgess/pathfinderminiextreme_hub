#ifndef PATHFINDERMINIEXTREME_025_COMBAT_H
#define PATHFINDERMINIEXTREME_025_COMBAT_H

#include <Arduino.h>

#include "characters/characters.h"
#include "dungeon.h"

//======================================
// Combat Phases
//======================================

enum CombatPhase
{
    COMBAT_NONE,
    COMBAT_INITIATIVE,
    COMBAT_TURN,
    COMBAT_END
};

enum TurnState
{
    TURN_PLAYER,
    TURN_MONSTERS
};


constexpr uint8_t DETECTION_RANGE = 6;


//======================================
// Combat State
//======================================

struct Combat
{
    bool active = false;

    CombatPhase phase = COMBAT_NONE;

    Character* turnOrder[MAX_DUNGEON_CHARACTERS];

    TurnState turn = TURN_PLAYER;

    uint8_t combatantCount = 0;
    uint8_t currentTurnIndex = 0;

    uint8_t combatRound = 1;

    uint8_t movementRemaining = 0;

    bool standardActionUsed = false;
};

extern Combat combat;

static void takeMonsterTurn(Entity& monster)
{
    monster.x--;
}

//======================================
// Combat Functions
//======================================

void startCombat();

void endPlayerTurn();

void updateMonsterTurn();

void checkForCombat();

void updateCombat();

void endCombat();

bool isCombatActive();

#endif // PATHFINDERMINIEXTREME_025_COMBAT_H