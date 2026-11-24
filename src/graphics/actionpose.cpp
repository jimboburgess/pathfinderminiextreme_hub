#include "actionpose.h"

#include <Arduino.h>

#include "data/entities.h"
#include "graphics/display.h"
#include "map/activemap.h"

const ActionSprite fighterWalkSprite =
    { WALK_SPRITE_HUMANOID, 0xFFE0, 0xFFFF };
const ActionSprite fighterActionSprite =
    { ACTION_SPRITE_WEAPON, 0xFFE0, 0xFFFF };
const ActionSprite fighterHitSprite =
    { HIT_SPRITE_RECOIL, 0xFFE0, 0xFFFF };
const ActionSprite rogueWalkSprite =
    { WALK_SPRITE_LIGHT, 0xC618, 0xFFFF };
const ActionSprite rogueActionSprite =
    { ACTION_SPRITE_WEAPON, 0xC618, 0xFFFF };
const ActionSprite rogueHitSprite =
    { HIT_SPRITE_CROUCH, 0xC618, 0xFFFF };
const ActionSprite wizardWalkSprite =
    { WALK_SPRITE_ROBE, 0xF81F, 0x07FF };
const ActionSprite wizardActionSprite =
    { ACTION_SPRITE_CAST, 0xF81F, 0x07FF };
const ActionSprite wizardHitSprite =
    { HIT_SPRITE_RECOIL, 0xF81F, 0x07FF };
const ActionSprite clericWalkSprite =
    { WALK_SPRITE_HUMANOID, 0xFFE0, 0xFFFF };
const ActionSprite clericActionSprite =
    { ACTION_SPRITE_CAST, 0xFFE0, 0xFFFF };
const ActionSprite clericHitSprite =
    { HIT_SPRITE_RECOIL, 0xFFE0, 0xFFFF };

#define HUMANOID_POSES(name, action, primary, accent) \
    const ActionSprite name##WalkSprite = \
        { WALK_SPRITE_HUMANOID, primary, accent }; \
    const ActionSprite name##ActionSprite = \
        { action, primary, accent }; \
    const ActionSprite name##HitSprite = \
        { HIT_SPRITE_RECOIL, primary, accent }

HUMANOID_POSES(goblin, ACTION_SPRITE_WEAPON, 0xFFE0, 0xC618);
HUMANOID_POSES(goblinArcher, ACTION_SPRITE_RANGED, 0xC618, 0xFFE0);
HUMANOID_POSES(bugbear, ACTION_SPRITE_WEAPON, 0xFBE0, 0xC618);
HUMANOID_POSES(skeleton, ACTION_SPRITE_WEAPON, 0xFFFF, 0xC618);
HUMANOID_POSES(zombie, ACTION_SPRITE_NATURAL, 0xAFE5, 0x7BE0);
HUMANOID_POSES(ghoul, ACTION_SPRITE_NATURAL, 0xD69A, 0xFFFF);
HUMANOID_POSES(wight, ACTION_SPRITE_NATURAL, 0xA81F, 0xF81F);
HUMANOID_POSES(choker, ACTION_SPRITE_NATURAL, 0xFFE0, 0xFD20);
HUMANOID_POSES(skeletonMage, ACTION_SPRITE_CAST, 0xA81F, 0x07FF);
HUMANOID_POSES(goblinChieftain, ACTION_SPRITE_WEAPON, 0xF800, 0xFFE0);

#undef HUMANOID_POSES

const ActionSprite giantSpiderWalkSprite =
    { WALK_SPRITE_SPIDER, 0xFFFF, 0xF800 };
const ActionSprite giantSpiderActionSprite =
    { ACTION_SPRITE_NATURAL, 0xFFFF, 0xF800 };
const ActionSprite giantSpiderHitSprite =
    { HIT_SPRITE_COMPRESS, 0xFFFF, 0xF800 };
const ActionSprite grayOozeWalkSprite =
    { WALK_SPRITE_OOZE, 0x7DFF, 0x07FF };
const ActionSprite grayOozeActionSprite =
    { ACTION_SPRITE_NATURAL, 0x7DFF, 0x07FF };
const ActionSprite grayOozeHitSprite =
    { HIT_SPRITE_COMPRESS, 0x7DFF, 0x07FF };
const ActionSprite violetFungusActionSprite =
    { ACTION_SPRITE_NATURAL, 0xF81F, 0xFFFF };
const ActionSprite violetFungusHitSprite =
    { HIT_SPRITE_COMPRESS, 0xF81F, 0xFFFF };
const ActionSprite spectatorWalkSprite =
    { WALK_SPRITE_FLOAT, 0xF81F, 0x07FF };
