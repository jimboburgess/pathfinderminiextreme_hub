//
// Created by james on 8/1/2026.
//

#include "monsterscripts.h"

#include "map/activemap.h"
#include "abilityresolver.h"
#include "characters/conditions.h"
#include "characters/items.h"
#include "dungeon.h"
#include "monsters.h"
#include "map/movement.h"
#include "map/mapeffects.h"
#include "graphics/messagelog.h"
#include "combat.h"
#include "data/entityspawn.h"
#include "data/entitytraits.h"
#include "data/game.h"
#include "graphics/display.h"
#include "audio/audio.h"

namespace
{
constexpr uint8_t PATH_MAP_WIDTH = ROOM_SIZE;
constexpr uint8_t PATH_MAP_HEIGHT = ROOM_SIZE;
constexpr uint16_t PATH_NODE_COUNT =
    PATH_MAP_WIDTH * PATH_MAP_HEIGHT;
constexpr uint8_t CONTROL_CASTER_LOW_HP_PERCENT = 40;
constexpr int CONTROL_CASTER_PREFERRED_DISTANCE = 3;

struct PathNode
{
    int8_t x;
    int8_t y;
    int8_t firstStepX;
    int8_t firstStepY;
};

const int8_t pathDirections[][2] =
{
    { 0, -1 },
    { 1,  0 },
    { 0,  1 },
    {-1,  0 },
    { 1, -1 },
    { 1,  1 },
    {-1,  1 },
    {-1, -1 }
};

static MonsterScript getMonsterScript(const Entity* monster)
{
    return monster != nullptr && monster->monster != nullptr
        ? monster->monster->script
        : SCRIPT_NONE;
}

static bool isMonsterWalkableTile(TileType tile)
{
    if (gameState == GAME_FOREST)
    {
        // Forest terrain is open unless a tree occupies the square.
        return tile != TILE_TREE;
    }

    if (gameState == GAME_DUNGEON)
    {
        // Doors change rooms for the player, so monsters must not attempt
        // to traverse them.  They navigate around walls on floor tiles.
        return tile == TILE_FLOOR;
    }

    return false;
}

static int intervalDistance(
    int firstStart,
    int firstEnd,
    int secondStart,
    int secondEnd)
{
    if (firstEnd < secondStart)
        return secondStart - firstEnd;

    if (secondEnd < firstStart)
        return firstStart - secondEnd;

    return 0;
}

static int footprintDistanceAt(
    const Entity& first,
    int firstX,
    int firstY,
    const Entity& second,
    int secondX,
    int secondY)
{
    int firstRight = firstX + getEntityTileWidth(first) - 1;
    int firstBottom = firstY + getEntityTileHeight(first) - 1;
    int secondRight = secondX + getEntityTileWidth(second) - 1;
    int secondBottom = secondY + getEntityTileHeight(second) - 1;

    int horizontalDistance = intervalDistance(
        firstX, firstRight, secondX, secondRight);
    int verticalDistance = intervalDistance(
        firstY, firstBottom, secondY, secondBottom);

    return horizontalDistance > verticalDistance
        ? horizontalDistance
        : verticalDistance;
}

static bool areFootprintsAdjacentAt(
    const Entity& first,
    int firstX,
    int firstY,
    const Entity& second,
    int secondX,
    int secondY)
{
    int distance = footprintDistanceAt(
        first, firstX, firstY, second, secondX, secondY);

    if (distance == 0)
        return false;

    return distance == 1;
}

static const Weapon* getMonsterRangedWeapon(const Entity* monster)
{
    if (monster == nullptr)
        return nullptr;

    const Weapon* weapon = getEquippedRangedWeapon(monster->character);

    if (weapon != nullptr && weapon->type == WEAPON_RANGED)
        return weapon;

    // Supports creatures created before the ranged equipment slot existed.
    weapon = getEquippedMeleeWeapon(monster->character);

    return weapon != nullptr && weapon->type == WEAPON_RANGED
        ? weapon
        : nullptr;
}

static int getPreferredRangedDistance(const Entity* monster)
{
    const Weapon* weapon = getMonsterRangedWeapon(monster);

    if (weapon == nullptr || weapon->rangeIncrement == 0)
        return 1;

    // Map squares represent five feet.  Round up so an unusual range value
    // never produces a zero-square preferred distance.
    return (weapon->rangeIncrement + 4) / 5;
}

static const Ability* getSupportedMonsterAbility(
    const Entity* monster,
    bool requireAffordable)
{
    if (monster == nullptr || monster->monster == nullptr)
        return nullptr;

    for (uint8_t i = 0;
         i < sizeof(monster->monster->specialAbilities) /
                 sizeof(monster->monster->specialAbilities[0]);
         i++)
    {
        AbilityID abilityID = monster->monster->specialAbilities[i];
        const Ability* ability = getAbility(abilityID);

        if (ability == nullptr || !isAbilitySupported(abilityID))
            continue;

        if (requireAffordable &&
            monster->character.magic.currentMP < ability->mpCost)
        {
            continue;
        }

        return ability;
    }

    return nullptr;
}

static Entity* getMonsterAbilityTarget(
    Entity* monster,
    Entity* enemy,
    const Ability& ability)
{
    return ability.target == TARGET_SELF ? monster : enemy;
}

static const Ability* getValidMonsterAbility(
    Entity* monster,
    Entity* enemy)
{
    if (monster == nullptr || monster->monster == nullptr)
        return nullptr;

    for (uint8_t i = 0;
         i < sizeof(monster->monster->specialAbilities) /
                 sizeof(monster->monster->specialAbilities[0]);
         i++)
    {
        const Ability* ability = getAbility(
            monster->monster->specialAbilities[i]);

        if (ability == nullptr || !isAbilitySupported(ability->id))
            continue;

        Entity* target = getMonsterAbilityTarget(
            monster, enemy, *ability);

        if (validateAbility(*monster, target, ability->id) ==
            ABILITY_RESULT_SUCCESS)
        {
            return ability;
        }
    }

    return nullptr;
}

static bool canPerformRangedAttack(
    const Entity* monster,
    const Entity* target)
{
    return getMonsterRangedWeapon(monster) != nullptr &&
           target != nullptr &&
           canSee(*monster) &&
           target->active &&
           target->character.state == STATE_ALIVE &&
           hasLineOfSightBetweenFootprintsAt(
               *monster, monster->x, monster->y, *target);
}

static bool canTakePathStep(
    Entity* monster,
    int fromX,
    int fromY,
    int toX,
    int toY)
{
    int deltaX = toX - fromX;
    int deltaY = toY - fromY;

    if (deltaX != 0 && deltaY != 0)
    {
        // A diagonal step cannot squeeze through the corner of a tree,
        // wall, or another creature.
        if (!canMonsterMoveTo(monster, fromX + deltaX, fromY) ||
            !canMonsterMoveTo(monster, fromX, fromY + deltaY))
        {
            return false;
        }
    }

    return canMonsterMoveTo(monster, toX, toY);
}

static bool findMeleePathStep(
    Entity* monster,
    Entity* target,
    int& nextX,
    int& nextY)
{
    if (monster == nullptr || target == nullptr)
        return false;

    const int mapWidth = getActiveMapWidth();
    const int mapHeight = getActiveMapHeight();
    PathNode frontier[PATH_NODE_COUNT];
    bool visited[PATH_MAP_HEIGHT][PATH_MAP_WIDTH] = {};
    uint16_t first = 0;
    uint16_t last = 0;

    frontier[last++] = {
        static_cast<int8_t>(monster->x),
        static_cast<int8_t>(monster->y),
        static_cast<int8_t>(monster->x),
        static_cast<int8_t>(monster->y)
    };
    visited[monster->y][monster->x] = true;

    while (first < last)
    {
        PathNode current = frontier[first++];

        if (areFootprintsAdjacentAt(
                *monster, current.x, current.y,
                *target, target->x, target->y))
        {
            if (current.x == monster->x && current.y == monster->y)
                return false;

            nextX = current.firstStepX;
            nextY = current.firstStepY;
            return true;
        }

        for (uint8_t direction = 0;
             direction < sizeof(pathDirections) / sizeof(pathDirections[0]);
             direction++)
        {
            int candidateX = current.x + pathDirections[direction][0];
            int candidateY = current.y + pathDirections[direction][1];

            if (candidateX < 0 || candidateX >= mapWidth ||
                candidateY < 0 || candidateY >= mapHeight ||
                visited[candidateY][candidateX] ||
                !canTakePathStep(
                    monster, current.x, current.y,
                    candidateX, candidateY))
            {
                continue;
            }

            visited[candidateY][candidateX] = true;

            bool isFirstStep = current.x == monster->x &&
                               current.y == monster->y;

            frontier[last++] = {
                static_cast<int8_t>(candidateX),
                static_cast<int8_t>(candidateY),
                static_cast<int8_t>(
                    isFirstStep ? candidateX : current.firstStepX),
                static_cast<int8_t>(
                    isFirstStep ? candidateY : current.firstStepY)
            };
        }
    }

    return false;
}

static bool findRangedPathStep(
    Entity* monster,
    Entity* target,
    int preferredDistance,
    int& nextX,
    int& nextY)
{
    if (monster == nullptr || target == nullptr)
        return false;

    const int mapWidth = getActiveMapWidth();
    const int mapHeight = getActiveMapHeight();
    PathNode frontier[PATH_NODE_COUNT];
    bool visited[PATH_MAP_HEIGHT][PATH_MAP_WIDTH] = {};
    uint16_t first = 0;
    uint16_t last = 0;
    bool hasBestPosition = false;
    PathNode bestPosition = {};
    int bestDistanceError = 0;

    frontier[last++] = {
        static_cast<int8_t>(monster->x),
        static_cast<int8_t>(monster->y),
        static_cast<int8_t>(monster->x),
        static_cast<int8_t>(monster->y)
    };
    visited[monster->y][monster->x] = true;

    while (first < last)
    {
        PathNode current = frontier[first++];
        int distance = footprintDistanceAt(
            *monster, current.x, current.y,
            *target, target->x, target->y);
        int distanceError = abs(distance - preferredDistance);

        if (hasLineOfSightBetweenFootprintsAt(
                *monster, current.x, current.y, *target) &&
            (!hasBestPosition || distanceError < bestDistanceError))
        {
            bestPosition = current;
            bestDistanceError = distanceError;
            hasBestPosition = true;

            // Breadth-first traversal guarantees the first exact range
            // square is reached by the shortest available route.
            if (distanceError == 0)
                break;
        }

        for (uint8_t direction = 0;
             direction < sizeof(pathDirections) / sizeof(pathDirections[0]);
             direction++)
        {
            int candidateX = current.x + pathDirections[direction][0];
            int candidateY = current.y + pathDirections[direction][1];

            if (candidateX < 0 || candidateX >= mapWidth ||
                candidateY < 0 || candidateY >= mapHeight ||
                visited[candidateY][candidateX] ||
                !canTakePathStep(
                    monster, current.x, current.y,
                    candidateX, candidateY))
            {
                continue;
            }

            visited[candidateY][candidateX] = true;

            bool isFirstStep = current.x == monster->x &&
                               current.y == monster->y;

            frontier[last++] = {
                static_cast<int8_t>(candidateX),
                static_cast<int8_t>(candidateY),
                static_cast<int8_t>(
                    isFirstStep ? candidateX : current.firstStepX),
                static_cast<int8_t>(
                    isFirstStep ? candidateY : current.firstStepY)
            };
        }
    }

    if (!hasBestPosition ||
        (bestPosition.x == monster->x && bestPosition.y == monster->y))
    {
        return false;
    }

    nextX = bestPosition.firstStepX;
    nextY = bestPosition.firstStepY;
    return true;
}

static bool moveMonsterTo(Entity* monster, int newX, int newY)
{
    if (monster == nullptr ||
        (monster->x == newX && monster->y == newY) ||
        !canMonsterMoveTo(monster, newX, newY) ||
        !canAffordMovementCost(*monster, newX, newY))
    {
        return false;
    }

    int oldX = monster->x;
    int oldY = monster->y;

    markEntityFootprintDirtyAt(*monster, oldX, oldY);

    monster->x = newX;
    monster->y = newY;

    spendMovementCost(*monster, newX, newY);
    bool trapTriggered = false;
    ConditionType enteredCondition = handleEnteredTile(
        *monster, newX, newY, &trapTriggered);

    markEntityFootprintDirty(*monster);

    if (!trapTriggered)
    {
        char message[32];
        snprintf(
            message,
            sizeof(message),
            enteredCondition == CONDITION_PRONE
                ? "%s falls prone!"
                : enteredCondition == CONDITION_WEBBED
                    ? "%s is caught in the web!"
                    : "%s moves.",
            getEntityName(monster));
        setGameMessage(message);
    }

    return true;
}

static bool isControlSpellcaster(const Entity* monster)
{
    return getMonsterScript(monster) == SCRIPT_CONTROL_SPELLCASTER;
}

static bool isControlTargetImpaired(const Entity& target)
{
    return hasCondition(target.character, CONDITION_PRONE) ||
           hasCondition(target.character, CONDITION_STUNNED) ||
           hasCondition(target.character, CONDITION_BLINDED);
}

static const Ability* getControlAbility(
    const Entity* monster,
    AbilityID abilityID)
{
    if (monster == nullptr || monster->monster == nullptr ||
        !monsterHasSpecialAbility(*monster->monster, abilityID) ||
        !isAbilitySupported(abilityID))
    {
        return nullptr;
    }

    return getAbility(abilityID);
}

static bool findControlColorSprayDirection(
    const Entity& monster,
    const Entity& target,
    Direction& direction)
{
    const Ability* ability = getControlAbility(&monster, ABILITY_COLOR_SPRAY);

    if (ability == nullptr ||
        !hasLineOfSightBetweenFootprintsAt(monster, monster.x, monster.y, target))
    {
        return false;
    }

    const Direction directions[] = { DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST };

    for (Direction candidate : directions)
    {
        for (uint8_t y = target.y;
             y < target.y + getEntityTileHeight(target);
             y++)
        {
            for (uint8_t x = target.x;
                 x < target.x + getEntityTileWidth(target);
                 x++)
            {
                if (isTileInDirectionalAbilityArea(
                        monster, ability->id, candidate, x, y))
                {
                    direction = candidate;
                    return true;
                }
            }
        }
    }

    return false;
}

static AbilityResult getControlGreaseValidation(
    const Entity& monster,
    const Entity& target)
{
    const Ability* ability = getControlAbility(&monster, ABILITY_GREASE);
    return ability != nullptr
        ? validateAbilityAt(monster, target.x, target.y, ability->id)
        : ABILITY_RESULT_UNSUPPORTED;
}

static bool canControlCastGrease(const Entity& monster, const Entity& target)
{
    AbilityResult result = getControlGreaseValidation(monster, target);
    return result == ABILITY_RESULT_SUCCESS ||
           result == ABILITY_RESULT_NOT_ENOUGH_MP;
}

static bool hasControlPotion(const Entity& monster, ItemID item);

static bool canControlPrepareAbility(
    const Entity& monster,
    const Ability* ability)
{
    return ability != nullptr &&
           (monster.character.magic.currentMP >= ability->mpCost ||
            hasControlPotion(monster, ITEM_MANA_POTION));
}

static bool hasControlSpellPlan(const Entity& monster, const Entity& target)
{
    Direction direction = DIR_NORTH;
    const Ability* colorSpray = getControlAbility(
        &monster, ABILITY_COLOR_SPRAY);

    if (findControlColorSprayDirection(monster, target, direction) &&
        canControlPrepareAbility(monster, colorSpray))
    {
        return true;
    }

    const Ability* grease = getControlAbility(&monster, ABILITY_GREASE);
    return canControlCastGrease(monster, target) &&
           canControlPrepareAbility(monster, grease);
}

static bool hasControlPotion(const Entity& monster, ItemID item)
{
    return hasItem(monster.character, item);
}

static bool isControlCasterLowHealth(const Entity& monster)
{
    const HealthData& health = monster.character.health;
    return health.maxHP > 0 &&
           health.currentHP * 100 <=
               static_cast<int>(health.maxHP) * CONTROL_CASTER_LOW_HP_PERCENT;
}

static void presentMonsterPotionUse(Entity& monster, const char* message)
{
    monster.turn.standardActionUsed = true;
    setGameMessage(message);
    playSound(SoundEffect::POTION);
    markEntityFootprintDirty(monster);
}

static bool useControlHealingPotion(Entity& monster)
{
    int restored = 0;

    if (!useCureLightWoundsPotion(monster.character, restored))
        return false;

    char message[48];
    snprintf(message, sizeof(message), "%s drinks a potion (+%d HP).",
             getEntityName(&monster), restored);
    presentMonsterPotionUse(monster, message);
    return true;
}

static bool useControlManaPotion(Entity& monster)
{
    int restored = 0;
    ManaPotionUseResult result = useManaPotion(
        monster.character, makeItemInstance(ITEM_MANA_POTION), restored);

    if (result != MANA_POTION_USE_SUCCESS)
        return false;

    char message[48];
    snprintf(message, sizeof(message), "%s drinks a mana potion (+%d MP).",
             getEntityName(&monster), restored);
    presentMonsterPotionUse(monster, message);
    return true;
}

static bool tryControlColorSpray(Entity& monster, Entity& target)
{
    const Ability* ability = getControlAbility(&monster, ABILITY_COLOR_SPRAY);
    Direction direction = DIR_NORTH;

    if (ability == nullptr ||
        monster.character.magic.currentMP < ability->mpCost ||
        !findControlColorSprayDirection(monster, target, direction))
    {
        return false;
    }

    AbilityResolution resolution = resolveAbilityInDirection(
        monster, direction, ability->id);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
        return false;

    presentDirectionalAbilityResolution(monster, ability->id, resolution);
    return true;
}

static bool tryControlGrease(Entity& monster, Entity& target)
{
    const Ability* ability = getControlAbility(&monster, ABILITY_GREASE);

    if (ability == nullptr ||
        monster.character.magic.currentMP < ability->mpCost ||
        getControlGreaseValidation(monster, target) != ABILITY_RESULT_SUCCESS)
    {
        return false;
    }

    AbilityResolution resolution = resolveAbilityAt(
        monster, target.x, target.y, ability->id);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
        return false;

    presentGroundAbilityResolution(monster, ability->id, resolution);
    return true;
}

static bool moveControlCasterToSpellDistance(Entity* monster)
{
    Entity* target = chooseTarget(monster);

    if (monster == nullptr || target == nullptr)
        return false;

    int nextX = monster->x;
    int nextY = monster->y;

    return findRangedPathStep(
               monster, target, CONTROL_CASTER_PREFERRED_DISTANCE,
               nextX, nextY) &&
           moveMonsterTo(monster, nextX, nextY);
}

static bool isControlCasterReadyForAction(Entity* monster)
{
    Entity* target = chooseTarget(monster);

    if (monster == nullptr || target == nullptr)
        return true;

    if (isControlTargetImpaired(*target))
        return isAdjacent(monster, target);

    if (isAdjacent(monster, target))
        return false;

    return hasControlSpellPlan(*monster, *target);
}
}

