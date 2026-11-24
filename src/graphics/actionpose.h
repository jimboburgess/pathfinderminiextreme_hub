#ifndef PATHFINDERMINIEXTREME_025_ACTIONPOSE_H
#define PATHFINDERMINIEXTREME_025_ACTIONPOSE_H

#include <stdint.h>

struct Entity;

constexpr uint32_t ACTION_POSE_DURATION_MS = 1000;
constexpr uint32_t HIT_POSE_DURATION_MS = 300;
constexpr uint32_t PLAYER_WALK_POSE_DURATION_MS = 150;
constexpr uint32_t MONSTER_WALK_POSE_DURATION_MS = 200;

enum EntityVisualState : uint8_t
{
    ENTITY_VISUAL_NORMAL,
    ENTITY_VISUAL_WALK,
    ENTITY_VISUAL_ACTION,
    ENTITY_VISUAL_HIT
};

enum ActionSpriteStyle : uint8_t
{
    ACTION_SPRITE_WEAPON,
    ACTION_SPRITE_RANGED,
    ACTION_SPRITE_CAST,
    ACTION_SPRITE_NATURAL,
    WALK_SPRITE_HUMANOID,
    WALK_SPRITE_LIGHT,
    WALK_SPRITE_ROBE,
    WALK_SPRITE_SPIDER,
    WALK_SPRITE_OOZE,
    WALK_SPRITE_FLOAT,
    HIT_SPRITE_RECOIL,
    HIT_SPRITE_CROUCH,
    HIT_SPRITE_COMPRESS
};

struct ActionSprite
{
    ActionSpriteStyle style;
    uint16_t primaryColor;
    uint16_t accentColor;
};

extern const ActionSprite fighterWalkSprite;
extern const ActionSprite fighterActionSprite;
extern const ActionSprite fighterHitSprite;
extern const ActionSprite rogueWalkSprite;
extern const ActionSprite rogueActionSprite;
extern const ActionSprite rogueHitSprite;
extern const ActionSprite wizardWalkSprite;
extern const ActionSprite wizardActionSprite;
extern const ActionSprite wizardHitSprite;
extern const ActionSprite clericWalkSprite;
extern const ActionSprite clericActionSprite;
extern const ActionSprite clericHitSprite;

extern const ActionSprite goblinWalkSprite;
extern const ActionSprite goblinActionSprite;
extern const ActionSprite goblinHitSprite;
extern const ActionSprite goblinArcherWalkSprite;
extern const ActionSprite goblinArcherActionSprite;
extern const ActionSprite goblinArcherHitSprite;
extern const ActionSprite bugbearWalkSprite;
extern const ActionSprite bugbearActionSprite;
extern const ActionSprite bugbearHitSprite;
extern const ActionSprite skeletonWalkSprite;
extern const ActionSprite skeletonActionSprite;
extern const ActionSprite skeletonHitSprite;
extern const ActionSprite zombieWalkSprite;
extern const ActionSprite zombieActionSprite;
extern const ActionSprite zombieHitSprite;
extern const ActionSprite ghoulWalkSprite;
extern const ActionSprite ghoulActionSprite;
extern const ActionSprite ghoulHitSprite;
extern const ActionSprite wightWalkSprite;
extern const ActionSprite wightActionSprite;
extern const ActionSprite wightHitSprite;
extern const ActionSprite giantSpiderWalkSprite;
extern const ActionSprite giantSpiderActionSprite;
extern const ActionSprite giantSpiderHitSprite;
extern const ActionSprite grayOozeWalkSprite;
extern const ActionSprite grayOozeActionSprite;
extern const ActionSprite grayOozeHitSprite;
extern const ActionSprite violetFungusActionSprite;
extern const ActionSprite violetFungusHitSprite;
extern const ActionSprite chokerWalkSprite;
extern const ActionSprite chokerActionSprite;
extern const ActionSprite chokerHitSprite;
extern const ActionSprite spectatorWalkSprite;
extern const ActionSprite spectatorActionSprite;
extern const ActionSprite spectatorHitSprite;
extern const ActionSprite skeletonMageWalkSprite;
extern const ActionSprite skeletonMageActionSprite;
extern const ActionSprite skeletonMageHitSprite;
extern const ActionSprite goblinChieftainWalkSprite;
extern const ActionSprite goblinChieftainActionSprite;
extern const ActionSprite goblinChieftainHitSprite;
extern const ActionSprite spiderQueenWalkSprite;
extern const ActionSprite spiderQueenActionSprite;
extern const ActionSprite spiderQueenHitSprite;

const ActionSprite* getEntityWalkSprite(const Entity& entity);
const ActionSprite* getEntityActionSprite(const Entity& entity);
const ActionSprite* getEntityHitSprite(const Entity& entity);
const ActionSprite* getEntityVisualSprite(const Entity& entity);
EntityVisualState getEntityVisualState(const Entity& entity);
bool isEntityActionPoseActive(const Entity& entity);
void triggerEntityWalkPose(Entity& entity,
                           uint32_t durationMs = PLAYER_WALK_POSE_DURATION_MS);
void triggerEntityActionPose(Entity& entity);
void triggerEntityHitPose(Entity& entity);
void clearEntityActionPose(Entity& entity);
void clearEntityVisualPoses(Entity& entity);
void updateEntityActionPoses();

#endif
