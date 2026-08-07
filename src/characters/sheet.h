//
// Created by james on 7/16/2026.
//

#ifndef CHARACTER_SHEET_H
#define CHARACTER_SHEET_H

#include "characters.h"
#include "data/game.h"

enum CharacterView
{
    CHARACTER_VIEW_SHEET,
    CHARACTER_VIEW_INVENTORY,
    CHARACTER_VIEW_EQUIPMENT,
    CHARACTER_VIEW_SKILLS,
    CHARACTER_VIEW_QUESTS
};

void enterCharacterSheet(Character* character);

void scrollCharacterSheetUp();
void scrollCharacterSheetDown();

void updateCharacterSheet();
void drawCharacterSheet();

void openCharacterSheet();
void openCharacterView(CharacterView view);
void closeCharacterSheet();
bool isCharacterSheetVisible();



#endif
