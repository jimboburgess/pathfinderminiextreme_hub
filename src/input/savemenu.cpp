#include "savemenu.h"

#include <cstdio>

#include "audio/audio.h"
#include "characters/sheet.h"
#include "data/game.h"
#include "data/savegame.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "graphics/messagelog.h"
#include "input/buttons.h"

namespace
{
const MenuItem loadItems[] =
{
    { "Slot 1", "Load this character.", MENU_LOAD_SLOT_1,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE },
    { "Slot 2", "Load this character.", MENU_LOAD_SLOT_2,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE },
    { "Slot 3", "Load this character.", MENU_LOAD_SLOT_3,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE },
    { "Back", "Return to the title.", MENU_NONE,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE }
};

const MenuItem saveItems[] =
{
    { "Slot 1", "Save this character here.", MENU_SAVE_SLOT_1,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE },
    { "Slot 2", "Save this character here.", MENU_SAVE_SLOT_2,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE },
    { "Slot 3", "Save this character here.", MENU_SAVE_SLOT_3,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE },
    { "Back", "Return without saving.", MENU_NONE,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE }
};

const Menu loadMenu =
{
    "Load Character",
    loadItems,
    SAVE_SLOT_COUNT + 1,
    nullptr,
    nullptr,
    menuBodyScratch,
    56
};

const Menu saveMenu =
{
    "Save Game",
    saveItems,
    SAVE_SLOT_COUNT + 1,
    nullptr,
    nullptr,
    menuBodyScratch,
    56
};

const MenuItem overwriteItems[] =
{
    { "No", "Keep the existing save.", MENU_OVERWRITE_NO,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE },
    { "Yes", "Replace it with this character.", MENU_OVERWRITE_YES,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE }
};

const Menu overwriteMenus[SAVE_SLOT_COUNT] =
{
    { "Overwrite Slot 1?", overwriteItems, 2 },
    { "Overwrite Slot 2?", overwriteItems, 2 },
    { "Overwrite Slot 3?", overwriteItems, 2 }
};

void rebuildSlotBody()
{
    size_t used = 0;
    for (uint8_t slot = 0; slot < SAVE_SLOT_COUNT; slot++)
    {
        SaveSlotSummary summary = {};
        const SaveSlotState state = getSaveSlotSummary(slot, summary);
        if (state == SaveSlotState::Occupied)
        {
            used += std::snprintf(
                menuBodyScratch + used, sizeof(menuBodyScratch) - used,
                "%u %s - %s Lv %u\n", static_cast<unsigned>(slot + 1),
                summary.characterName,
                getCharacterClassName(summary.characterClass),
                static_cast<unsigned>(summary.level));
        }
        else if (state == SaveSlotState::Corrupt)
        {
            used += std::snprintf(
                menuBodyScratch + used, sizeof(menuBodyScratch) - used,
                "%u Corrupt Save\n", static_cast<unsigned>(slot + 1));
        }
        else
        {
            used += std::snprintf(
                menuBodyScratch + used, sizeof(menuBodyScratch) - used,
                "%u Empty\n", static_cast<unsigned>(slot + 1));
        }
        if (used >= sizeof(menuBodyScratch))
            break;
    }
}

void finishLoad(uint8_t slot)
{
    Character loaded = {};
    if (!loadGameFromSlot(loaded, slot))
    {
        setGameMessage("Empty or corrupt save slot.");
        playSound(SoundEffect::ERROR);
        requestMenuRedraw();
        return;
    }

    player = loaded;
    abortCombat();
    resetDungeonRun(dungeon);
    closeCharacterSheet();
    closeMenu();
    clearGameMessage();
    resetButtonStates();
    townSelection = TOWN_STAY_HOME;
    redrawType = REDRAW_FULL;
    gameState = GAME_TOWN;
    needsRedraw = true;
}

void finishSave(uint8_t slot)
{
    if (!saveGameToSlot(player, slot))
    {
        setGameMessage("Unable to save game.");
        playSound(SoundEffect::ERROR);
        requestMenuRedraw();
        return;
    }

    closeMenu();
    setGameMessage("Game saved.");
    needsRedraw = true;
}

const Menu* currentSaveMenu()
{
    return menuState.depth == 0
        ? nullptr : menuState.menuStack[menuState.depth - 1];
}

uint8_t currentOverwriteSlot()
{
    const Menu* menu = currentSaveMenu();
    for (uint8_t slot = 0; slot < SAVE_SLOT_COUNT; slot++)
    {
        if (menu == &overwriteMenus[slot])
            return slot;
    }
    return SAVE_SLOT_COUNT;
}
}

void openLoadGameMenu()
{
    rebuildSlotBody();
    openMenu(&loadMenu);
}

void openSaveGameMenu()
{
    rebuildSlotBody();
    openMenu(&saveMenu);
}

bool activateSaveMenuAction(MenuAction action)
{
    if (action == MENU_NONE &&
        (currentSaveMenu() == &loadMenu || currentSaveMenu() == &saveMenu))
    {
        closeMenu();
        return true;
    }

    if (action >= MENU_LOAD_SLOT_1 && action <= MENU_LOAD_SLOT_3)
    {
        finishLoad(static_cast<uint8_t>(action - MENU_LOAD_SLOT_1));
        return true;
    }

    if (action >= MENU_SAVE_SLOT_1 && action <= MENU_SAVE_SLOT_3)
    {
        const uint8_t slot = static_cast<uint8_t>(action - MENU_SAVE_SLOT_1);
        SaveSlotSummary summary = {};
        const SaveSlotState state = getSaveSlotSummary(slot, summary);
        if (state == SaveSlotState::Occupied ||
            state == SaveSlotState::Corrupt)
        {
            pushMenu(&overwriteMenus[slot]);
        }
        else
            finishSave(slot);
        return true;
    }

    if (action == MENU_OVERWRITE_NO)
    {
        menuCancel();
        return true;
    }

    if (action == MENU_OVERWRITE_YES)
    {
        const uint8_t slot = currentOverwriteSlot();
        if (slot < SAVE_SLOT_COUNT)
            finishSave(slot);
        return true;
    }

    return false;
}