const ActionSprite spectatorActionSprite =
    { ACTION_SPRITE_CAST, 0xF81F, 0x07FF };
const ActionSprite spectatorHitSprite =
    { HIT_SPRITE_COMPRESS, 0xF81F, 0x07FF };
const ActionSprite spiderQueenWalkSprite =
    { WALK_SPRITE_SPIDER, 0xF81F, 0xF800 };
const ActionSprite spiderQueenActionSprite =
    { ACTION_SPRITE_NATURAL, 0xF81F, 0xF800 };
const ActionSprite spiderQueenHitSprite =
    { HIT_SPRITE_COMPRESS, 0xF81F, 0xF800 };

namespace
{
constexpr uint32_t VISUAL_TIME_MASK = 0x7FFFu;
constexpr uint32_t TRANSIENT_TIME_SHIFT = 15;
constexpr uint32_t TRANSIENT_STATE_SHIFT = 30;
constexpr uint32_t TRANSIENT_TIME_MASK = VISUAL_TIME_MASK << TRANSIENT_TIME_SHIFT;
constexpr uint32_t TRANSIENT_STATE_MASK = 0x3u << TRANSIENT_STATE_SHIFT;

enum TransientVisualState : uint8_t
{
    TRANSIENT_NONE,
    TRANSIENT_WALK,
    TRANSIENT_HIT
};

uint32_t encodeDeadline(uint32_t now, uint32_t durationMs)
{
    uint32_t deadline = (now + durationMs) & VISUAL_TIME_MASK;
    return deadline == 0 ? 1 : deadline;
}

bool deadlineActive(uint32_t now, uint32_t deadline)
{
    if (deadline == 0) return false;
    const uint32_t difference =
        (deadline - (now & VISUAL_TIME_MASK)) & VISUAL_TIME_MASK;
    return difference != 0 && difference < 0x4000u;
}

uint32_t actionDeadline(const Entity& entity)
{
    return entity.turn.visualStateTiming & VISUAL_TIME_MASK;
}

uint32_t transientDeadline(const Entity& entity)
{
    return (entity.turn.visualStateTiming >> TRANSIENT_TIME_SHIFT) & VISUAL_TIME_MASK;
}

TransientVisualState transientState(const Entity& entity)
{
    return static_cast<TransientVisualState>(
        (entity.turn.visualStateTiming & TRANSIENT_STATE_MASK) >>
        TRANSIENT_STATE_SHIFT);
}

bool canAnimate(const Entity& entity)
{
    return entity.active && entity.character.state == STATE_ALIVE &&
           (entity.type == ENTITY_PLAYER || entity.type == ENTITY_MONSTER);
}

const ActionSprite* getPlayerSprite(
    const Entity& entity, EntityVisualState state)
{
    switch (entity.character.characterClass)
    {
        case CLASS_FIGHTER:
            return state == ENTITY_VISUAL_WALK ? &fighterWalkSprite :
                   state == ENTITY_VISUAL_ACTION ? &fighterActionSprite :
                   state == ENTITY_VISUAL_HIT ? &fighterHitSprite : nullptr;
        case CLASS_ROGUE:
            return state == ENTITY_VISUAL_WALK ? &rogueWalkSprite :
                   state == ENTITY_VISUAL_ACTION ? &rogueActionSprite :
                   state == ENTITY_VISUAL_HIT ? &rogueHitSprite : nullptr;
        case CLASS_WIZARD:
            return state == ENTITY_VISUAL_WALK ? &wizardWalkSprite :
                   state == ENTITY_VISUAL_ACTION ? &wizardActionSprite :
                   state == ENTITY_VISUAL_HIT ? &wizardHitSprite : nullptr;
        case CLASS_CLERIC:
            return state == ENTITY_VISUAL_WALK ? &clericWalkSprite :
                   state == ENTITY_VISUAL_ACTION ? &clericActionSprite :
                   state == ENTITY_VISUAL_HIT ? &clericHitSprite : nullptr;
        default:
            return nullptr;
    }
}

const ActionSprite* getVisualSprite(
    const Entity& entity, EntityVisualState state)
{
    if (entity.type == ENTITY_MONSTER && entity.monster != nullptr)
    {
        if (state == ENTITY_VISUAL_WALK) return entity.monster->walkSprite;
        if (state == ENTITY_VISUAL_ACTION) return entity.monster->actionSprite;
        if (state == ENTITY_VISUAL_HIT) return entity.monster->hitSprite;
        return nullptr;
    }
    return entity.type == ENTITY_PLAYER ? getPlayerSprite(entity, state) : nullptr;
}

EntityVisualState getVisualStateAt(const Entity& entity, uint32_t now)
{
    if (!canAnimate(entity)) return ENTITY_VISUAL_NORMAL;
    const TransientVisualState transient = transientState(entity);
    const bool transientActive = deadlineActive(now, transientDeadline(entity));
    if (transientActive && transient == TRANSIENT_HIT)
        return ENTITY_VISUAL_HIT;
    if (deadlineActive(now, actionDeadline(entity)))
        return ENTITY_VISUAL_ACTION;
    if (transientActive && transient == TRANSIENT_WALK)
        return ENTITY_VISUAL_WALK;
    return ENTITY_VISUAL_NORMAL;
}

void markVisualChange(Entity& entity, EntityVisualState before, uint32_t now)
{
    if (before == getVisualStateAt(entity, now)) return;
    markEntityFootprintDirty(entity);
    needsRedraw = true;
}
}

