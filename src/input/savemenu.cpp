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
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
#include "character_saves.h"
#include "native_party.h"
#endif

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
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    { "New Character", "Create another character; save current first.", MENU_NEW_SAVED_CHARACTER,
      nullptr, MENU_CLASS_ALL, ABILITY_NONE },
#endif
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    { "Back", "Return without loading.", MENU_NONE,
#else
    { "Back", "Return to the title.", MENU_NONE,
#endif
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

#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
constexpr int slotBodyHeight=72;
const MenuItem loadConfirmItems[]={
    {"No","Keep playing this character.",MENU_OVERWRITE_NO,nullptr,MENU_CLASS_ALL,ABILITY_NONE},
    {"Yes","Save current, then load selected.",MENU_OVERWRITE_YES,nullptr,MENU_CLASS_ALL,ABILITY_NONE}
};
const MenuItem newCharacterItems[]={
    {"No","Keep playing this character.",MENU_OVERWRITE_NO,nullptr,MENU_CLASS_ALL,ABILITY_NONE},
    {"Yes","Save current and create another.",MENU_OVERWRITE_YES,nullptr,MENU_CLASS_ALL,ABILITY_NONE}
};
const Menu newCharacterMenu={"New character?",newCharacterItems,2};
const Menu loadConfirmMenus[SAVE_SLOT_COUNT]={
    {"Load Slot 1?",loadConfirmItems,2},{"Load Slot 2?",loadConfirmItems,2},{"Load Slot 3?",loadConfirmItems,2}
};
#else
constexpr int slotBodyHeight=56;
#endif
const Menu loadMenu =
{
    "Load Character",
    loadItems,
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    SAVE_SLOT_COUNT + 2,
#else
    SAVE_SLOT_COUNT + 1,
#endif
    nullptr,
    nullptr,
    menuBodyScratch,
    slotBodyHeight
};

const Menu saveMenu =
{
    "Save Game",
    saveItems,
    SAVE_SLOT_COUNT + 1,
    nullptr,
    nullptr,
    menuBodyScratch,
    slotBodyHeight
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
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    if(gameState==GAME_TOWN)
        used=std::snprintf(menuBodyScratch,sizeof(menuBodyScratch),"Current: %s Lv %u\n",player.name,static_cast<unsigned>(player.level));
#endif
    for (uint8_t slot = 0; slot < SAVE_SLOT_COUNT; slot++)
    {
        SaveSlotSummary summary = {};
        const SaveSlotState state = getSaveSlotSummary(slot, summary);
        if (state == SaveSlotState::Occupied)
        {
            used += std::snprintf(
                menuBodyScratch + used, sizeof(menuBodyScratch) - used,
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
                "%u %s Lv %u\n", static_cast<unsigned>(slot + 1),
                summary.characterName, static_cast<unsigned>(summary.level));
#else
                "%u %s - %s Lv %u\n", static_cast<unsigned>(slot + 1),
                summary.characterName,
                getCharacterClassName(summary.characterClass),
                static_cast<unsigned>(summary.level));
#endif
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
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    if(pfme::runtime::accountCharactersEnabled()){
        if(const char* error=pfme::runtime::loadAccountCharacter(slot,loaded)){
            setGameMessage(error);playSound(SoundEffect::ERROR);requestMenuRedraw();return;
        }
    }else
#endif
    if (!loadGameFromSlot(loaded, slot))
    {
        setGameMessage("Empty or corrupt save slot.");
        playSound(SoundEffect::ERROR);
        requestMenuRedraw();
        return;
    }

    player = loaded;
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    if(!pfme::runtime::accountCharactersEnabled())
#endif
    {
        abortCombat();
        resetDungeonRun(dungeon);
    }
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
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    if(pfme::runtime::accountCharactersEnabled()){
        if(const char* error=pfme::runtime::saveAccountCharacter(slot)){
            setGameMessage(error);playSound(SoundEffect::ERROR);requestMenuRedraw();return;
        }
    }else
#endif
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
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    if(action==MENU_NEW_SAVED_CHARACTER){
        if(pfme::runtime::accountCharactersEnabled()&&gameState==GAME_TOWN)pushMenu(&newCharacterMenu);
        else {setGameMessage("Create new from the title.");playSound(SoundEffect::ERROR);}
        return true;
    }
    if(action==MENU_OVERWRITE_YES&&currentSaveMenu()==&newCharacterMenu){
        if(const char* error=pfme::runtime::newAccountCharacter()){
            setGameMessage(error);playSound(SoundEffect::ERROR);requestMenuRedraw();
        }
        return true;
    }
#endif
    if (action == MENU_NONE &&
        (currentSaveMenu() == &loadMenu || currentSaveMenu() == &saveMenu))
    {
        closeMenu();
        return true;
    }

    if (action >= MENU_LOAD_SLOT_1 && action <= MENU_LOAD_SLOT_3)
    {
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
        const auto slot=static_cast<uint8_t>(action-MENU_LOAD_SLOT_1);
        if(pfme::runtime::accountCharactersEnabled()&&gameState==GAME_TOWN)pushMenu(&loadConfirmMenus[slot]);
        else finishLoad(slot);
#else
        finishLoad(static_cast<uint8_t>(action - MENU_LOAD_SLOT_1));
#endif
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
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
        for(uint8_t slot=0;slot<SAVE_SLOT_COUNT;++slot)if(currentSaveMenu()==&loadConfirmMenus[slot]){
            finishLoad(slot);return true;
        }
#endif
        const uint8_t slot = currentOverwriteSlot();
        if (slot < SAVE_SLOT_COUNT)
            finishSave(slot);
        return true;
    }

    return false;
}
