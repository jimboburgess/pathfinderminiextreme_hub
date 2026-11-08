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
#include "map/worlditems.h"
#include "dungeon/combat.h"
#include "dungeon/loot.h"
#include "dungeon/dungeon.h"
#include "dungeon/fountain.h"
#include "dungeon/roomgen.h"
#include "dungeon/npcs.h"
#include "dungeon/riddlepuzzle.h"
#include "dungeon/bellpuzzle.h"
#include "dungeon/furniture.h"
#include "dungeon/numbertilepuzzle.h"
#include "dungeon/brazierpuzzle.h"
#include "audio/audio.h"
#include "graphics/messagelog.h"
#include "graphics/display.h"
#include "graphics/tiles.h"
#include "input/inventorymenu.h"
#include "input/menu.h"
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
#include "native_party.h"
#include "native_rooms.h"
#endif

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
      MENU_RIDDLE_DOOR_PICK_LOCK, nullptr, MENU_CLASS_ROGUE },
    { "Back", "Leave the puzzle exit locked.",
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

}

void clearInteractionEntityReferences()
{
    lockedChest = nullptr;
    selectedFountain = nullptr;
    selectedRiddleDoorDirection = DIR_NORTH;
}

namespace
{

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

namespace {
bool puzzleExit(const DungeonRoom& room,Direction& direction) {
    if(room.type!=ROOM_PUZZLE||room.puzzleBypassed)return false;
    switch(room.puzzleType){
    case PUZZLE_BELLS:if(room.bellPuzzle.progress==BELL_PUZZLE_NONE||room.bellPuzzle.progress==BELL_PUZZLE_COMPLETE)return false;direction=static_cast<Direction>(room.bellPuzzle.lockedExitDirection);break;
    case PUZZLE_NUMBER_TILES:if(room.numberPuzzle.progress!=NUMBER_PUZZLE_UNSOLVED)return false;direction=static_cast<Direction>(room.numberPuzzle.lockedExitDirection);break;
    case PUZZLE_BRAZIERS:if(room.brazierPuzzle.progress!=BRAZIER_PUZZLE_UNSOLVED)return false;direction=static_cast<Direction>(room.brazierPuzzle.lockedExitDirection);break;
    case PUZZLE_RIDDLEMAN:if(room.npcSpawn.puzzleState==RIDDLE_ROOM_NONE||room.npcSpawn.puzzleState==RIDDLE_ROOM_COMPLETE)return false;direction=static_cast<Direction>(room.npcSpawn.lockedExitDirection);break;
    default:return false;
    }
    return getRoomConnection(room,direction)!=nullptr;
}
}
bool getCurrentPuzzleExitDirectionAt(int x,int y,Direction& direction) {
    if(gameState!=GAME_DUNGEON||dungeon.currentRoom>=dungeon.roomCount)return false;
    const auto& room=dungeon.rooms[dungeon.currentRoom];if(!puzzleExit(room,direction))return false;
    const auto* exit=getRoomConnection(room,direction);return exit->x==x&&exit->y==y;
}
bool canAttemptCurrentPuzzleBypass(const Entity& actor,Direction direction) {
    if(gameState!=GAME_DUNGEON||combat.active||dungeon.currentRoom>=dungeon.roomCount||
       !actor.active||actor.type!=ENTITY_PLAYER||actor.character.state!=STATE_ALIVE||
       actor.character.health.currentHP<=0||actor.character.characterClass!=CLASS_ROGUE||
       getActiveMapPlayer()!=&actor)return false;
    Direction expected=DIR_NORTH;const auto& room=dungeon.rooms[dungeon.currentRoom];
    if(!puzzleExit(room,expected)||expected!=direction)return false;
    const auto* exit=getRoomConnection(room,direction);
    return exit&&actor.x+directionOffsets[moveDirection].dx==exit->x&&
        actor.y+directionOffsets[moveDirection].dy==exit->y;
}
bool attemptCurrentPuzzleBypass(Entity& actor,Direction direction,int total) {
    if(!canAttemptCurrentPuzzleBypass(actor,direction))return false;
    if(total<getRiddlemanLockDisableDC(actor.character.level))return false;
    auto& room=dungeon.rooms[dungeon.currentRoom];
    if(room.puzzleType==PUZZLE_RIDDLEMAN&&
       attemptCurrentRiddlemanDoorBypass(direction,total)!=RIDDLEMAN_BYPASS_SUCCEEDED)return false;
    room.puzzleBypassed=true;room.completed=true;
    const auto* exit=getRoomConnection(room,direction);markTileDirty(exit->x,exit->y);
    return true;
}

void pickRiddlemanDoorLock()
{
    Entity* playerEntity = getActiveMapPlayer();
    if (playerEntity == nullptr ||
        !canAttemptCurrentPuzzleBypass(*playerEntity,selectedRiddleDoorDirection))
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
        result = attemptCurrentPuzzleBypass(*playerEntity,selectedRiddleDoorDirection,total)
            ? RIDDLEMAN_BYPASS_SUCCEEDED : RIDDLEMAN_BYPASS_FAILED;
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
    Entity* playerEntity = getActiveMapPlayer();

    if (playerEntity == nullptr || !playerEntity->active)
        return false;

    // A player standing on the physical replay button can press A again;
    // stepping off/re-entering the room is never required.
    if(!combat.active&&gameState==GAME_DUNGEON&&
       isCurrentBellListeningRuneAt(playerEntity->x,playerEntity->y))
        return handleCurrentBellListeningRuneEntry(*playerEntity,playerEntity->x,playerEntity->y);

    int targetX = playerEntity->x + directionOffsets[moveDirection].dx;
    int targetY = playerEntity->y + directionOffsets[moveDirection].dy;

    if (!isInsideActiveMap(targetX, targetY))
        return false;

    if (!combat.active && gameState == GAME_DUNGEON)
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

        if (strikeCurrentBellAt(targetX, targetY))
            return true;

        if (interactWithCurrentBrazierPuzzleAt(targetX, targetY))
            return true;

        if (interactWithCurrentNumberPuzzleClue(targetX, targetY))
            return true;

        if (isCurrentBellListeningRuneAt(targetX,targetY))
            return handleCurrentBellListeningRuneEntry(*playerEntity,targetX,targetY);

        Direction riddleExitDirection = DIR_NORTH;
        if (getCurrentPuzzleExitDirectionAt(
                targetX, targetY, riddleExitDirection) &&
            canAttemptCurrentPuzzleBypass(*playerEntity,riddleExitDirection))
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

    if (target->type == ENTITY_LOOT && isDroppedWorldItem(*target))
    {
        const InventorySlot* slot = getDroppedWorldItem(*target);
        const ItemInstance item = slot->item;
        const int dropX = target->x;
        const int dropY = target->y;
        const WorldItemResult result = pickupWorldItem(*playerEntity, *target);
        if (result != WorldItemResult::SUCCESS)
        {
            if (result == WorldItemResult::INVENTORY_FULL)
                setGameMessage("Inventory full.");
            else if (result == WorldItemResult::NOT_ACTING ||
                     result == WorldItemResult::ACTION_SPENT)
                setGameMessage("Cannot pick that up now.");
            else
                setGameMessage("That item is unavailable.");
            playSound(SoundEffect::ERROR);
            return true;
        }

        char itemName[48];
        formatItemInstanceName(item, itemName, sizeof(itemName));
        char message[96];
        snprintf(message, sizeof(message), "%s picks up %s.",
                 getCharacterName(playerEntity->character), itemName);
        markTileDirty(dropX, dropY);
        playSound(SoundEffect::ITEM_PICKUP);
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
        if (pfme::runtime::nativeRoomsActive() &&
            pfme::runtime::socialPartiesEnabled())
            pfme::runtime::nativePartyMessage(message);
        else
#endif
            setGameMessage(message);
        if (combat.active) checkEndPlayerTurn();
        return true;
    }

    if (!combat.active && target->type == ENTITY_NPC)
        return handleNPCInteraction(*target);

    if (!combat.active && target->type == ENTITY_CHEST)
    {
        if (gameState == GAME_DUNGEON &&
            isCurrentBrazierPuzzleRewardAt(targetX, targetY) &&
            isCurrentBrazierPuzzleRewardSealed())
        {
            setGameMessage("The chest is sealed by the braziers.");
            return true;
        }

        if (target->locked)
        {
            lockedChest = target;
            openMenu(&lockedChestMenu);
            return true;
        }

        if (!target->loot.generated)
        {
            target->opened = true;
            const LootSource source = dungeon.currentRoom == dungeon.treasureRoom
                ? LOOT_SOURCE_FINAL_TREASURE : LOOT_SOURCE_CHEST;
            const LootTableID chestSize = gameState == GAME_DUNGEON &&
                isCurrentBrazierPuzzleRewardAt(targetX, targetY)
                    ? LOOT_CHEST_MEDIUM : LOOT_CHEST_LARGE;
            generateChestLoot(*target, chestSize,
                              playerEntity->character.level, source);
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

    if (combat.active)
    {
        Entity* combatant = getCurrentCombatant();
        if (combatant != playerEntity || !isPlayerTurn() ||
            !combat.waitingForPlayer ||
            !canCharacterAct(playerEntity->character) ||
            playerEntity->turn.standardActionUsed)
        {
            return false;
        }

        // Searching a body is a standard action. Item selection remains in
        // the existing corpse inventory UI and the turn advances only after
        // that modal closes, so the interaction can finish atomically.
        playerEntity->turn.standardActionUsed = true;
    }

    // The generator is idempotent. This also supports an older corpse that
    // entered STATE_DEAD before the loot system was added.
    generateCorpseLoot(*target, playerEntity->character.level);

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

        if (combat.active)
            checkEndPlayerTurn();
        return true;
    }

    openCorpseLootMenu(*target);
    return true;
}

#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
#include "context_fields.h"
PFME_CONTEXT_FIELD(lockedChest);
PFME_CONTEXT_FIELD(selectedFountain);
PFME_CONTEXT_FIELD(selectedRiddleDoorDirection);
#endif
