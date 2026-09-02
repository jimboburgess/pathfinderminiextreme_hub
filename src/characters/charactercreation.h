#ifndef CHARACTERCREATION_H
#define CHARACTERCREATION_H

#include "characters.h"

void createCharacter(Character &character,
                     CharacterClass characterClass,
                     WeaponGroup fighterWeaponGroup = WEAPON_GROUP_NONE);

#endif
