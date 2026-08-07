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
constexpr unsigned long COMBAT_MESSAGE_PAUSE_MS = 1200;

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

enum CombatAttackType
{
    COMBAT_ATTACK_NONE,
    COMBAT_ATTACK_MELEE,
    COMBAT_ATTACK_RANGED
};

enum MonsterAttackPhase
{
    MONSTER_ATTACK_NONE,
    MONSTER_ATTACK_ROLL_RESULT,
    MONSTER_ATTACK_DAMAGE_RESULT,
    MONSTER_ATTACK_COMPLETE
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

    //--------------------------------------------------
    // Player attack targeting and result timing
    //--------------------------------------------------

    CombatAttackType attackType = COMBAT_ATTACK_NONE;
    int8_t selectedTargetIndex = -1;
    Entity* pendingAttackTarget = nullptr;
    int pendingDamage = 0;
    unsigned long attackResultTime = 0;
    bool attackDamagePending = false;
    bool attackResolutionPending = false;

    //--------------------------------------------------
    // Monster attack result timing
    //--------------------------------------------------

    MonsterAttackPhase monsterAttackPhase = MONSTER_ATTACK_NONE;
    Entity* attackingMonster = nullptr;
    Entity* monsterAttackTarget = nullptr;
    int monsterPendingDamage = 0;
    bool monsterAttackHit = false;
    bool monsterDefeatedPlayer = false;
    unsigned long monsterAttackTime = 0;

    //--------------------------------------------------
    // Entity inspection
    //--------------------------------------------------

    bool inspecting = false;
    int8_t inspectedEntityIndex = -1;
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

void checkEndPlayerTurn();

//==================================================
// Player Attacks
//==================================================

void beginPlayerAttack(CombatAttackType attackType);
bool isPlayerTargetingAttack();
bool isPlayerAttackResolving();
Entity* getSelectedAttackTarget();
void rotateAttackTarget(bool forward);
void confirmPlayerAttack();
void cancelPlayerAttack();

//==================================================
// Monster Attacks
//==================================================

void beginMonsterAttack(Entity* monster, Entity* target);
bool isMonsterAttackResolving();

//==================================================
// Entity Inspection
//==================================================

void beginInspection();
bool isInspectingEntities();
Entity* getInspectedEntity();
void rotateInspectedEntity(bool forward);
void confirmInspection();
void cancelInspection();

//==================================================
// Player Combat Actions
//==================================================

void beginDoubleMove();
void beginTotalDefense();

//==================================================
// Combat Update
//==================================================

void updateCombat();

void endCombat();

bool isCombatActive();

#endif // PATHFINDERMINIEXTREME_025_COMBAT_H