void runMonsterScript(Entity* monster)
{
    if (monster == nullptr || monster->monster == nullptr ||
        !monster->active || !canCharacterAct(monster->character))
        return;

    switch (monster->monster->script)
    {
        case SCRIPT_RANGED:
            runRangedScript(monster);
            break;

        case SCRIPT_COWARD:
            runCowardScript(monster);
            break;

        case SCRIPT_GUARD:
            runGuardScript(monster);
            break;

        case SCRIPT_SPELLCASTER:
            runSpellcasterScript(monster);
            break;

        case SCRIPT_CONTROL_SPELLCASTER:
            runControlSpellcasterScript(monster);
            break;

        case SCRIPT_MELEE:
        default:
            runMeleeScript(monster);
            break;
    }
}

void runMeleeScript(Entity* monster)
{
    Entity* target = chooseTarget(monster);
    const Ability* web = getAbility(ABILITY_WEB);

    if (monster != nullptr && target != nullptr &&
        isImmuneToWeb(*monster) && web != nullptr &&
        monster->character.magic.currentMP >= web->mpCost &&
        !hasMapEffectAt(MAP_EFFECT_WEB, target->x, target->y) &&
        validateAbilityAt(*monster, target->x, target->y, web->id) ==
            ABILITY_RESULT_SUCCESS)
    {
        AbilityResolution resolution = resolveAbilityAt(
            *monster, target->x, target->y, web->id);
        if (resolution.result == ABILITY_RESULT_SUCCESS)
        {
            presentGroundAbilityResolution(*monster, web->id, resolution);
            return;
        }
    }

    performStandardAction(monster);
}

