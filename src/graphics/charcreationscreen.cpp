#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
#include "native_rooms.h"
#endif
//
// Created by james on 7/14/2026.
//

#include "charcreationscreen.h"
#include "characters/sheet.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "dungeon/dungeon.h"
#include "dungeon/combat.h"
#include "characters/charactercreation.h"
#include "config.h"
#include "data/game.h"
#include "input/buttons.h"

static const char *classNames[] =
{
    "Fighter",
    "Rogue",
    "Wizard",
    "Cleric"
};

extern Adafruit_ST7789 tft;

static CharacterCreationModel creation;

void enterCharacterCreation()
{
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    if (!pfme::runtime::nativeRoomsActive())
#endif
    {
        abortCombat();
        resetDungeonRun(dungeon);
    }
    creation.enter();
    resetButtonStates();
    needsRedraw = true;
}

static void drawWeaponStyleSelection()
{
    const WeaponGroup styles[] =
    {
        WEAPON_GROUP_BLADES,
        WEAPON_GROUP_AXES,
        WEAPON_GROUP_BLUDGEONS
    };

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 15);
    tft.println("Choose Weapon Style");

    for (uint8_t i = 0; i < 3; i++)
    {
        tft.setCursor(30, 75 + i * 40);
        tft.print(styles[i] == creation.selectedWeaponStyle ? "> " : "  ");
        tft.println(getWeaponGroupName(styles[i]));
    }

    tft.setTextSize(1);
    tft.setCursor(15, 220);
    tft.print("A/Encoder = Select  B = Back");
}

static void drawClassSelection() {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);

    tft.setCursor(30, 15);
    tft.println("Choose Class");

    for (int i = 0; i < 4; i++) {
        tft.setCursor(40, 60 + i * 35);

        if (i == creation.selectedClass)
            tft.print("> ");
        else
            tft.print("  ");

        tft.println(classNames[i]);
    }

    tft.setTextSize(1);

    tft.setCursor(15, 220);
    tft.print("Encoder = Select");
}

static void drawNameEntry()
{
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(28, 18);
    tft.println("Name Your Hero");

    tft.drawRect(18, 60, 204, 42, ST77XX_WHITE);
    tft.setCursor(28, 73);
    tft.print(getCharacterName(creation.preview));

    tft.setTextSize(1);
    tft.setCursor(28, 112);
    tft.print(hasCharacterName(creation.preview)
        ? "Current name" : "Empty uses Adventurer");

    tft.setTextSize(4);
    tft.setCursor(104, 140);
    tft.print(creation.currentNameCharacter());

    tft.setTextSize(1);
    tft.setCursor(8, 205);
    tft.print("Turn=Character  A=Add  B=Backspace");
    tft.setCursor(48, 220);
    tft.print("Encoder Press=Done");
}

static void drawPreview()
{
    drawCharacterSheet();
}

static void drawCharacterMenu()
{
    tft.fillRect(30, 30, 180, 150, ST77XX_BLUE);

    tft.drawRect(30, 30, 180, 150, ST77XX_WHITE);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);

    const char* options[] =
    {
        "Accept",
        "Reroll",
        "Change Class",
        "Cancel"
    };

    for (int i = 0; i < 4; i++)
    {
        tft.setCursor(45, 50 + i * 30);

        if (i == creation.selectedMenu)
            tft.print("> ");
        else
            tft.print("  ");

        tft.println(options[i]);
    }
}



CharacterCreationState getCharacterCreationState()
{
    return creation.creationState;
}


bool isShowingPreview()
{
    return creation.showingPreview;
}

bool isCharacterMenuOpen()
{
    return creation.creationState == CCS_MENU;
}


void drawCharacterCreationScreen()
{
    if (creation.creationState == CCS_NAME_ENTRY)
    {
        drawNameEntry();
    }
    else if (creation.creationState == CCS_WEAPON_STYLE_SELECT)
    {
        drawWeaponStyleSelection();
    }
    else if (creation.showingPreview)
    {
        drawPreview();

        if (creation.creationState == CCS_MENU)
            drawCharacterMenu();
    }
    else
    {
        drawClassSelection();
    }
}

void rotateFighterWeaponStyleCW()
{ if(creation.creationState!=CCS_WEAPON_STYLE_SELECT)return; creation.rotateStyle(true); needsRedraw=true; }