const ActionSprite* getEntityWalkSprite(const Entity& entity)
{
    return getVisualSprite(entity, ENTITY_VISUAL_WALK);
}

const ActionSprite* getEntityActionSprite(const Entity& entity)
{
    return getVisualSprite(entity, ENTITY_VISUAL_ACTION);
}

const ActionSprite* getEntityHitSprite(const Entity& entity)
{
    return getVisualSprite(entity, ENTITY_VISUAL_HIT);
}

EntityVisualState getEntityVisualState(const Entity& entity)
{
    return getVisualStateAt(entity, millis());
}

const ActionSprite* getEntityVisualSprite(const Entity& entity)
{
    return getVisualSprite(entity, getEntityVisualState(entity));
}

bool isEntityActionPoseActive(const Entity& entity)
{
    return canAnimate(entity) &&
           deadlineActive(millis(), actionDeadline(entity));
}

void triggerEntityWalkPose(Entity& entity, uint32_t durationMs)
{
    if (!canAnimate(entity) || durationMs == 0) return;
    const uint32_t now = millis();
    if (transientState(entity) == TRANSIENT_HIT &&
        deadlineActive(now, transientDeadline(entity)))
        return;
    const EntityVisualState before = getVisualStateAt(entity, now);
    entity.turn.visualStateTiming =
        (entity.turn.visualStateTiming & ~(TRANSIENT_TIME_MASK | TRANSIENT_STATE_MASK)) |
        (encodeDeadline(now, durationMs) << TRANSIENT_TIME_SHIFT) |
        (static_cast<uint32_t>(TRANSIENT_WALK) << TRANSIENT_STATE_SHIFT);
    markVisualChange(entity, before, now);
}

void triggerEntityActionPose(Entity& entity)
{
    if (!canAnimate(entity)) return;
    const uint32_t now = millis();
    const EntityVisualState before = getVisualStateAt(entity, now);
    entity.turn.visualStateTiming =
        (entity.turn.visualStateTiming & ~VISUAL_TIME_MASK) |
        encodeDeadline(now, ACTION_POSE_DURATION_MS);
    markVisualChange(entity, before, now);
}

void triggerEntityHitPose(Entity& entity)
{
    if (!canAnimate(entity)) return;
    const uint32_t now = millis();
    const EntityVisualState before = getVisualStateAt(entity, now);
    entity.turn.visualStateTiming =
        (entity.turn.visualStateTiming & ~(TRANSIENT_TIME_MASK | TRANSIENT_STATE_MASK)) |
        (encodeDeadline(now, HIT_POSE_DURATION_MS) << TRANSIENT_TIME_SHIFT) |
        (static_cast<uint32_t>(TRANSIENT_HIT) << TRANSIENT_STATE_SHIFT);
    markVisualChange(entity, before, now);
}

void clearEntityActionPose(Entity& entity)
{
    entity.turn.visualStateTiming &= ~VISUAL_TIME_MASK;
}

void clearEntityVisualPoses(Entity& entity)
{
    entity.turn.visualStateTiming = 0;
}

void updateEntityActionPoses()
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    const uint32_t now = millis();
    for (uint8_t index = 0; entities != nullptr && index < entityCount; ++index)
    {
        Entity& entity = entities[index];
        const uint32_t priorTiming = entity.turn.visualStateTiming;
        if (priorTiming == 0) continue;
        if (!deadlineActive(now, actionDeadline(entity)))
            entity.turn.visualStateTiming &= ~VISUAL_TIME_MASK;
        if (!deadlineActive(now, transientDeadline(entity)))
            entity.turn.visualStateTiming &=
                ~(TRANSIENT_TIME_MASK | TRANSIENT_STATE_MASK);
        if (entity.turn.visualStateTiming == priorTiming) continue;
        markEntityFootprintDirty(entity);
        needsRedraw = true;
    }
}