void runRangedScript(Entity* monster)
{
    performRangedAttack(monster);
}

void runCowardScript(Entity* monster)
{
    performStandardAction(monster);
}

void runGuardScript(Entity* monster)
{
    performStandardAction(monster);
}

void runSpellcasterScript(Entity* monster)
{
    if (monster == nullptr || monster->monster == nullptr ||
        monster->turn.standardActionUsed)
    {
        return;
    }

    Entity* enemy = chooseTarget(monster);

    if (enemy == nullptr)
        return;

    const Ability* ability = getValidMonsterAbility(monster, enemy);

    if (ability != nullptr)
    {
        Entity* target = getMonsterAbilityTarget(
            monster, enemy, *ability);
        AbilityResolution resolution = resolveAbility(
            *monster, target, ability->id);

        if (resolution.result == ABILITY_RESULT_SUCCESS)
        {
            presentAbilityResolution(
                *monster, *target, ability->id, resolution);
            return;
        }
    }

    // With no legal supported spell, retain the existing adjacent melee
    // action as the deliberately simple first-version fallback.
    performStandardAction(monster);
}

void runControlSpellcasterScript(Entity* monster)
{
    if (monster == nullptr || monster->monster == nullptr ||
        monster->turn.standardActionUsed)
    {
        return;
    }

    Entity* target = chooseTarget(monster);

    if (target == nullptr)
        return;

    // Priority 1: stay alive. A potion is a normal standard action and is
    // removed from this individual monster's normal inventory on success.
    if (isControlCasterLowHealth(*monster) &&
        hasControlPotion(*monster, ITEM_POTION_CURE_LIGHT_WOUNDS) &&
        useControlHealingPotion(*monster))
    {
        return;
    }

    // Priority 2: exploit an already-controlled player with the equipped
    // melee weapon instead of spending another control spell.
    if (isControlTargetImpaired(*target))
    {
        performStandardAction(monster);
        return;
    }

    // Priority 3: after the movement phase has tried to retreat, use the
    // short control cone when its exact shared geometry includes the player.
    Direction direction = DIR_NORTH;
    const Ability* colorSpray = getControlAbility(
        monster, ABILITY_COLOR_SPRAY);

    if (colorSpray != nullptr &&
        findControlColorSprayDirection(*monster, *target, direction))
    {
        if (monster->character.magic.currentMP < colorSpray->mpCost)
        {
            if (hasControlPotion(*monster, ITEM_MANA_POTION))
                useControlManaPotion(*monster);
            return;
        }

        if (tryControlColorSpray(*monster, *target))
            return;
    }

    // Priority 4: Grease controls a standing target outside the useful cone.
    const Ability* grease = getControlAbility(monster, ABILITY_GREASE);

    if (grease != nullptr && canControlCastGrease(*monster, *target))
    {
        if (monster->character.magic.currentMP < grease->mpCost)
        {
            if (hasControlPotion(*monster, ITEM_MANA_POTION))
                useControlManaPotion(*monster);
            return;
        }

        if (tryControlGrease(*monster, *target))
            return;
    }

    // If retreat was impossible or spells are unavailable, the normal melee
    // action remains the final fallback and uses the equipped scythe.
    performStandardAction(monster);
}

