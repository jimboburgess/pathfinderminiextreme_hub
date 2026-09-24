#include "map/mapeffects.h"

#include <stdlib.h>

#include "dungeon/abilityresolver.h"
#include "dungeon/combat.h"
#include "dungeon/npcs.h"
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
        case MAP_EFFECT_WEB:
            return true;

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

bool isWebAt(int x, int y)
{
    return hasMapEffectAt(MAP_EFFECT_WEB, x, y);
}

uint8_t countWebTilesOnLine(
    int startX, int startY, int endX, int endY)
{
    int currentX = startX;
    int currentY = startY;
    const int deltaX = abs(endX - startX);
    const int deltaY = abs(endY - startY);
    const int stepX = startX < endX ? 1 : -1;
    const int stepY = startY < endY ? 1 : -1;
    int error = deltaX - deltaY;
    uint8_t webTiles = 0;

    while (true)
    {
        if (!(currentX == startX && currentY == startY) &&
            !(currentX == endX && currentY == endY) &&
            isWebAt(currentX, currentY))
        {
            webTiles++;
        }

        if (currentX == endX && currentY == endY)
            return webTiles;

        const int doubledError = error * 2;
        if (doubledError > -deltaY)
        {
            error -= deltaY;
            currentX += stepX;
        }
        if (doubledError < deltaX)
        {
            error += deltaX;
            currentY += stepY;
        }
    }
}

CoverLevel coverForWebCount(uint8_t webTiles)
{
    if (webTiles >= 4)
        return COVER_TOTAL;
    if (webTiles >= 1)
        return COVER_PARTIAL;
    return COVER_NONE;
}

bool tileIsIgnited(
    int x, int y, const AreaFlashTile* tiles, uint8_t tileCount)
{
    for (uint8_t i = 0; i < tileCount; i++)
        if (tiles[i].x == x && tiles[i].y == y)
            return true;
    return false;
}

bool mapEffectEntityOccupiesTile(const Entity& entity, int x, int y)
{
    return x >= entity.x && x < entity.x + getEntityTileWidth(entity) &&
           y >= entity.y && y < entity.y + getEntityTileHeight(entity);
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
    {
        removeCondition(entities[i].character, CONDITION_GRAPPLED);
        removeCondition(entities[i].character, CONDITION_WEBBED);
    }
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

bool hasDifficultMapEffectForEntityAt(
    const Entity& entity, int x, int y)
{
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        const MapEffect& effect = activeMapEffects[i];
        if (!isDifficultMapEffect(effect.type) ||
            !mapEffectAffectsTile(effect, x, y))
        {
            continue;
        }

        if (effect.type == MAP_EFFECT_WEB && isImmuneToWeb(entity))
            continue;
        return true;
    }
    return false;
}

CoverLevel getCoverBetween(const Entity& attacker, const Entity& target)
{
    uint8_t bestWebCount = UINT8_MAX;
    for (uint8_t attackerY = 0;
         attackerY < getEntityTileHeight(attacker); attackerY++)
    {
        for (uint8_t attackerX = 0;
             attackerX < getEntityTileWidth(attacker); attackerX++)
        {
            for (uint8_t targetY = 0;
                 targetY < getEntityTileHeight(target); targetY++)
            {
                for (uint8_t targetX = 0;
                     targetX < getEntityTileWidth(target); targetX++)
                {
                    const uint8_t count = countWebTilesOnLine(
                        attacker.x + attackerX, attacker.y + attackerY,
                        target.x + targetX, target.y + targetY);
                    if (count < bestWebCount)
                        bestWebCount = count;
                }
            }
        }
    }
    return coverForWebCount(
        bestWebCount == UINT8_MAX ? 0 : bestWebCount);
}

CoverLevel getCoverFromEntityToTile(
    const Entity& attacker, int targetX, int targetY)
{
    uint8_t bestWebCount = UINT8_MAX;
    for (uint8_t offsetY = 0;
         offsetY < getEntityTileHeight(attacker); offsetY++)
    {
        for (uint8_t offsetX = 0;
             offsetX < getEntityTileWidth(attacker); offsetX++)
        {
            const uint8_t count = countWebTilesOnLine(
                attacker.x + offsetX, attacker.y + offsetY,
                targetX, targetY);
            if (count < bestWebCount)
                bestWebCount = count;
        }
    }
    return coverForWebCount(
        bestWebCount == UINT8_MAX ? 0 : bestWebCount);
}

int getCoverArmorClassBonus(CoverLevel cover)
{
    return cover == COVER_PARTIAL ? 4 : 0;
}

int getCoverSavingThrowBonus(CoverLevel cover, SaveType saveType)
{
    return cover == COVER_PARTIAL && saveType == SAVE_REFLEX ? 2 : 0;
}

