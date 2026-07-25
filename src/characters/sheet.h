//
// Created by james on 7/16/2026.
//

#ifndef CHARACTER_SHEET_H
#define CHARACTER_SHEET_H

#include "characters.h"
#include "data/game.h"

void enterCharacterSheet(Character* character);

void scrollCharacterSheetUp();
void scrollCharacterSheetDown();

void updateCharacterSheet();
void drawCharacterSheet();

void openCharacterSheet();
void closeCharacterSheet();
bool isCharacterSheetVisible();



#endif