#include "map/skillactions.h"

#include <Arduino.h>
#include <cstdio>

#include "audio/audio.h"
#include "characters/conditions.h"
#include "characters/items.h"
#include "data/dice.h"
#include "data/entitytraits.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "dungeon/traps.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "map/activemap.h"
#include "map/dungeontools.h"
#include "map/awareness.h"
#include "map/mapeffects.h"

namespace
{
constexpr int TRAP_SEARCH_RANGE = 3;

bool canUseStandardSkillAction(Entity& player)
{
    return !combat.active ||
        (isPlayerTurn() && combat.waitingForPlayer &&
         canCharacterAct(player.character) &&
         !player.turn.standardActionUsed);
}

void spendStandardSkillAction(Entity& player)
{
    if (!combat.active)
        return;

    player.turn.standardActionUsed = true;
    combat.waitingForPlayer = true;
    checkEndPlayerTurn();
}

Entity* getClosestVisibleHostile(Entity& player)
{
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    Entity* best = nullptr;
    int bestDistance = 32767;

    for (uint8_t i = 0; entities != nullptr && i < count; i++)
    {
        Entity& candidate = entities[i];
        if (!candidate.active || candidate.type != ENTITY_MONSTER ||
            candidate.character.team != TEAM_MONSTER ||
            candidate.character.state != STATE_ALIVE ||
            !candidate.visibleToPlayer)
        {
            continue;
        }

        const int distance = getEntityGridDistance(player, candidate);
        if (distance < bestDistance)
        {
            best = &candidate;
            bestDistance = distance;
        }
    }

    return best;
}

bool canInspectTrapFrom(const Entity& player, const TrapInstance& trap)
{
    return getEntityGridDistanceToTile(player, trap.x, trap.y) <=
               TRAP_SEARCH_RANGE &&
           hasLineOfSightFromFootprintAt(
               player, player.x, player.y, trap.x, trap.y);
}

TrapInstance* searchNearbyTraps(Entity& player)
{
    if (gameState != GAME_DUNGEON ||
        dungeon.currentRoom >= MAX_ROOMS)
    {
        return nullptr;
    }

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    TrapInstance* discoveredTrap = nullptr;
    bool rolled = false;
    int perceptionTotal = 0;

    for (TrapInstance& trap : room.traps)
    {
        if (trap.id == TRAP_NONE || trap.discovered ||
            trap.manualPerceptionAttempted ||
            !canInspectTrapFrom(player, trap))
        {
            continue;
        }

        if (!rolled)
        {
            perceptionTotal = rollDie(20) +
                getSkillBonus(player.character, SKILL_PERCEPTION);
            rolled = true;
        }

        if (attemptManualTrapDiscovery(trap, perceptionTotal) ==
            TRAP_DISCOVERY_SUCCESS)
        {
            discoveredTrap = &trap;
            markTileDirty(trap.x, trap.y);
        }
    }

    return discoveredTrap;
}

bool usePerception(Entity& player)
{
    if (!canUseStandardSkillAction(player))
        return false;

    bool found = false;
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    for (uint8_t i = 0; entities != nullptr && i < count; i++)
        found = tryPlayerDetectMonster(player, entities[i]) || found;

    TrapInstance* foundTrap = searchNearbyTraps(player);

    // Secret-door room search belongs here when that persistent geometry
    // system is added. This single entry point prevents menu-driven rerolls
    // from bypassing its eventual per-character/per-door attempt state.
    if (foundTrap != nullptr)
    {
        const TrapDefinition* definition =
            getTrapDefinition(foundTrap->id);
        char message[64];
        snprintf(message, sizeof(message), "You discover a %s!",
                 definition != nullptr ? definition->name : "trap");
        setGameMessage(message);
    }
    else if (found)
    {
        setGameMessage("You notice a hidden creature!");
    }
    else
    {
        // A failed or exhausted search never certifies that a square is safe.
        setGameMessage(getTrapSearchFailureMessage(random(5)));
    }

    spendStandardSkillAction(player);
    return true;
}

TrapInstance* getAdjacentDiscoveredTrap(Entity& player)
{
    if (gameState != GAME_DUNGEON ||
        dungeon.currentRoom >= MAX_ROOMS)
    {
        return nullptr;
    }

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];

    // Prefer the square the player is facing, then check every adjacent tile.
    const int facingX =
        player.x + directionOffsets[moveDirection].dx;
    const int facingY =
        player.y + directionOffsets[moveDirection].dy;
    TrapInstance* trap = getTrapAt(room, facingX, facingY);
    if (trap != nullptr && trap->discovered && isTrapActive(*trap))
        return trap;

