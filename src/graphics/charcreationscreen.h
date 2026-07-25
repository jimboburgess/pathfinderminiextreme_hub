#ifndef CHARACTERCREATIONSCREEN_H
#define CHARACTERCREATIONSCREEN_H

#include "characters/characters.h"

enum CharacterCreationState
{
    CCS_CLASS_SELECT,
    CCS_VIEW_STATS,
    CCS_MENU
};

enum CharacterCreationMenuOption
{
    MENU_ACCEPT,
    MENU_REROLL,
    MENU_CHANGE_CLASS,
    MENU_CANCEL
};

CharacterCreationState getCharacterCreationState();

void enterCharacterCreation();

void drawCharacterCreationScreen();
void redrawCharacterMenu();

// Called from buttons.cpp
void rotateCharacterClassCW();
void rotateCharacterClassCCW();

void createPreviewCharacter();
void rerollCharacter();
void acceptCharacter();

bool isShowingPreview();
bool isCharacterMenuOpen();

static void drawCharacterMenu();
void openCharacterMenu();
void closeCharacterMenu();

void menuUp();
void menuDown();
void menuSelect();




#endif