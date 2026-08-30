#include "map/mapeffects.h"

#include <stdlib.h>

#include "dungeon/abilityresolver.h"
#include "dungeon/combat.h"
#include "data/dice.h"
#include "map/activemap.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/entitytraits.h"
#include "data/game.h"
#include "graphics/display.h"

MapEffect activeMapEffects[MAX_MAP_EFFECTS];

namespace
{
bool isCombatEntity(const Entity& entity)
{
    return entity.type == ENTITY_PLAYER ||
           entity.type == ENTITY_MONSTER ||
           entity.type == ENTITY_NPC;
}

bool isDifficultMapEffect(MapEffectType type)
{
    switch (type)
    {
        case MAP_EFFECT_GREASE:
            return true;

        case MAP_EFFECT_WEB:
        case MAP_EFFECT_WALL_OF_FIRE:
        case MAP_EFFECT_ACID_FOG:
        case MAP_EFFECT_BLADE_BARRIER:
            return false;

        case MAP_EFFECT_NONE:
            return false;
    }

    return false;
}

void addTriggerResult(
    MapEffectTriggerResult& total,
    const MapEffectTriggerResult& addition)
{
    total.savesAttempted += addition.savesAttempted;
    total.savesSucceeded += addition.savesSucceeded;
    total.conditionsApplied += addition.conditionsApplied;
    total.damageTriggers += addition.damageTriggers;
    total.damageRolled += addition.damageRolled;
    total.targetDefeated |= addition.targetDefeated;

    if (total.conditionApplied == CONDITION_NONE)
        total.conditionApplied = addition.conditionApplied;
}
}

bool hasMapEffectCapacity()
{
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        if (!activeMapEffects[i].active)
            return true;
    }

    return false;
}

MapEffect* addMapEffect(const MapEffect& effect)
{
    if (effect.type == MAP_EFFECT_NONE ||
        !isInsideActiveMap(effect.x, effect.y) ||
        (effect.roundsRemaining == 0 && !effect.expiresWithCombat))
    {
        return nullptr;
    }

    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        if (activeMapEffects[i].active)
            continue;

        activeMapEffects[i] = effect;
        activeMapEffects[i].active = true;
        markMapEffectTilesDirty(activeMapEffects[i]);
        return &activeMapEffects[i];
    }

    return nullptr;
}

bool removeMapEffect(MapEffect& effect)
{
    if (!effect.active)
        return false;

    markMapEffectTilesDirty(effect);
    effect = MapEffect{};
    return true;
}

void clearMapEffects()
{
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        if (activeMapEffects[i].active)
            markMapEffectTilesDirty(activeMapEffects[i]);

        activeMapEffects[i] = MapEffect{};
    }

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    for (uint8_t i = 0; entities != nullptr && i < entityCount; i++)
        removeCondition(entities[i].character, CONDITION_WEBBED);
}

void tickMapEffects()
{
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        MapEffect& effect = activeMapEffects[i];

        if (!effect.active || effect.expiresWithCombat ||
            effect.roundsRemaining == 0)
            continue;

        effect.roundsRemaining--;

        if (effect.roundsRemaining == 0)
            removeMapEffect(effect);
    }
}

bool mapEffectAffectsTile(const MapEffect& effect, int x, int y)
{
    if (!effect.active || !isInsideActiveMap(x, y))
        return false;

    if (effect.tileCount > 0)
    {
        for (uint8_t i = 0; i < effect.tileCount; i++)
            if (effect.tiles[i].x == x && effect.tiles[i].y == y)
                return true;
        return false;
    }

    return abs(x - effect.x) <= effect.radius &&
           abs(y - effect.y) <= effect.radius;
}

bool mapEffectAffectsEntityAt(
    const MapEffect& effect,
    const Entity& entity,
    int entityX,
    int entityY)
{
    for (uint8_t offsetY = 0;
         offsetY < getEntityTileHeight(entity);
         offsetY++)
    {
        for (uint8_t offsetX = 0;
             offsetX < getEntityTileWidth(entity);
             offsetX++)
        {
            if (mapEffectAffectsTile(
                    effect,
                    entityX + offsetX,
                    entityY + offsetY))
            {
                return true;
            }
        }
    }

    return false;
}

const MapEffect* getMapEffectAt(int x, int y)
{
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        if (mapEffectAffectsTile(activeMapEffects[i], x, y))
            return &activeMapEffects[i];
    }

    return nullptr;
}

bool hasMapEffectAt(MapEffectType type, int x, int y)
{
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        if (activeMapEffects[i].type == type &&
            mapEffectAffectsTile(activeMapEffects[i], x, y))
        {
            return true;
        }
    }

    return false;
}

bool hasDifficultMapEffectAt(int x, int y)
{
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        const MapEffect& effect = activeMapEffects[i];

        if (isDifficultMapEffect(effect.type) &&
            mapEffectAffectsTile(effect, x, y))
        {
            return true;
        }
    }

    return false;
}

