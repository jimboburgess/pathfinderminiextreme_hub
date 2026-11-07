#ifndef PATHFINDERMINIEXTREME_025_ACTIONPOSE_H
#define PATHFINDERMINIEXTREME_025_ACTIONPOSE_H

#include <stdint.h>

struct Entity;

constexpr uint32_t ACTION_POSE_DURATION_MS = 1000;

enum ActionSpriteStyle : uint8_t
{
    ACTION_SPRITE_WEAPON,
    ACTION_SPRITE_RANGED,
    ACTION_SPRITE_CAST,
    ACTION_SPRITE_NATURAL
};

struct ActionSprite
{
    ActionSpriteStyle style;
    uint16_t primaryColor;
    uint16_t accentColor;
};

extern const ActionSprite fighterActionSprite;
extern const ActionSprite rogueActionSprite;
extern const ActionSprite wizardActionSprite;
extern const ActionSprite clericActionSprite;

extern const ActionSprite goblinActionSprite;
extern const ActionSprite goblinArcherActionSprite;
extern const ActionSprite bugbearActionSprite;
extern const ActionSprite skeletonActionSprite;
extern const ActionSprite zombieActionSprite;
extern const ActionSprite ghoulActionSprite;
extern const ActionSprite wightActionSprite;
extern const ActionSprite giantSpiderActionSprite;
extern const ActionSprite grayOozeActionSprite;
extern const ActionSprite violetFungusActionSprite;
extern const ActionSprite chokerActionSprite;
extern const ActionSprite spectatorActionSprite;
extern const ActionSprite skeletonMageActionSprite;
extern const ActionSprite goblinChieftainActionSprite;
extern const ActionSprite spiderQueenActionSprite;

const ActionSprite* getEntityActionSprite(const Entity& entity);
bool isEntityActionPoseActive(const Entity& entity);
void triggerEntityActionPose(Entity& entity);
void clearEntityActionPose(Entity& entity);
void updateEntityActionPoses();

#endif