Entity* chooseTarget(Entity* monster)
{
    (void)monster;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    return entities != nullptr
        ? getPlayerEntity(entities, entityCount)
        : nullptr;
}

void performStandardAction(Entity* monster)
{
    Entity* target = chooseTarget(monster);

    if (monster == nullptr || target == nullptr ||
        monster->turn.standardActionUsed ||
        !isAdjacent(monster, target))
    {
        return;
    }

    beginMonsterAttack(monster, target, COMBAT_ATTACK_MELEE);
}

void performRangedAttack(Entity* monster)
{
    Entity* target = chooseTarget(monster);

    if (monster == nullptr || target == nullptr ||
        monster->turn.standardActionUsed ||
        !canPerformRangedAttack(monster, target))
    {
        return;
    }

    beginMonsterAttack(monster, target, COMBAT_ATTACK_RANGED);
}

bool isAdjacent(const Entity* first, const Entity* second)
{
    if (first == nullptr || second == nullptr)
        return false;

    return areFootprintsAdjacentAt(
        *first, first->x, first->y,
        *second, second->x, second->y);
}

bool canMonsterMoveTo(Entity* monster, int x, int y)
{
    if (monster == nullptr)
        return false;

    uint8_t footprintWidth = getEntityTileWidth(*monster);
    uint8_t footprintHeight = getEntityTileHeight(*monster);

    if (x < 0 || x + footprintWidth > getActiveMapWidth() ||
        y < 0 || y + footprintHeight > getActiveMapHeight())
    {
        return false;
    }

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities == nullptr)
        return false;

    for (uint8_t offsetY = 0; offsetY < footprintHeight; offsetY++)
    {
        for (uint8_t offsetX = 0; offsetX < footprintWidth; offsetX++)
        {
            int tileX = x + offsetX;
            int tileY = y + offsetY;

            if (!isMonsterWalkableTile(getActiveMapTile(tileX, tileY)))
                return false;

            for (uint8_t i = 0; i < entityCount; i++)
            {
                Entity& entity = entities[i];

                if (!entity.active || &entity == monster)
                    continue;

                if (entityOccupiesTile(entity, tileX, tileY))
                    return false;
            }
        }
    }

    return true;
}

