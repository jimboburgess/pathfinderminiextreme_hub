#ifndef PATHFINDERMINIEXTREME_025_MAP_EFFECTS_H
#define PATHFINDERMINIEXTREME_025_MAP_EFFECTS_H

#include <stdint.h>

#include "characters/abilities.h"

struct Entity;

constexpr uint8_t MAX_MAP_EFFECTS = 8;

struct MapEffect
{
    bool active = false;
    MapEffectType type = MAP_EFFECT_NONE;
    AbilityID sourceAbility = ABILITY_NONE;

    int8_t x = 0;
    int8_t y = 0;
    uint8_t radius = 0;
    uint8_t roundsRemaining = 0;

    SaveType saveType = SAVE_NONE;
    int16_t saveDC = 0;
    ConditionType conditionType = CONDITION_NONE;
    int16_t conditionValue = 0;
    uint8_t conditionDuration = 0;
};

struct MapEffectTriggerResult
{
    uint8_t savesAttempted = 0;
    uint8_t savesSucceeded = 0;
    uint8_t conditionsApplied = 0;
    ConditionType conditionApplied = CONDITION_NONE;
};

extern MapEffect activeMapEffects[MAX_MAP_EFFECTS];

bool hasMapEffectCapacity();
MapEffect* addMapEffect(const MapEffect& effect);
bool removeMapEffect(MapEffect& effect);
void clearMapEffects();
void tickMapEffects();

bool mapEffectAffectsTile(const MapEffect& effect, int x, int y);
bool mapEffectAffectsEntityAt(
    const MapEffect& effect,
    const Entity& entity,
    int entityX,
    int entityY);

const MapEffect* getMapEffectAt(int x, int y);
bool hasMapEffectAt(MapEffectType type, int x, int y);
bool hasDifficultMapEffectAt(int x, int y);

MapEffectTriggerResult applyMapEffectToEntity(
    const MapEffect& effect,
    Entity& entity);
MapEffectTriggerResult handleEnteredMapEffects(
    Entity& entity,
    int entityX,
    int entityY);

void markMapEffectTilesDirty(const MapEffect& effect);

#endif // PATHFINDERMINIEXTREME_025_MAP_EFFECTS_H
