#ifndef PATHFINDERMINIEXTREME_025_ENTITY_TRAITS_H
#define PATHFINDERMINIEXTREME_025_ENTITY_TRAITS_H

#include <stdint.h>

struct Entity;

// Player/NPC level is their effective HD. Monsters retain the hit dice from
// their static definition rather than borrowing caster level or deriving HD
// from rolled hit points.
uint8_t getEffectiveHitDice(const Entity& entity);

// One shared visual-perception query for naturally sightless monsters and
// temporary blindness. Map line of sight remains a separate geometry rule.
bool canSee(const Entity& entity);
bool isImmuneToWeb(const Entity& entity);
bool isUndeadCreature(const Entity& entity);

#endif // PATHFINDERMINIEXTREME_025_ENTITY_TRAITS_H
