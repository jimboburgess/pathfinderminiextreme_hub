//
// Created by james on 8/1/2026.
//

#ifndef MONSTER_SCRIPTS_H
#define MONSTER_SCRIPTS_H

struct Entity;

void runMonsterScript(Entity* monster);

// Individual scripts

void runMeleeScript(Entity* monster);
void runRangedScript(Entity* monster);
void runCowardScript(Entity* monster);
void runGuardScript(Entity* monster);
void runWanderScript(Entity* monster);
void runSupportScript(Entity* monster);
void runSpellcasterScript(Entity* monster);
void runControlSpellcasterScript(Entity* monster);
void runDebugScript(Entity* monster);

Entity* chooseTarget(Entity* monster);

void performStandardAction(Entity* monster);
bool keepDistance(Entity* monster);
void performRangedAttack(Entity* monster);
bool isMonsterReadyForAction(Entity* monster);

//==================================================
// Movement
//==================================================

void performMovementPhase(Entity* entity);
void moveMonsterTowardsPlayer(Entity* monster);
bool canMonsterMoveTo(Entity* monster, int x, int y);
bool isAdjacent(const Entity* a, const Entity* b);


#endif