void rotateFighterWeaponStyleCCW()
{ if(creation.creationState!=CCS_WEAPON_STYLE_SELECT)return; creation.rotateStyle(false); needsRedraw=true; }

void cancelFighterWeaponStyleSelection()
{
    if (creation.creationState != CCS_WEAPON_STYLE_SELECT)
        return;
    creation.creationState = CCS_CLASS_SELECT;
    needsRedraw = true;
}

//======================================================
// Rotate Class Selection
//======================================================

void rotateCharacterClassCW() { creation.rotateClass(true); if(!creation.showingPreview)needsRedraw=true; }

void rotateCharacterClassCCW() { creation.rotateClass(false); if(!creation.showingPreview)needsRedraw=true; }

//======================================================
// Create Preview Character
//======================================================

void createPreviewCharacter()
{
    if(creation.createPreview())enterCharacterSheet(&creation.preview);
    needsRedraw=true;
}

//======================================================
// Reroll Character
//======================================================

void rerollCharacter() { if(!creation.showingPreview)return; creation.reroll(); needsRedraw=true; }

//======================================================
// Accept Character
//======================================================

void acceptCharacter() {
    if(!creation.accept(player))return;
    resetButtonStates();
    gameState=GAME_TOWN;
    needsRedraw=true;
}

void beginCharacterNameEntry()
{
    creation.beginNameEntry();
    suppressMenuInputUntilRelease();
    needsRedraw = true;
}

void rotateCharacterNameCW()
{
    creation.rotateNameCharacter(true);
    needsRedraw = true;
}

void rotateCharacterNameCCW()
{
    creation.rotateNameCharacter(false);
    needsRedraw = true;
}

void appendSelectedCharacterNameCharacter()
{
    creation.appendSelectedNameCharacter();
    needsRedraw = true;
}

bool appendCharacterNameCharacter(char character)
{
    const bool appended = creation.appendNameCharacter(character);
    if (appended)
        needsRedraw = true;
    return appended;
}

void backspaceCharacterName()
{
    if (!creation.backspaceName())
        creation.cancelNameEntry();
    needsRedraw = true;
}

void cancelCharacterNameEntry()
{
    creation.cancelNameEntry();
    needsRedraw = true;
}

bool isCharacterNameEntryActive()
{
    return creation.creationState == CCS_NAME_ENTRY;
}

void openCharacterMenu()
{
    Serial.println("Open Menu");

    creation.creationState = CCS_MENU;
    creation.selectedMenu = MENU_ACCEPT;
    suppressMenuInputUntilRelease();
    needsRedraw = true;
}

void closeCharacterMenu()
{
    Serial.println("Close Menu");

    creation.creationState = CCS_VIEW_STATS;
    needsRedraw = true;
}

void menuUp()
{
    if (creation.creationState != CCS_MENU)
        return;

    if (creation.selectedMenu == MENU_ACCEPT)
        creation.selectedMenu = MENU_CANCEL;
    else
        creation.selectedMenu =
            (CharacterCreationMenuOption)(creation.selectedMenu - 1);

    drawCharacterMenu();
}

void menuDown()
{
    if (creation.creationState != CCS_MENU)
        return;

    creation.selectedMenu =
        (CharacterCreationMenuOption)((creation.selectedMenu + 1) % 4);

    drawCharacterMenu();
}

void menuSelect()
{
    Serial.print("Menu Select: ");
    Serial.println(creation.selectedMenu);

    if (creation.creationState != CCS_MENU)
        return;

    switch (creation.selectedMenu)
    {
        case MENU_ACCEPT:
            beginCharacterNameEntry();
            break;

        case MENU_REROLL:
            rerollCharacter();
            closeCharacterMenu();
            break;

        case MENU_CHANGE_CLASS:
            creation.showingPreview = false;
            creation.selectedWeaponStyle = WEAPON_GROUP_BLADES;
            creation.creationState = CCS_CLASS_SELECT;
            needsRedraw = true;
            break;

        case MENU_CANCEL:
            closeCharacterMenu();
            break;
    }

    needsRedraw = true;
}

#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
#include "context_fields.h"
namespace pfme { namespace runtime { CharacterCreationModel& productionCreationState() {return creation;} }}
#endif
