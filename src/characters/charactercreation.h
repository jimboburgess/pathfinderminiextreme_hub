#ifndef CHARACTERCREATION_H
#define CHARACTERCREATION_H

#include "characters.h"

void createCharacter(Character &character,
                     CharacterClass characterClass,
                     WeaponGroup fighterWeaponGroup = WEAPON_GROUP_NONE);

uint8_t getTestCharacterStartingLevel(const char* name, bool enabled);

bool giveTestCharacterStartingEquipment(Character& character,
                                        uint8_t startingLevel);

uint8_t applyTestCharacterTitle(Character& character, bool enabled);

bool testCharactersEnabled();

#endif