void moveMonsterTowardsPlayer(Entity* monster)
{
    Entity* target = chooseTarget(monster);

    if (monster == nullptr || target == nullptr || isAdjacent(monster, target))
        return;

    int nextX = monster->x;
    int nextY = monster->y;

    if (findMeleePathStep(monster, target, nextX, nextY))
    {
        moveMonsterTo(monster, nextX, nextY);
    }
}

bool keepDistance(Entity* monster)
{
    Entity* target = chooseTarget(monster);

    if (monster == nullptr || target == nullptr ||
        getMonsterRangedWeapon(monster) == nullptr)
    {
        return false;
    }

    int nextX = monster->x;
    int nextY = monster->y;

    return findRangedPathStep(
               monster,
               target,
               getPreferredRangedDistance(monster),
               nextX,
               nextY) &&
           moveMonsterTo(monster, nextX, nextY);
}

static bool moveMonsterForAbility(Entity* monster)
{
    Entity* target = chooseTarget(monster);
    const Ability* ability = getSupportedMonsterAbility(monster, true);

    if (monster == nullptr || target == nullptr || ability == nullptr ||
        ability->target != TARGET_ENEMY)
    {
        return false;
    }

    int nextX = monster->x;
    int nextY = monster->y;

    return findRangedPathStep(
               monster,
               target,
               ability->rangeTiles,
               nextX,
               nextY) &&
           moveMonsterTo(monster, nextX, nextY);
}

