#ifndef PATHFINDERMINIEXTREME_025_COMBAT_H
#define PATHFINDERMINIEXTREME_025_COMBAT_H

#include <Arduino.h>

#include "characters/characters.h"
#include "dungeon.h"
#include "forest.h"

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


constexpr uint8_t DETECTION_RANGE = 6;


//======================================
// Combat State
//======================================

struct Combat
{
    bool active;

    CombatPhase phase;

    Character* turnOrder[MAX_DUNGEON_CHARACTERS];

    uint8_t combatantCount;
    uint8_t currentTurnIndex;

    uint8_t combatRound;

    uint8_t movementRemaining;

    bool standardActionUsed;
};

extern Combat combat;

static void takeMonsterTurn(Entity& monster)
{
    monster.x--;
}

Character* getCurrentCombatant();


bool isPlayerTurn();

void nextTurn();



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