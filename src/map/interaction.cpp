//
// Created by james on 7/12/2026.
//

#include "map/interaction.h"

#include <cstdio>

#include "data/entityspawn.h"
#include "data/dice.h"
#include "data/game.h"
#include "map/dungeontools.h"
#include "map/activemap.h"
#include "dungeon/combat.h"
#include "dungeon/loot.h"
#include "dungeon/dungeon.h"
#include "dungeon/fountain.h"
#include "dungeon/npcs.h"
#include "dungeon/riddlepuzzle.h"
#include "audio/audio.h"
#include "graphics/messagelog.h"
#include "graphics/display.h"
#include "graphics/tiles.h"
#include "input/inventorymenu.h"
#include "input/menu.h"

namespace
{
Entity* lockedChest = nullptr;
HealingFountain* selectedFountain = nullptr;
Direction selectedRiddleDoorDirection = DIR_NORTH;

const MenuItem lockedChestMenuItems[] =
{
    { "Pick Lock", "Use Disable Device to pick the lock.",
      MENU_CHEST_PICK_LOCK, nullptr, MENU_CLASS_ALL },
    { "Force Open", "Use Strength to force the chest open.",
      MENU_CHEST_FORCE_OPEN, nullptr, MENU_CLASS_ALL },
    { "Back", "Leave the chest locked.",
      MENU_CHEST_BACK, nullptr, MENU_CLASS_ALL }
};

const Menu lockedChestMenu =
{
    "Locked Chest",
    lockedChestMenuItems,
    sizeof(lockedChestMenuItems) / sizeof(lockedChestMenuItems[0])
};

const MenuItem riddleDoorMenuItems[] =
{
    { "Pick Lock", "Attempt a difficult Disable Device check.",
      MENU_RIDDLE_DOOR_PICK_LOCK, nullptr, MENU_CLASS_ALL },
    { "Back", "Leave Bertram's puzzle door locked.",
      MENU_RIDDLE_DOOR_BACK, nullptr, MENU_CLASS_ALL }
};

const Menu riddleDoorMenu =
{
    "Puzzle Door",
    riddleDoorMenuItems,
    sizeof(riddleDoorMenuItems) / sizeof(riddleDoorMenuItems[0])
};

const MenuItem fountainMenuItems[] =
{
    { "Drink from Fountain", "Restore all HP and MP once.",
      MENU_FOUNTAIN_DRINK, nullptr, MENU_CLASS_ALL },
    { "Back", "Leave the fountain alone.",
      MENU_FOUNTAIN_BACK, nullptr, MENU_CLASS_ALL }
};

const Menu fountainMenu =
{
    "Healing Fountain",
    fountainMenuItems,
    sizeof(fountainMenuItems) / sizeof(fountainMenuItems[0])
};

Entity* getLockedChest()
{
    return lockedChest != nullptr && lockedChest->active &&
           lockedChest->type == ENTITY_CHEST && lockedChest->locked
        ? lockedChest
        : nullptr;
}

void drinkFromSelectedFountain()
{
    Entity* playerEntity = getActiveMapPlayer();
    if (selectedFountain == nullptr || playerEntity == nullptr)
    {
        closeMenu();
        return;
    }

    if (!drinkFromHealingFountain(*selectedFountain, playerEntity->character))
    {
        closeMenu();
        setGameMessage("The fountain's magic is spent.");
        return;
    }

    const int originX = selectedFountain->x;
    const int originY = selectedFountain->y;
    selectedFountain = nullptr;
    closeMenu();
    for (uint8_t y = 0; y < HEALING_FOUNTAIN_HEIGHT; y++)
    {
        for (uint8_t x = 0; x < HEALING_FOUNTAIN_WIDTH; x++)
            markTileDirty(originX + x, originY + y);
    }
    playSound(SoundEffect::SPELL_HEAL);
    setGameMessage("The magical water restores you!");
}

void unlockChest(Entity& chest, const char* message)
{
    chest.locked = false;
    lockedChest = nullptr;
    closeMenu();
    setGameMessage(message);
}
}

void drinkFromFountain()
{
    drinkFromSelectedFountain();
}

void pickLockedChest()
{
    Entity* chest = getLockedChest();
    Entity* playerEntity = getActiveMapPlayer();

    if (chest == nullptr || playerEntity == nullptr)
    {
        closeMenu();
        return;
    }

    const DisableDeviceToolType tool =
        getDisableDeviceTool(playerEntity->character);
    const int naturalRoll = rollDie(20);

    if (isDisableDeviceAutomaticFailure(naturalRoll))
    {
        handleDisableDeviceToolBreak(playerEntity->character, tool, naturalRoll);
        lockedChest = nullptr;
        closeMenu();

        if (tool == DISABLE_TOOL_MASTERWORK)
            setGameMessage("Your masterwork tools broke!");
        else if (tool == DISABLE_TOOL_STANDARD)
            setGameMessage("Your thieves' tools broke!");
        else
            setGameMessage("Failed to pick lock.");
        return;
    }

    const int total = naturalRoll +
        getSkillBonus(playerEntity->character, SKILL_DISABLE_DEVICE) +
        getLockDisableDeviceModifier(playerEntity->character);

    if (total >= CHEST_LOCK_DC)
        unlockChest(*chest, "Lock picked.");
    else
    {
        lockedChest = nullptr;
        closeMenu();
        setGameMessage("Failed to pick lock.");
    }
}

