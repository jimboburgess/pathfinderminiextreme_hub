#include "savegame.h"

#include <Preferences.h>

namespace
{
constexpr uint32_t SAVE_MAGIC = 0x50464D45; // PFME
constexpr uint8_t SAVE_VERSION = 1;

struct SavedCharacter
{
    uint32_t magic;
    uint8_t version;
    CharacterClass characterClass;
    uint8_t level;
    uint32_t xp;
    AbilityScores abilities;
    HealthData health;
    EquipmentData equipment;
    InventoryData inventory;
};
}

bool saveGame(const Character& character)
{
    SavedCharacter saved =
    {
        SAVE_MAGIC,
        SAVE_VERSION,
        character.characterClass,
        character.level,
        character.xp,
        character.abilities,
        character.health,
        character.equipment,
        character.inventory
    };

    Preferences preferences;

    if (!preferences.begin("pathfinder", false))
        return false;

    size_t bytesWritten = preferences.putBytes(
        "player", &saved, sizeof(saved));
    preferences.end();

    return bytesWritten == sizeof(saved);
}

bool loadGame(Character& character)
{
    Preferences preferences;

    if (!preferences.begin("pathfinder", true))
        return false;

    if (preferences.getBytesLength("player") != sizeof(SavedCharacter))
    {
        preferences.end();
        return false;
    }

    SavedCharacter saved = {};
    size_t bytesRead = preferences.getBytes(
        "player", &saved, sizeof(saved));
    preferences.end();

    if (bytesRead != sizeof(saved) ||
        saved.magic != SAVE_MAGIC ||
        saved.version != SAVE_VERSION ||
        saved.characterClass < CLASS_FIGHTER ||
        saved.characterClass > CLASS_CLERIC ||
        saved.level == 0 ||
        saved.health.maxHP <= 0 ||
        saved.health.currentHP > saved.health.maxHP ||
        saved.inventory.itemCount > MAX_INVENTORY)
    {
        return false;
    }

    // The save format intentionally contains only data that can be persisted
    // safely. Rebuild the runtime-only player identity and status here.
    Character loaded = {};
    loaded.characterClass = saved.characterClass;
    loaded.creatureType = CREATURE_PLAYER;
    loaded.team = TEAM_PLAYER;
    loaded.state = STATE_ALIVE;
    loaded.level = saved.level;
    loaded.xp = saved.xp;
    loaded.speed = 6;
    loaded.abilities = saved.abilities;
    loaded.health = saved.health;
    loaded.equipment = saved.equipment;
    loaded.inventory = saved.inventory;

    character = loaded;
    return true;
}