bool isMonsterReadyForAction(Entity* monster)
{
    Entity* target = chooseTarget(monster);

    if (monster == nullptr || monster->monster == nullptr || target == nullptr)
        return true;

    const Ability* web = getAbility(ABILITY_WEB);
    if (isImmuneToWeb(*monster) && web != nullptr &&
        monster->character.magic.currentMP >= web->mpCost &&
        !hasMapEffectAt(MAP_EFFECT_WEB, target->x, target->y) &&
        validateAbilityAt(*monster, target->x, target->y, web->id) ==
            ABILITY_RESULT_SUCCESS)
    {
        return true;
    }

    if (getMonsterScript(monster) == SCRIPT_RANGED)
    {
        return canPerformRangedAttack(monster, target) &&
               footprintDistanceAt(
                   *monster, monster->x, monster->y,
                   *target, target->x, target->y) ==
                   getPreferredRangedDistance(monster);
    }

    if (getMonsterScript(monster) == SCRIPT_SPELLCASTER)
    {
        if (getValidMonsterAbility(monster, target) != nullptr)
            return true;

        // Out of MP or unable to establish a cast: become ready for the
        // existing melee fallback only after closing to adjacency.
        return isAdjacent(monster, target);
    }

    if (isControlSpellcaster(monster))
        return isControlCasterReadyForAction(monster);

    return isAdjacent(monster, target);
}

