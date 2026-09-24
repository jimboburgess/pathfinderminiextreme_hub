#ifndef PATHFINDERMINIEXTREME_025_MAP_EFFECTS_H
#define PATHFINDERMINIEXTREME_025_MAP_EFFECTS_H

#include <stdint.h>

#include "characters/abilities.h"

struct Entity;
struct AreaFlashTile;

enum CoverLevel : uint8_t
{
    COVER_NONE,
    COVER_PARTIAL,
    COVER_TOTAL
};

constexpr uint8_t MAX_MAP_EFFECTS = 8;
constexpr uint8_t MAX_MAP_EFFECT_TILES = 25;

struct MapEffectTile
{
    int8_t x = 0;
    int8_t y = 0;
};

struct MapEffect
{
    bool active = false;
    MapEffectType type = MAP_EFFECT_NONE;
    AbilityID sourceAbility = ABILITY_NONE;

    int8_t x = 0;
    int8_t y = 0;
    uint8_t radius = 0;
    uint8_t roundsRemaining = 0;
    bool expiresWithCombat = false;

    SaveType saveType = SAVE_NONE;
    int16_t saveDC = 0;
    ConditionType conditionType = CONDITION_NONE;
    int16_t conditionValue = 0;
    uint8_t conditionDuration = 0;

    // A fixed footprint lets persistent lines and ground bursts use the exact
    // same tiles selected by the ability geometry without heap allocation.
    uint8_t tileCount = 0;
    MapEffectTile tiles[MAX_MAP_EFFECT_TILES];

    DamageType damageType = DAMAGE_NONE;
    uint8_t damageDiceCount = 0;
    uint8_t damageDiceSides = 0;
    int16_t flatDamage = 0;
    SaveEffect damageSaveEffect = SAVE_EFFECT_NONE;
};

struct MapEffectTriggerResult
{
    uint8_t savesAttempted = 0;
    uint8_t savesSucceeded = 0;
    uint8_t conditionsApplied = 0;
    ConditionType conditionApplied = CONDITION_NONE;
    uint8_t damageTriggers = 0;
    int16_t damageRolled = 0;
    bool targetDefeated = false;
};

struct WebBurnResult
{
    uint8_t tilesCleared = 0;
    uint8_t creaturesDamaged = 0;
    int16_t damageApplied = 0;
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
bool hasDifficultMapEffectForEntityAt(
    const Entity& entity, int x, int y);

// Generic relative cover hooks. Web is the first map-effect contributor;
// callers do not need to know which terrain/effect supplied the cover.
CoverLevel getCoverBetween(const Entity& attacker, const Entity& target);
CoverLevel getCoverFromEntityToTile(
    const Entity& attacker, int targetX, int targetY);
int getCoverArmorClassBonus(CoverLevel cover);
int getCoverSavingThrowBonus(CoverLevel cover, SaveType saveType);

MapEffectTriggerResult applyMapEffectToEntity(
    const MapEffect& effect,
    Entity& entity);
MapEffectTriggerResult handleEnteredMapEffects(
    Entity& entity,
    int entityX,
    int entityY);
MapEffectTriggerResult handleStartingTurnMapEffects(Entity& entity);

void markMapEffectTilesDirty(const MapEffect& effect);
const MapEffect* getWebEffectAffectingEntity(const Entity& entity);
bool canAttemptEscapeWeb(const Entity& entity);
bool attemptEscapeWeb(Entity& entity, int acrobaticsTotal);
bool removeWebEffect(MapEffect& effect);
WebBurnResult burnWebAtTiles(
    const AreaFlashTile* tiles, uint8_t tileCount);
WebBurnResult burnWebEffect(MapEffect& effect);

#endif // PATHFINDERMINIEXTREME_025_MAP_EFFECTS_H