MapEffectTriggerResult applyMapEffectToEntity(
    const MapEffect& effect,
    Entity& entity)
{
    MapEffectTriggerResult result;

    if (!effect.active || !entity.active || isBertramRiddleman(entity) ||
        !isCombatEntity(entity) ||
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
            entity.character, effect.saveType, effect.saveDC,
            getCoverSavingThrowBonus(
                getCoverFromEntityToTile(entity, effect.x, effect.y),
                effect.saveType));
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
            // Retain the compact result field, but report damage that reached
            // HP after typed protection and resistance rather than the raw
            // dice result.
            result.damageRolled = damageResult.damageApplied;
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

bool canAttemptEscapeWeb(const Entity& entity)
{
    return entity.active && entity.character.state == STATE_ALIVE &&
        hasCondition(entity.character, CONDITION_GRAPPLED) &&
        getWebEffectAffectingEntity(entity) != nullptr;
}

bool attemptEscapeWeb(Entity& entity, int acrobaticsTotal)
{
    const MapEffect* web = getWebEffectAffectingEntity(entity);
    if (!canAttemptEscapeWeb(entity) || web == nullptr ||
        acrobaticsTotal < web->saveDC)
    {
        return false;
    }
    return removeCondition(entity.character, CONDITION_GRAPPLED);
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
        {
            removeCondition(entities[i].character, CONDITION_GRAPPLED);
            removeCondition(entities[i].character, CONDITION_WEBBED);
        }
    }
    return true;
}

WebBurnResult burnWebAtTiles(
    const AreaFlashTile* tiles, uint8_t tileCount)
{
    WebBurnResult result;
    if (tiles == nullptr || tileCount == 0)
        return result;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    bool shouldDamage[MAX_ENTITIES]{};

    for (uint8_t effectIndex = 0;
         effectIndex < MAX_MAP_EFFECTS; effectIndex++)
    {
        MapEffect& effect = activeMapEffects[effectIndex];
        if (!effect.active || effect.type != MAP_EFFECT_WEB)
            continue;

        MapEffectTile sourceTiles[MAX_MAP_EFFECT_TILES];
        uint8_t sourceCount = 0;
        if (effect.tileCount > 0)
        {
            sourceCount = effect.tileCount;
            for (uint8_t i = 0; i < sourceCount; i++)
                sourceTiles[i] = effect.tiles[i];
        }
        else
        {
            for (int y = effect.y - effect.radius;
                 y <= effect.y + effect.radius; y++)
            {
                for (int x = effect.x - effect.radius;
                     x <= effect.x + effect.radius; x++)
                {
                    if (sourceCount < MAX_MAP_EFFECT_TILES &&
                        isInsideActiveMap(x, y))
                    {
                        sourceTiles[sourceCount].x =
                            static_cast<int8_t>(x);
                        sourceTiles[sourceCount].y =
                            static_cast<int8_t>(y);
                        sourceCount++;
                    }
                }
            }
        }

        uint8_t remainingCount = 0;
        for (uint8_t i = 0; i < sourceCount; i++)
        {
            const int x = sourceTiles[i].x;
            const int y = sourceTiles[i].y;
            if (!tileIsIgnited(x, y, tiles, tileCount))
            {
                effect.tiles[remainingCount++] = sourceTiles[i];
                continue;
            }

            result.tilesCleared++;
            markTileDirty(x, y);
            for (uint8_t entityIndex = 0;
                 entities != nullptr && entityIndex < entityCount;
                 entityIndex++)
            {
                const Entity& entity = entities[entityIndex];
                if (entity.active && entity.character.state == STATE_ALIVE &&
                    mapEffectEntityOccupiesTile(entity, x, y))
                {
                    shouldDamage[entityIndex] = true;
                }
            }
        }

        effect.tileCount = remainingCount;
        effect.radius = 0;
        if (remainingCount == 0)
            effect = MapEffect{};
    }

    for (uint8_t i = 0; entities != nullptr && i < entityCount; i++)
    {
        if (shouldDamage[i] && entities[i].active &&
            entities[i].character.state == STATE_ALIVE)
        {
            const CombatDamageResult damage = applyCombatDamage(
                entities[i], rollDice(2, 4), DAMAGE_FIRE);
            if (damage.applied)
            {
                result.creaturesDamaged++;
                result.damageApplied += damage.damageApplied;
            }
        }

        if (getWebEffectAffectingEntity(entities[i]) == nullptr)
            removeCondition(entities[i].character, CONDITION_GRAPPLED);
    }

    return result;
}

WebBurnResult burnWebEffect(MapEffect& effect)
{
    AreaFlashTile tiles[MAX_MAP_EFFECT_TILES];
    uint8_t tileCount = 0;
    if (!effect.active || effect.type != MAP_EFFECT_WEB)
        return WebBurnResult{};

    if (effect.tileCount > 0)
    {
        tileCount = effect.tileCount;
        for (uint8_t i = 0; i < tileCount; i++)
            tiles[i] = { effect.tiles[i].x, effect.tiles[i].y };
    }
    else
    {
        for (int y = effect.y - effect.radius;
             y <= effect.y + effect.radius; y++)
        {
            for (int x = effect.x - effect.radius;
                 x <= effect.x + effect.radius; x++)
            {
                if (tileCount < MAX_MAP_EFFECT_TILES &&
                    isInsideActiveMap(x, y))
                {
                    tiles[tileCount++] = {
                        static_cast<int8_t>(x),
                        static_cast<int8_t>(y) };
                }
            }
        }
    }
    return burnWebAtTiles(tiles, tileCount);
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
