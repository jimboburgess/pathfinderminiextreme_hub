#include "actionpose.h"

#include <Arduino.h>

#include "data/entities.h"
#include "graphics/display.h"
#include "map/activemap.h"

const ActionSprite fighterActionSprite =
    { ACTION_SPRITE_WEAPON, 0xFFE0, 0xFFFF };
const ActionSprite rogueActionSprite =
    { ACTION_SPRITE_WEAPON, 0xC618, 0xFFFF };
const ActionSprite wizardActionSprite =
    { ACTION_SPRITE_CAST, 0xF81F, 0x07FF };
const ActionSprite clericActionSprite =
    { ACTION_SPRITE_CAST, 0xFFE0, 0xFFFF };

const ActionSprite goblinActionSprite =
    { ACTION_SPRITE_WEAPON, 0xFFE0, 0xC618 };
const ActionSprite goblinArcherActionSprite =
    { ACTION_SPRITE_RANGED, 0xC618, 0xFFE0 };
const ActionSprite bugbearActionSprite =
    { ACTION_SPRITE_WEAPON, 0xFBE0, 0xC618 };
const ActionSprite skeletonActionSprite =
    { ACTION_SPRITE_WEAPON, 0xFFFF, 0xC618 };
const ActionSprite zombieActionSprite =
    { ACTION_SPRITE_NATURAL, 0xAFE5, 0x7BE0 };
const ActionSprite ghoulActionSprite =
    { ACTION_SPRITE_NATURAL, 0xD69A, 0xFFFF };
const ActionSprite wightActionSprite =
    { ACTION_SPRITE_NATURAL, 0xA81F, 0xF81F };
const ActionSprite giantSpiderActionSprite =
    { ACTION_SPRITE_NATURAL, 0xFFFF, 0xF800 };
const ActionSprite grayOozeActionSprite =
    { ACTION_SPRITE_NATURAL, 0x7DFF, 0x07FF };
const ActionSprite violetFungusActionSprite =
    { ACTION_SPRITE_NATURAL, 0xF81F, 0xFFFF };
const ActionSprite chokerActionSprite =
    { ACTION_SPRITE_NATURAL, 0xFFE0, 0xFD20 };
const ActionSprite spectatorActionSprite =
    { ACTION_SPRITE_CAST, 0xF81F, 0x07FF };
const ActionSprite skeletonMageActionSprite =
    { ACTION_SPRITE_CAST, 0xA81F, 0x07FF };
const ActionSprite goblinChieftainActionSprite =
    { ACTION_SPRITE_WEAPON, 0xF800, 0xFFE0 };
const ActionSprite spiderQueenActionSprite =
    { ACTION_SPRITE_NATURAL, 0xF81F, 0xF800 };

namespace
{
constexpr uint32_t ACTION_POSE_MARKER = 0x80000000u;
constexpr uint32_t ACTION_POSE_TIME_MASK = 0x7FFFFFFFu;

bool timeHasNotReached(uint32_t now, uint32_t encodedDeadline)
{
    if ((encodedDeadline & ACTION_POSE_MARKER) == 0) return false;
    const uint32_t deadline = encodedDeadline & ACTION_POSE_TIME_MASK;
    const uint32_t difference =
        (deadline - (now & ACTION_POSE_TIME_MASK)) & ACTION_POSE_TIME_MASK;
    return difference != 0 && difference < 0x40000000u;
}
}

const ActionSprite* getEntityActionSprite(const Entity& entity)
{
    if (entity.type == ENTITY_MONSTER && entity.monster != nullptr &&
        entity.monster->actionSprite != nullptr)
        return entity.monster->actionSprite;

    if (entity.type != ENTITY_PLAYER)
        return nullptr;

    switch (entity.character.characterClass)
    {
        case CLASS_FIGHTER: return &fighterActionSprite;
        case CLASS_ROGUE: return &rogueActionSprite;
        case CLASS_WIZARD: return &wizardActionSprite;
        case CLASS_CLERIC: return &clericActionSprite;
        default: return &fighterActionSprite;
    }
}

bool isEntityActionPoseActive(const Entity& entity)
{
    return entity.active && entity.character.state == STATE_ALIVE &&
           getEntityActionSprite(entity) != nullptr &&
           timeHasNotReached(millis(), entity.actionPoseUntil);
}

void triggerEntityActionPose(Entity& entity)
{
    if (!entity.active || entity.character.state != STATE_ALIVE ||
        getEntityActionSprite(entity) == nullptr)
        return;

    uint32_t deadline =
        (millis() + ACTION_POSE_DURATION_MS) & ACTION_POSE_TIME_MASK;
    if (deadline == 0) deadline = 1;
    entity.actionPoseUntil = ACTION_POSE_MARKER | deadline;
    markEntityFootprintDirty(entity);
    needsRedraw = true;
}

void clearEntityActionPose(Entity& entity)
{
    entity.actionPoseUntil = 0;
}

void updateEntityActionPoses()
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    const uint32_t now = millis();
    for (uint8_t index = 0; entities != nullptr && index < entityCount;
         ++index)
    {
        Entity& entity = entities[index];
        if (entity.actionPoseUntil == 0 ||
            timeHasNotReached(now, entity.actionPoseUntil))
            continue;
        entity.actionPoseUntil = 0;
        markEntityFootprintDirty(entity);
        needsRedraw = true;
    }
}
