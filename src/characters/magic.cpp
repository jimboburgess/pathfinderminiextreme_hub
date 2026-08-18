#include "characters.h"

int restoreMana(Character& character, int amount)
{
    if (amount <= 0 || character.magic.maxMP <= 0)
        return 0;

    // Treat a corrupted negative runtime value as an empty pool. Normal save
    // loading already clamps MP, but keeping this helper defensive avoids
    // signed arithmetic surprises for every caller.
    if (character.magic.currentMP < 0)
        character.magic.currentMP = 0;

    if (character.magic.currentMP >= character.magic.maxMP)
    {
        if (character.magic.currentMP > character.magic.maxMP)
            character.magic.currentMP = character.magic.maxMP;

        return 0;
    }

    const int missingMP =
        character.magic.maxMP - character.magic.currentMP;
    const int restored = amount < missingMP ? amount : missingMP;

    character.magic.currentMP += restored;
    return restored;
}