    for (uint8_t direction = 0; direction < 8; direction++)
    {
        if (direction == static_cast<uint8_t>(moveDirection))
            continue;

        const int x = player.x + directionOffsets[direction].dx;
        const int y = player.y + directionOffsets[direction].dy;
        trap = getTrapAt(room, x, y);
        if (trap != nullptr && trap->discovered && isTrapActive(*trap))
            return trap;
    }

    return nullptr;
}

bool useDisableDevice(Entity& player)
{
    if (!canUseStandardSkillAction(player))
        return false;

    TrapInstance* trap = getAdjacentDiscoveredTrap(player);
    if (trap == nullptr)
    {
        setGameMessage("No active trap is within reach.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    const DisableDeviceToolType tool =
        getDisableDeviceTool(player.character);
    const int naturalRoll = rollDie(20);

    if (isDisableDeviceAutomaticFailure(naturalRoll))
    {
        handleDisableDeviceToolBreak(player.character, tool, naturalRoll);

        if (tool == DISABLE_TOOL_MASTERWORK)
            setGameMessage("Your masterwork tools broke!");
        else if (tool == DISABLE_TOOL_STANDARD)
            setGameMessage("Your thieves' tools broke!");
        else
            setGameMessage("You fail to disable it.");

        spendStandardSkillAction(player);
        return true;
    }

    const int total = naturalRoll +
        getSkillBonus(player.character, SKILL_DISABLE_DEVICE) +
        getDisableDeviceToolModifier(tool);
    const TrapDisableResult result = attemptDisableTrap(*trap, total);

    if (result == TRAP_DISABLE_SUCCESS)
    {
        setGameMessage("Pressure plate disabled.");
        markTileDirty(trap->x, trap->y);
    }
    else
    {
        setGameMessage("You fail to disable it.");
    }

    spendStandardSkillAction(player);
    return true;
}

bool useStealth(Entity& player)
{
    if (combat.active)
    {
        setGameMessage("You cannot hide during combat.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    bool detected = false;
    for (uint8_t i = 0; entities != nullptr && i < count; i++)
    {
        if (tryMonsterDetectPlayer(entities[i], player))
        {
            detected = true;
            break;
        }
    }

    if (detected)
    {
        setGameMessage("Stealth failed!");
        startCombat();
    }
    else
    {
        setGameMessage("You move quietly.");
    }

    return true;
}

bool useIntimidate(Entity& player)
{
    if (!combat.active || !canUseStandardSkillAction(player))
    {
        setGameMessage("No Intimidate target available.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    Entity* target = getClosestVisibleHostile(player);
    if (target == nullptr)
    {
        setGameMessage("No Intimidate target available.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    const int total = rollDie(20) +
        getSkillBonus(player.character, SKILL_INTIMIDATE);
    const int dc = getIntimidateDC(*target);
    char message[64];

    if (intimidateSucceeds(total, dc) &&
        addCondition(target->character, CONDITION_FRIGHTENED, 0, 1))
    {
        snprintf(message, sizeof(message), "%s is frightened!",
                 getEntityName(target));
    }
    else
    {
        snprintf(message, sizeof(message), "Intimidate failed.");
    }

    setGameMessage(message);
    spendStandardSkillAction(player);
    return true;
}

TrapInstance* getReachableTriggeredPit(Entity& player)
{
    if (gameState != GAME_DUNGEON || dungeon.currentRoom >= MAX_ROOMS)
        return nullptr;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    TrapInstance* pit = getTriggeredCollapsingPitAt(room, player.x, player.y);
    if (pit != nullptr)
        return pit;
    for (uint8_t direction = 0; direction < 8; direction++)
    {
        const int x = player.x + directionOffsets[direction].dx;
        const int y = player.y + directionOffsets[direction].dy;
        pit = getTriggeredCollapsingPitAt(room, x, y);
        if (pit != nullptr)
            return pit;
    }
    return nullptr;
}

bool findPitExit(
    const DungeonRoom& room,
    const TrapInstance& pit,
    Entity& player,
    int& exitX,
    int& exitY)
{
    static constexpr int8_t offsets[][2] = {
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1},
        {0, -2}, {2, 0}, {0, 2}, {-2, 0},
        {-1, -2}, {1, -2}, {2, -1}, {2, 1},
        {-1, 2}, {1, 2}, {-2, -1}, {-2, 1}};
    for (const auto& offset : offsets)
    {
        const int x = pit.x + offset[0];
        const int y = pit.y + offset[1];
        if (x < 0 || y < 0 || x >= ROOM_WIDTH || y >= ROOM_HEIGHT ||
            !isDungeonFloorTerrain(room.map.tiles[y][x]) ||
            isOpenCollapsingPitTile(room, x, y))
        {
            continue;
        }
        Entity* occupant = getEntityAt(
            dungeon.entities, dungeon.entityCount,
            static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        if (occupant == nullptr || occupant == &player)
        {
            exitX = x;
            exitY = y;
            return true;
        }
    }
    return false;
}

int getPitAssistBonus(const TrapInstance& pit, const Entity& climber)
{
    int bonus = 0;
    for (uint8_t i = 0; i < dungeon.entityCount; i++)
    {
        const Entity& ally = dungeon.entities[i];
        if (!ally.active || &ally == &climber || ally.type != ENTITY_PLAYER ||
            ally.character.state != STATE_ALIVE ||
            hasCondition(ally.character, CONDITION_PIT_BOTTOM))
        {
            continue;
        }
        if (getEntityGridDistanceToTile(ally, pit.x, pit.y) <= 2)
        {
            bonus = 2;
            break;
        }
    }
    return bonus;
}

bool useClimb(Entity& player)
{
    TrapInstance* pit = getReachableTriggeredPit(player);
    if (pit == nullptr)
    {
        setGameMessage("No Climb action available.");
        return false;
    }
    if (!canUseStandardSkillAction(player))
        return false;

    if (!hasCondition(player.character, CONDITION_PIT_BOTTOM))
    {
        if (!hasItem(player.character, ITEM_ROPE) &&
            !hasItem(player.character, ITEM_GRAPPLING_HOOK))
        {
            setGameMessage("You need rope or a grappling hook.");
            return false;
        }
        lowerRopeIntoCollapsingPit(*pit, player.x, player.y);
        for (int y = pit->y - 1; y <= pit->y + 1; y++)
            for (int x = pit->x - 1; x <= pit->x + 1; x++)
                markTileDirty(x, y);
        setSharedGameMessage("A rope is lowered into the pit.");
        spendStandardSkillAction(player);
        return true;
    }

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    int dc = getCollapsingPitClimbDC(room, *pit);
    if (hasItem(player.character, ITEM_ROPE) ||
        hasItem(player.character, ITEM_GRAPPLING_HOOK))
    {
        dc -= 2;
    }
    dc -= getPitAssistBonus(*pit, player);
    if (dc < 5)
        dc = 5;
    const int total = rollDie(20) +
        getSkillBonus(player.character, SKILL_CLIMB);
    if (total < dc)
    {
        setGameMessage("You fail to climb the pit wall.");
        spendStandardSkillAction(player);
        return true;
    }

    int exitX = player.x;
    int exitY = player.y;
    if (!findPitExit(room, *pit, player, exitX, exitY))
    {
        setGameMessage("There is no safe ledge to climb onto.");
        return false;
    }
    markEntityFootprintDirty(player);
    player.x = static_cast<uint8_t>(exitX);
    player.y = static_cast<uint8_t>(exitY);
    removeCondition(player.character, CONDITION_PIT_BOTTOM);
    markEntityFootprintDirty(player);
    setSharedGameMessage("A party member climbs out of the pit.");
    spendStandardSkillAction(player);
    return true;
}

bool usePitJump(Entity& player)
{
    if (gameState != GAME_DUNGEON || dungeon.currentRoom >= MAX_ROOMS)
        return false;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    const DirectionOffset offset = directionOffsets[moveDirection];
    const int pitX = player.x + offset.dx;
    const int pitY = player.y + offset.dy;
    TrapInstance* pit = getTriggeredCollapsingPitAt(room, pitX, pitY);
    if (pit == nullptr || getCollapsingPitTile(*pit, pitX, pitY) !=
            COLLAPSING_PIT_OPEN)
    {
        return false;
    }
    const int landingX = player.x + offset.dx * 2;
    const int landingY = player.y + offset.dy * 2;
    if (landingX < 0 || landingY < 0 || landingX >= ROOM_WIDTH ||
        landingY >= ROOM_HEIGHT ||
        !isDungeonFloorTerrain(room.map.tiles[landingY][landingX]) ||
        isOpenCollapsingPitTile(room, landingX, landingY) ||
        getEntityAt(dungeon.entities, dungeon.entityCount,
                    static_cast<uint8_t>(landingX),
                    static_cast<uint8_t>(landingY)) != nullptr)
    {
        return false;
    }
    if (!canUseStandardSkillAction(player))
        return false;

    const int total = rollDie(20) +
        getSkillBonus(player.character, SKILL_ACROBATICS);
    markEntityFootprintDirty(player);
    if (total >= COLLAPSING_PIT_ACROBATICS_DC)
    {
        player.x = static_cast<uint8_t>(landingX);
        player.y = static_cast<uint8_t>(landingY);
        setGameMessage("You leap across the pit.");
    }
    else
    {
        player.x = static_cast<uint8_t>(pitX);
        player.y = static_cast<uint8_t>(pitY);
        addCondition(player.character, CONDITION_PIT_BOTTOM,
                     getCollapsingPitDepthFeet(*pit), 0);
        applyCombatDamage(
            player,
            rollDice(getCollapsingPitDamageDice(*pit), 6),
            DAMAGE_BLUDGEONING);
        setGameMessage("You miss the ledge and fall!");
    }
    markEntityFootprintDirty(player);
    spendStandardSkillAction(player);
    return true;
}
}

int getIntimidateDC(const Entity& target)
{
    return calculateIntimidateDC(
        getEffectiveHitDice(target),
        getAbilityModifier(target.character, ABILITY_WISDOM));
}

bool useSkill(Skill skill)
{
    Entity* player = getActiveMapPlayer();
    if (player == nullptr || player->character.state != STATE_ALIVE)
        return false;

    switch (skill)
    {
        case SKILL_PERCEPTION: return usePerception(*player);
        case SKILL_DISABLE_DEVICE: return useDisableDevice(*player);
        case SKILL_STEALTH: return useStealth(*player);
        case SKILL_INTIMIDATE: return useIntimidate(*player);
        case SKILL_CLIMB: return useClimb(*player);
        case SKILL_ACROBATICS:
        {
            const MapEffect* web = getWebEffectAffectingEntity(*player);
            if (!canAttemptEscapeWeb(*player) || web == nullptr)
            {
                if (usePitJump(*player))
                    return true;
                setGameMessage("No Acrobatics action available.");
                return false;
            }

            if (!canUseStandardSkillAction(*player))
                return false;

            const int total = rollDie(20) +
                getSkillBonus(player->character, SKILL_ACROBATICS);
            if (attemptEscapeWeb(*player, total))
            {
                setGameMessage("You break free of the web.");
            }
            else
            {
                setGameMessage("Escape failed.");
            }
            spendStandardSkillAction(*player);
            return true;
        }
        default:
            setGameMessage("That skill is unavailable.");
            return false;
    }
}

bool canCutFreeFromWeb(const Entity& entity)
{
    return hasCondition(entity.character, CONDITION_GRAPPLED) &&
        (getEquippedMeleeWeapon(entity.character) != nullptr ||
         getEquippedRangedWeapon(entity.character) != nullptr);
}

bool cutFreeFromWeb()
{
    Entity* player = getActiveMapPlayer();
    if (player == nullptr || !canCutFreeFromWeb(*player) ||
        !canUseStandardSkillAction(*player))
        return false;

    if (!releaseEntityFromWeb(*player))
        return false;
    setGameMessage("You cut yourself free.");
    spendStandardSkillAction(*player);
    return true;
}

bool canIgniteWeb(const Entity& entity)
{
    const ItemInstance& melee =
        entity.character.equipment.equipped[SLOT_MELEE_WEAPON];
    const ItemInstance& ranged =
        entity.character.equipment.equipped[SLOT_RANGED_WEAPON];
    return getWebEffectAffectingEntity(entity) != nullptr &&
        (hasWeaponProperty(melee, WEAPON_PROPERTY_FLAMING) ||
         hasWeaponProperty(ranged, WEAPON_PROPERTY_FLAMING));
}

bool igniteWeb()
{
    Entity* player = getActiveMapPlayer();
    if (player == nullptr || !canIgniteWeb(*player) ||
        !canUseStandardSkillAction(*player))
        return false;

    MapEffect* web = const_cast<MapEffect*>(getWebEffectAffectingEntity(*player));
    if (web == nullptr)
        return false;

    burnWebEffect(*web);
    setGameMessage("The web burns away!");
    spendStandardSkillAction(*player);
    return true;
}

SocialCheckResult resolveAutomaticSocialCheck(
    const Character& character,
    Skill skill,
    int dc)
{
    return resolveSocialCheckTotal(
        rollDie(20) + getSkillBonus(character, skill), dc);
}

const char* getShopDiplomacyMessage(SocialCheckResult result)
{
    return result == SOCIAL_FAVORABLE
        ? "The shopkeeper smiles as you walk in."
        : "The shopkeeper looks your way, waiting for you to act.";
}
