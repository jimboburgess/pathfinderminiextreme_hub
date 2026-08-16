//
// Created by james on 7/13/2026.
//
//
// Created by james on 7/13/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_PROGRESSION_H
#define PATHFINDERMINIEXTREME_025_PROGRESSION_H

#include "progression.h"
#include <Arduino.h>
#include "../characters/characters.h"

//======================================
// Progression
//======================================

enum SaveType
{
    SAVE_FORTITUDE,
    SAVE_REFLEX,
    SAVE_WILL
};

constexpr uint8_t MAX_CHARACTER_LEVEL = 20;

int getBaseSave(
    CharacterClass characterClass,
    SaveType saveType,
    uint8_t level);

uint32_t getExperienceForLevel(uint8_t level);

uint8_t getLevelForExperience(uint32_t experience);

bool canLevelUp(const Character& character);

// Adds XP and applies every newly crossed class level in order. The return
// value is the number of levels gained, which lets gameplay code present
// one-shot feedback without coupling character progression to the UI.
uint8_t awardExperience(Character& character, uint32_t amount);

int getBaseAttackBonus(CharacterClass characterClass, uint8_t level);

int getBaseHitPoints(CharacterClass characterClass, uint8_t level);

int getFortitudeSave(CharacterClass characterClass, uint8_t level);

int getReflexSave(CharacterClass characterClass, uint8_t level);

int getWillSave(CharacterClass characterClass, uint8_t level);

int getAbilityModifier(int score);

#endif

