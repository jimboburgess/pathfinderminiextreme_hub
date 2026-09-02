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

static Character previewCharacter;
static CharacterClass selectedClass = CLASS_FIGHTER;
static WeaponGroup selectedWeaponStyle = WEAPON_GROUP_BLADES;

static bool showingPreview = false;

static CharacterCreationState creationState = CCS_CLASS_SELECT;
static CharacterCreationMenuOption selectedMenu = MENU_ACCEPT;

void enterCharacterCreation()
{
    abortCombat();
    resetDungeonRun(dungeon);
    selectedClass = CLASS_FIGHTER;
    selectedWeaponStyle = WEAPON_GROUP_BLADES;
    showingPreview = false;
    creationState = CCS_CLASS_SELECT;
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
        tft.print(styles[i] == selectedWeaponStyle ? "> " : "  ");
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

        if (i == selectedClass)
            tft.print("> ");
        else
            tft.print("  ");

        tft.println(classNames[i]);
    }

    tft.setTextSize(1);

    tft.setCursor(15, 220);
    tft.print("Encoder = Select");
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

        if (i == selectedMenu)
            tft.print("> ");
        else
            tft.print("  ");

        tft.println(options[i]);
    }
}



CharacterCreationState getCharacterCreationState()
{
    return creationState;
}


bool isShowingPreview()
{
    return showingPreview;
}

bool isCharacterMenuOpen()
{
    return creationState == CCS_MENU;
}


void drawCharacterCreationScreen()
{
    if (creationState == CCS_WEAPON_STYLE_SELECT)
    {
        drawWeaponStyleSelection();
    }
    else if (showingPreview)
    {
        drawPreview();

        if (creationState == CCS_MENU)
            drawCharacterMenu();
    }
    else
    {
        drawClassSelection();
    }
}

void rotateFighterWeaponStyleCW()
{
    if (creationState != CCS_WEAPON_STYLE_SELECT)
        return;

    if (selectedWeaponStyle == WEAPON_GROUP_BLADES)
        selectedWeaponStyle = WEAPON_GROUP_AXES;
    else if (selectedWeaponStyle == WEAPON_GROUP_AXES)
        selectedWeaponStyle = WEAPON_GROUP_BLUDGEONS;
    else
        selectedWeaponStyle = WEAPON_GROUP_BLADES;
    needsRedraw = true;
}

void rotateFighterWeaponStyleCCW()
{
    if (creationState != CCS_WEAPON_STYLE_SELECT)
        return;

    if (selectedWeaponStyle == WEAPON_GROUP_BLADES)
        selectedWeaponStyle = WEAPON_GROUP_BLUDGEONS;
    else if (selectedWeaponStyle == WEAPON_GROUP_BLUDGEONS)
        selectedWeaponStyle = WEAPON_GROUP_AXES;
    else
        selectedWeaponStyle = WEAPON_GROUP_BLADES;
    needsRedraw = true;
}

void cancelFighterWeaponStyleSelection()
{
    if (creationState != CCS_WEAPON_STYLE_SELECT)
        return;
    creationState = CCS_CLASS_SELECT;
    needsRedraw = true;
}

//======================================================
// Rotate Class Selection
//======================================================

void rotateCharacterClassCW() {
    if (showingPreview)
        return;

    selectedClass = (CharacterClass) ((selectedClass + 1) % 4);

    needsRedraw = true;
}

void rotateCharacterClassCCW() {
    if (showingPreview)
        return;

    if (selectedClass == CLASS_FIGHTER) {
        selectedClass = CLASS_CLERIC;
    } else {
        selectedClass = (CharacterClass) (selectedClass - 1);
    }

    needsRedraw = true;
}

//======================================================
// Create Preview Character
//======================================================

void createPreviewCharacter()
{
    if (selectedClass == CLASS_FIGHTER &&
        creationState == CCS_CLASS_SELECT)
    {
        selectedWeaponStyle = WEAPON_GROUP_BLADES;
        creationState = CCS_WEAPON_STYLE_SELECT;
        needsRedraw = true;
        return;
    }

    createCharacter(previewCharacter, selectedClass, selectedWeaponStyle);

    enterCharacterSheet(&previewCharacter);

    showingPreview = true;

    creationState = CCS_VIEW_STATS;

    needsRedraw = true;
}

//======================================================
// Reroll Character
//======================================================

void rerollCharacter() {
    if (!showingPreview)
        return;

    createCharacter(previewCharacter, selectedClass, selectedWeaponStyle);

    needsRedraw = true;
}

//======================================================
// Accept Character
//======================================================

void acceptCharacter() {
    if (!showingPreview)
        return;

    // Copy preview into the real player
    player = previewCharacter;

    // Continue to town
    resetButtonStates();
    gameState = GAME_TOWN;

    showingPreview = false;

    needsRedraw = true;
}

void openCharacterMenu()
{
    Serial.println("Open Menu");

    creationState = CCS_MENU;
    selectedMenu = MENU_ACCEPT;
    suppressMenuInputUntilRelease();
    needsRedraw = true;
}

void closeCharacterMenu()
{
    Serial.println("Close Menu");

    creationState = CCS_VIEW_STATS;
    needsRedraw = true;
}

void menuUp()
{
    if (creationState != CCS_MENU)
        return;

    if (selectedMenu == MENU_ACCEPT)
        selectedMenu = MENU_CANCEL;
    else
        selectedMenu =
            (CharacterCreationMenuOption)(selectedMenu - 1);

    drawCharacterMenu();
}

void menuDown()
{
    if (creationState != CCS_MENU)
        return;

    selectedMenu =
        (CharacterCreationMenuOption)((selectedMenu + 1) % 4);

    drawCharacterMenu();
}

void menuSelect()
{
    Serial.print("Menu Select: ");
    Serial.println(selectedMenu);

    if (creationState != CCS_MENU)
        return;

    switch (selectedMenu)
    {
        case MENU_ACCEPT:
            acceptCharacter();
            break;

        case MENU_REROLL:
            rerollCharacter();
            closeCharacterMenu();
            break;

        case MENU_CHANGE_CLASS:
            showingPreview = false;
            selectedWeaponStyle = WEAPON_GROUP_BLADES;
            creationState = CCS_CLASS_SELECT;
            needsRedraw = true;
            break;

        case MENU_CANCEL:
            closeCharacterMenu();
            break;
    }

    needsRedraw = true;
}
