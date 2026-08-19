#ifndef PATHFINDERMINIEXTREME_025_COMBAT_H
#define PATHFINDERMINIEXTREME_025_COMBAT_H

#include <Arduino.h>

#include "dungeon.h"

struct Entity;
struct AbilityResolution;

struct CombatDamageResult
{
    bool applied = false;
    bool defeated = false;
    uint8_t levelReached = 0;
};

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
    MONSTER_ATTACK_POISON_RESULT,
    MONSTER_ATTACK_COMPLETE
};

enum TurnStartConditionMessagePhase
{
    TURN_START_CONDITION_NONE,
    TURN_START_CONDITION_DAMAGE_MESSAGE,
    TURN_START_CONDITION_EXPIRY_MESSAGE
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

    Entity* initiativeOrder[MAX_COMBATANTS];

    uint8_t combatantCount = 0;
    uint8_t currentTurnIndex = 0;

    uint8_t combatRound = 0;
    uint32_t defeatedMonsterExperience = 0;
    uint32_t experienceGained = 0;

    //--------------------------------------------------
    // Player Input
    //--------------------------------------------------

    bool waitingForPlayer = false;
    bool endPlayerTurnAfterMessage = false;

    //--------------------------------------------------
    // Shared ability targeting and result timing
    //--------------------------------------------------

    AbilityID selectedAbility = ABILITY_NONE;
    int8_t selectedAbilityX = -1;
    int8_t selectedAbilityY = -1;
    Direction selectedAbilityDirection = DIR_NORTH;
    bool abilityResolutionPending = false;
    Entity* abilityCaster = nullptr;
    bool abilityEndedCombat = false;
    unsigned long abilityResultTime = 0;

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
    bool pendingSneakAttack = false;
    int pendingSneakAttackDamage = 0;
    bool pendingPowerAttack = false;
    int pendingPowerAttackDamage = 0;
    unsigned long attackResultTime = 0;
    bool attackDamagePending = false;
    bool attackResolutionPending = false;
    bool openingAttackTargeting = false;
    bool openingAttackInProgress = false;
    bool openingAttackWasAmbush = false;

    //--------------------------------------------------
    // Monster attack result timing
    //--------------------------------------------------

    MonsterAttackPhase monsterAttackPhase = MONSTER_ATTACK_NONE;
    Entity* attackingMonster = nullptr;
    Entity* monsterAttackTarget = nullptr;
    int monsterPendingDamage = 0;
    bool monsterSneakAttack = false;
    int monsterPendingSneakAttackDamage = 0;
    bool monsterAttackHit = false;
    CombatAttackType monsterAttackType = COMBAT_ATTACK_NONE;
    bool monsterDefeatedPlayer = false;
    unsigned long monsterAttackTime = 0;

    //--------------------------------------------------
    // Start-of-turn condition message timing
    //--------------------------------------------------

    TurnStartConditionMessagePhase turnStartConditionPhase =
        TURN_START_CONDITION_NONE;
    bool turnStartPoisonExpired = false;
    bool turnStartConditionDefeated = false;
    bool turnStartActionPrevented = false;

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
bool isCombatParticipant(const Entity& entity);

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

void nextTurn();

void endPlayerTurn();

// This checks the class, team, target state, and flat-footed requirements.
// Weapon attack resolution calls it only after confirming a hit.
bool canSneakAttack(const Entity& attacker, const Entity& target);

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
void beginOutOfCombatAttack(CombatAttackType attackType);
bool canPlayerAttackOutsideCombat(CombatAttackType attackType);
bool isPlayerTargetingAttack();
bool isPlayerAttackResolving();
Entity* getSelectedAttackTarget();
void rotateAttackTarget(bool forward);
void confirmPlayerAttack();
void cancelPlayerAttack();

//==================================================
// Player Ability Targeting
//==================================================

void beginPlayerAbility(AbilityID abilityID);
bool isPlayerTargetingAbility();
bool isPlayerTargetingGroundAbility();
bool isPlayerTargetingDirectionalAbility();
bool isAbilityResolving();
Entity* getSelectedAbilityTarget();
bool getSelectedAbilityGroundTarget(int& x, int& y);
void rotateAbilityTarget(bool forward);
bool moveGroundAbilityTarget();
void confirmPlayerAbility();
void cancelPlayerAbility();

//==================================================
// Monster Attacks
//==================================================

void beginMonsterAttack(
    Entity* monster,
    Entity* target,
    CombatAttackType attackType = COMBAT_ATTACK_MELEE);
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
bool canTogglePowerAttack(const Entity& fighter);
bool togglePowerAttack(Entity& fighter);
bool canUseChannelEnergy(const Entity& cleric);
bool useChannelEnergy(Entity& cleric);

// Applies damage through the existing one-time combat defeat/XP/loot path.
CombatDamageResult applyCombatDamage(Entity& target, int damage);

// Shared player/monster feedback and combat pacing after a successful
// resolveAbility() call.
void presentAbilityResolution(
    Entity& caster,
    Entity& target,
    AbilityID abilityID,
    const AbilityResolution& resolution);

// Shared feedback/pacing for successful ground and directional resolver calls.
// Player targeting and reusable monster scripts both use these helpers.
void presentGroundAbilityResolution(
    Entity& caster,
    AbilityID abilityID,
    const AbilityResolution& resolution);
void presentDirectionalAbilityResolution(
    Entity& caster,
    AbilityID abilityID,
    const AbilityResolution& resolution);

//==================================================
// Combat Update
//==================================================

void updateCombat();

void endCombat();

// Stops an encounter without running victory/reward handling. Character
// state is preserved; only combat bookkeeping and transient TurnState data
// are discarded.
void abortCombat();

bool isCombatActive();

#endif // PATHFINDERMINIEXTREME_025_COMBAT_H
