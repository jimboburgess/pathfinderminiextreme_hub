#ifndef PATHFINDERMINIEXTREME_025_SAVEGAME_H
#define PATHFINDERMINIEXTREME_025_SAVEGAME_H

#include "characters/characters.h"

constexpr uint8_t SAVE_SLOT_COUNT = 3;

enum class SaveSlotState : uint8_t
{
    Empty,
    Occupied,
    Corrupt
};

struct SaveSlotSummary
{
    SaveSlotState state = SaveSlotState::Empty;
    char characterName[CHARACTER_NAME_SIZE] = {};
    CharacterClass characterClass = CLASS_FIGHTER;
    uint8_t level = 0;
};

bool saveGameToSlot(const Character& character, uint8_t slot);
bool loadGameFromSlot(Character& character, uint8_t slot);
SaveSlotState getSaveSlotSummary(uint8_t slot, SaveSlotSummary& summary);
bool deleteSaveSlot(uint8_t slot);

// Compatibility wrappers use Slot 1.
bool saveGame(const Character& character);
bool loadGame(Character& character);

#endif // PATHFINDERMINIEXTREME_025_SAVEGAME_H