void performMovementPhase(Entity* monster)
{
    if (monster == nullptr || !monster->active ||
        !canCharacterAct(monster->character) ||
        monster->turn.movementRemaining == 0)
    {
        return;
    }

    StandForMovementResult standResult = tryStandForMovement(
        *monster, true);

    if (standResult == STAND_COMPLETED)
    {
        char message[32];
        snprintf(message, sizeof(message), "%s stands up.",
                 getEntityName(monster));
        setGameMessage(message);
        markEntityFootprintDirty(*monster);
        return;
    }

    if (standResult == STAND_NO_MOVEMENT ||
        isMonsterReadyForAction(monster))
    {
        return;
    }

    int oldX = monster->x;
    int oldY = monster->y;

    if (isControlSpellcaster(monster))
    {
        Entity* target = chooseTarget(monster);

        if (target != nullptr && isControlTargetImpaired(*target))
            moveMonsterTowardsPlayer(monster);
        else if (target != nullptr && isAdjacent(monster, target))
            moveControlCasterToSpellDistance(monster);
        else if (target != nullptr &&
                 !hasControlSpellPlan(*monster, *target))
            moveMonsterTowardsPlayer(monster);
        else
            moveControlCasterToSpellDistance(monster);
    }
    else if (getMonsterScript(monster) == SCRIPT_RANGED)
    {
        keepDistance(monster);
    }
    else if (getMonsterScript(monster) == SCRIPT_SPELLCASTER &&
             getSupportedMonsterAbility(monster, true) != nullptr)
    {
        moveMonsterForAbility(monster);
    }
    else
    {
        moveMonsterTowardsPlayer(monster);
    }

    if (monster->x == oldX && monster->y == oldY)
    {
        // No route is available.  Stop moving so the monster can take any
        // legal action instead of consuming a full turn bumping a wall.
        monster->turn.movementRemaining = 0;
        return;
    }

}
