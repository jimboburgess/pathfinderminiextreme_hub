#ifndef PATHFINDERMINIEXTREME_025_COMBAT_H
#define PATHFINDERMINIEXTREME_025_COMBAT_H

#include <Arduino.h>
#include <algorithm>
#include <stdlib.h>

#include "characters/characters.h"
#include "dungeon.h"
#include "forest.h"
#include "graphics/tiles.h"
#include "monsterScripts.h"

//==================================================
// Combat Constants
//==================================================

constexpr uint8_t COMBAT_DETECTION_RANGE = 6;

//==================================================
// Combat Phases
//==================================================

enum CombatPhase
{
    COMBAT_NONE,          // No combat active
    COMBAT_INITIATIVE,    // Determine turn order
    COMBAT_TURN,          // Combat is in progress
    COMBAT_END            // Cleanup combat
};

//==================================================
// Combat State
//==================================================

struct Combat
{
    bool active = false;

    CombatPhase phase = COMBAT_NONE;

    //--------------------------------------------------
    // Initiative
    //--------------------------------------------------

    Entity* initiativeOrder[MAX_DUNGEON_CHARACTERS];

    uint8_t combatantCount = 0;
    uint8_t currentTurnIndex = 0;

    uint8_t combatRound = 0;

    //--------------------------------------------------
    // Player Input
    //--------------------------------------------------

    bool waitingForPlayer = false;

    //--------------------------------------------------
    // Phase Timing
    //--------------------------------------------------

    unsigned long phaseStartTime = 0;
    unsigned long nextMonsterStep = 0;
    bool initiativeMessageShown = false;
};

extern Combat combat;

//==================================================
// Combat Detection
//==================================================

void checkForCombat();

void findCombatants();

bool hasLineOfSight(int x1, int y1, int x2, int y2);

bool blocksSight(TileType tile);

//==================================================
// Combat Startup
//==================================================

void startCombat();

void rollInitiative();

void sortInitiative();

//==================================================
// Turn Engine
//==================================================

Entity* getCurrentCombatant();

bool isPlayerTurn();

void announceTurn(Entity* entity);

void runCombatTurn(Entity* entity);

void nextTurn();

void endPlayerTurn();

//==================================================
// Player / Monster Turns
//==================================================

void runPlayerTurn(Entity* entity);

void runMonsterTurn(Entity* entity);

void runMonsterAI(Entity* monster);



//==================================================
// Combat Update
//==================================================

void updateCombat();

void endCombat();

bool isCombatActive();

#endif // PATHFINDERMINIEXTREME_025_COMBAT_H