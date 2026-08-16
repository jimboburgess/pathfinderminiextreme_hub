//
// Created by james on 8/1/2026.
//

#include "monsterscripts.h"

#include "activemap.h"
#include "abilityresolver.h"
#include "dungeon.h"
#include "monsters.h"
#include "graphics/messagelog.h"
#include "combat.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "graphics/display.h"

namespace
{
constexpr uint8_t PATH_MAP_WIDTH = ROOM_SIZE;
constexpr uint8_t PATH_MAP_HEIGHT = ROOM_SIZE;
constexpr uint16_t PATH_NODE_COUNT =
    PATH_MAP_WIDTH * PATH_MAP_HEIGHT;

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
        !canMonsterMoveTo(monster, newX, newY))
    {
        return false;
    }

    int oldX = monster->x;
    int oldY = monster->y;

    markEntityFootprintDirtyAt(*monster, oldX, oldY);

    monster->x = newX;
    monster->y = newY;

    markEntityFootprintDirty(*monster);

    char message[32];
    snprintf(message, sizeof(message), "%s moves.", getEntityName(monster));
    setGameMessage(message);

    return true;
}
}

void runMonsterScript(Entity* monster)
{
    if (monster == nullptr || monster->monster == nullptr)
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

        case SCRIPT_MELEE:
        default:
            runMeleeScript(monster);
            break;
    }
}

void runMeleeScript(Entity* monster)
{
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

    return isAdjacent(monster, target);
}

void performMovementPhase(Entity* monster)
{
    if (monster == nullptr || monster->turn.movementRemaining == 0 ||
        isMonsterReadyForAction(monster))
    {
        return;
    }

    int oldX = monster->x;
    int oldY = monster->y;

    if (getMonsterScript(monster) == SCRIPT_RANGED)
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

    monster->turn.movementRemaining--;
}