void forceOpenLockedChest()
{
    Entity* chest = getLockedChest();
    Entity* playerEntity = getActiveMapPlayer();

    if (chest == nullptr || playerEntity == nullptr)
    {
        closeMenu();
        return;
    }

    const int total = rollDie(20) +
        getAbilityModifier(playerEntity->character, ABILITY_STRENGTH) +
        getForceOpenToolModifier(playerEntity->character);

    if (total >= CHEST_FORCE_OPEN_DC)
        unlockChest(*chest, "Chest forced open.");
    else
    {
        lockedChest = nullptr;
        closeMenu();
        setGameMessage("Failed to force chest.");
    }
}

void pickRiddlemanDoorLock()
{
    Entity* playerEntity = getActiveMapPlayer();
    if (playerEntity == nullptr || dungeon.currentRoom >= dungeon.roomCount ||
        !isRiddlemanExitLocked(
            dungeon.rooms[dungeon.currentRoom], selectedRiddleDoorDirection))
    {
        closeMenu();
        return;
    }

    const bool showBertramReaction = noteCurrentRiddlemanBypassAttempt();
    const DisableDeviceToolType tool =
        getDisableDeviceTool(playerEntity->character);
    const int naturalRoll = rollDie(20);
    RiddlemanDoorBypassResult result = RIDDLEMAN_BYPASS_FAILED;

    if (isDisableDeviceAutomaticFailure(naturalRoll))
    {
        handleDisableDeviceToolBreak(playerEntity->character, tool, naturalRoll);
    }
    else
    {
        const int total = naturalRoll +
            getSkillBonus(playerEntity->character, SKILL_DISABLE_DEVICE) +
            getLockDisableDeviceModifier(playerEntity->character);
        result = attemptCurrentRiddlemanDoorBypass(
            selectedRiddleDoorDirection, total);
    }

    closeMenu();
    if (showBertramReaction)
        setGameMessage("Hey! What are you doing?");
    else if (result == RIDDLEMAN_BYPASS_SUCCEEDED)
        setGameMessage("Lock picked.");
    else if (isDisableDeviceAutomaticFailure(naturalRoll) &&
             tool == DISABLE_TOOL_MASTERWORK)
        setGameMessage("Your masterwork tools broke!");
    else if (isDisableDeviceAutomaticFailure(naturalRoll) &&
             tool == DISABLE_TOOL_STANDARD)
        setGameMessage("Your thieves' tools broke!");
    else
        setGameMessage("Failed to pick lock.");
}

bool tryInteractWithFacingEntity()
{
    // Looting is deliberately a post-combat map interaction. It should not
    // bypass the existing combat action economy while a fight is active.
    if (combat.active)
        return false;

    Entity* playerEntity = getActiveMapPlayer();

    if (playerEntity == nullptr || !playerEntity->active)
        return false;

    int targetX = playerEntity->x + directionOffsets[moveDirection].dx;
    int targetY = playerEntity->y + directionOffsets[moveDirection].dy;

    if (!isInsideActiveMap(targetX, targetY))
        return false;

    if (gameState == GAME_DUNGEON)
    {
        HealingFountain* fountain = getHealingFountainAt(
            dungeon.rooms[dungeon.currentRoom], targetX, targetY);
        if (fountain != nullptr)
        {
            if (fountain->used)
            {
                setGameMessage("The fountain's magic is spent.");
            }
            else
            {
                selectedFountain = fountain;
                openMenu(&fountainMenu);
            }
            return true;
        }

        Direction riddleExitDirection = DIR_NORTH;
        if (getCurrentRiddlemanExitDirectionAt(
                targetX, targetY, riddleExitDirection) &&
            isRiddlemanExitLocked(
                dungeon.rooms[dungeon.currentRoom], riddleExitDirection))
        {
            selectedRiddleDoorDirection = riddleExitDirection;
            openMenu(&riddleDoorMenu);
            return true;
        }
    }

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities == nullptr)
        return false;

    Entity* target = getEntityAt(
        entities,
        entityCount,
        static_cast<uint8_t>(targetX),
        static_cast<uint8_t>(targetY));

    if (target == nullptr)
    {
        return false;
    }

    if (target->type == ENTITY_NPC)
        return handleNPCInteraction(*target);

    if (target->type == ENTITY_CHEST)
    {
        if (target->locked)
        {
            lockedChest = target;
            openMenu(&lockedChestMenu);
            return true;
        }

        if (!target->loot.generated)
        {
            target->opened = true;
            generateChestLoot(*target, LOOT_CHEST_LARGE);
            target->sprite = chestopenwith;
            markEntityFootprintDirty(*target);
            setGameMessage("You open the chest.");
        }

        if (target->loot.itemCount == 0 && target->loot.gold == 0)
        {
            target->sprite = chestopenwithout;
            setGameMessage("The chest is empty.");
            return true;
        }

        uint16_t gold = takeCorpseGold(*target, playerEntity->character);
        if (target->loot.itemCount == 0)
        {
            finishLootingCorpse(*target);
            return true;
        }

        openCorpseLootMenu(*target);
        return true;
    }

    if (target->type != ENTITY_MONSTER || !isLootable(target->character))
        return false;

    // The generator is idempotent. This also supports an older corpse that
    // entered STATE_DEAD before the loot system was added.
    generateCorpseLoot(*target);

    uint16_t gold = takeCorpseGold(*target, playerEntity->character);

    if (gold > 0)
    {
        char message[40];
        snprintf(message, sizeof(message), "Found %u gp.",
                 static_cast<unsigned>(gold));
        setGameMessage(message);
    }

    if (target->loot.itemCount == 0)
    {
        finishLootingCorpse(*target);

        if (gold == 0)
            setGameMessage("Nothing useful remains.");

        return true;
    }

    openCorpseLootMenu(*target);
    return true;
}
