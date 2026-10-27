#ifndef CHARACTERCREATIONSCREEN_H
#define CHARACTERCREATIONSCREEN_H

#include "characters/creation_state.h"

CharacterCreationState getCharacterCreationState();

void enterCharacterCreation();

void drawCharacterCreationScreen();
void redrawCharacterMenu();

// Called from buttons.cpp
void rotateCharacterClassCW();
void rotateCharacterClassCCW();
void rotateFighterWeaponStyleCW();
void rotateFighterWeaponStyleCCW();
void cancelFighterWeaponStyleSelection();

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