MapEffectTriggerResult applyMapEffectToEntity(
    const MapEffect& effect,
    Entity& entity)
{
    MapEffectTriggerResult result;

    if (!effect.active || !entity.active || !isCombatEntity(entity) ||
        entity.character.state != STATE_ALIVE ||
        !mapEffectAffectsEntityAt(
            effect, entity, entity.x, entity.y))
    {
        return result;
    }

    if (effect.type == MAP_EFFECT_WEB && isImmuneToWeb(entity))
        return result;

    bool saveSucceeded = false;
    if (effect.saveType != SAVE_NONE)
    {
        AbilitySavingThrow savingThrow = resolveSavingThrow(
            entity.character, effect.saveType, effect.saveDC);
        result.savesAttempted = 1;

        if (savingThrow.result == SAVE_RESULT_SUCCESS)
        {
            result.savesSucceeded = 1;
            saveSucceeded = true;
        }
    }

    if (effect.damageType != DAMAGE_NONE &&
        !(saveSucceeded && effect.damageSaveEffect == SAVE_EFFECT_NEGATES))
    {
        int damage = effect.flatDamage;
        if (effect.damageDiceCount > 0 && effect.damageDiceSides > 0)
            damage += rollDice(effect.damageDiceCount, effect.damageDiceSides);
        if (saveSucceeded && effect.damageSaveEffect == SAVE_EFFECT_HALF)
            damage /= 2;

        playAbilityImpactFlash(
            IMPACT_DAMAGE, effect.damageType, entity.x, entity.y);
        const CombatDamageResult damageResult = applyCombatDamage(
            entity, damage, effect.damageType);
        if (damageResult.applied)
        {
            result.damageTriggers = 1;
            result.damageRolled = damage;
            result.targetDefeated = damageResult.defeated ||
                entity.character.state != STATE_ALIVE;
        }
    }

    if (!saveSucceeded && effect.conditionType != CONDITION_NONE &&
        canReceiveCondition(entity.character, effect.conditionType) &&
        addCondition(
            entity.character,
            effect.conditionType,
            effect.conditionValue,
            effect.conditionDuration))
    {
        result.conditionsApplied = 1;
        result.conditionApplied = effect.conditionType;
        if (effect.type == MAP_EFFECT_WEB)
            entity.turn.movementRemaining = 0;
    }

    return result;
}

const MapEffect* getWebEffectAffectingEntity(const Entity& entity)
{
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        if (activeMapEffects[i].type == MAP_EFFECT_WEB &&
            mapEffectAffectsEntityAt(
                activeMapEffects[i], entity, entity.x, entity.y))
        {
            return &activeMapEffects[i];
        }
    }
    return nullptr;
}

bool removeWebEffect(MapEffect& effect)
{
    if (!effect.active || effect.type != MAP_EFFECT_WEB)
        return false;

    removeMapEffect(effect);
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    for (uint8_t i = 0; entities != nullptr && i < entityCount; i++)
    {
        if (getWebEffectAffectingEntity(entities[i]) == nullptr)
            removeCondition(entities[i].character, CONDITION_WEBBED);
    }
    return true;
}

MapEffectTriggerResult handleEnteredMapEffects(
    Entity& entity,
    int entityX,
    int entityY)
{
    MapEffectTriggerResult result;

    if (!entity.active || entity.character.state != STATE_ALIVE)
        return result;

    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        const MapEffect& effect = activeMapEffects[i];

        if (!mapEffectAffectsEntityAt(
                effect, entity, entityX, entityY))
        {
            continue;
        }

        addTriggerResult(result, applyMapEffectToEntity(effect, entity));
    }

    return result;
}

MapEffectTriggerResult handleStartingTurnMapEffects(Entity& entity)
{
    MapEffectTriggerResult result;
    if (!entity.active || entity.character.state != STATE_ALIVE)
        return result;

    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        const MapEffect& effect = activeMapEffects[i];
        if (!mapEffectAffectsEntityAt(effect, entity, entity.x, entity.y))
            continue;

        addTriggerResult(result, applyMapEffectToEntity(effect, entity));
        if (entity.character.state != STATE_ALIVE)
            break;
    }
    return result;
}

void markMapEffectTilesDirty(const MapEffect& effect)
{
    if (!effect.active)
        return;

    uint8_t validTileCount = 0;

    if (effect.tileCount > 0)
        validTileCount = effect.tileCount;
    else
        for (int y = effect.y - effect.radius; y <= effect.y + effect.radius; y++)
            for (int x = effect.x - effect.radius; x <= effect.x + effect.radius; x++)
                if (isInsideActiveMap(x, y)) validTileCount++;

    // Multiple effects may expire on the same round boundary. If their
    // combined footprints cannot fit in the fixed dirty-tile queue, request
    // one full redraw so no stale overlay remains on screen.
    if (dirtyTileCount + validTileCount > MAX_DIRTY_TILES)
    {
        backgroundNeedsRedraw = true;
        redrawType = REDRAW_FULL;
        needsRedraw = true;
        return;
    }

    if (effect.tileCount > 0)
    {
        for (uint8_t i = 0; i < effect.tileCount; i++)
            markTileDirty(effect.tiles[i].x, effect.tiles[i].y);
        return;
    }

    for (int y = effect.y - effect.radius; y <= effect.y + effect.radius; y++)
        for (int x = effect.x - effect.radius; x <= effect.x + effect.radius; x++)
            if (isInsideActiveMap(x, y)) markTileDirty(x, y);
}
