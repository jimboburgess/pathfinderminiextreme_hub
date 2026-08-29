#include "map/skillactions.h"

#include <Arduino.h>
#include <cstdio>

#include "audio/audio.h"
#include "characters/conditions.h"
#include "data/dice.h"
#include "data/entitytraits.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
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
        case SKILL_ACROBATICS:
        {
            const MapEffect* web = getWebEffectAffectingEntity(*player);
            if (!hasCondition(player->character, CONDITION_WEBBED) ||
                web == nullptr)
            {
                setGameMessage("No Acrobatics action available.");
                return false;
            }

            if (!canUseStandardSkillAction(*player))
                return false;

            const int total = rollDie(20) +
                getSkillBonus(player->character, SKILL_ACROBATICS);
            if (total >= web->saveDC)
            {
                removeCondition(player->character, CONDITION_WEBBED);
                setGameMessage("You escape the web.");
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
    return hasCondition(entity.character, CONDITION_WEBBED) &&
        (getEquippedMeleeWeapon(entity.character) != nullptr ||
         getEquippedRangedWeapon(entity.character) != nullptr);
}

bool cutFreeFromWeb()
{
    Entity* player = getActiveMapPlayer();
    if (player == nullptr || !canCutFreeFromWeb(*player) ||
        !canUseStandardSkillAction(*player))
        return false;

    removeCondition(player->character, CONDITION_WEBBED);
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
        (melee.weaponEnhancement == WEAPON_ENHANCEMENT_FLAMING ||
         ranged.weaponEnhancement == WEAPON_ENHANCEMENT_FLAMING);
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

    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    for (uint8_t i = 0; entities != nullptr && i < count; i++)
    {
        if (mapEffectAffectsEntityAt(
                *web, entities[i], entities[i].x, entities[i].y))
            applyEnvironmentalDamage(entities[i], rollDice(2, 4));
    }

    removeWebEffect(*web);
    setGameMessage("The webs burn away!");
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
