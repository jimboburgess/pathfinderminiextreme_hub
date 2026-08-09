#include "savegame.h"

#include <Preferences.h>

namespace
{
constexpr uint32_t SAVE_MAGIC = 0x50464D45; // PFME
constexpr uint8_t SAVE_VERSION = 2;
constexpr uint8_t LEGACY_SAVE_VERSION = 1;

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

// Version 1 wrote the old enum-based ItemID arrays directly. Keep this
// layout only for migration; all new saves use the compact InventorySlot
// representation above.
struct LegacyEquipmentData
{
    uint32_t equipped[NUM_EQUIPMENT_SLOTS];
};

struct LegacyInventoryData
{
    uint32_t items[MAX_INVENTORY];
    uint8_t itemCount;
};

struct LegacySavedCharacter
{
    uint32_t magic;
    uint8_t version;
    CharacterClass characterClass;
    uint8_t level;
    uint32_t xp;
    AbilityScores abilities;
    HealthData health;
    LegacyEquipmentData equipment;
    LegacyInventoryData inventory;
};

bool isValidCharacterData(CharacterClass characterClass,
                          uint8_t level,
                          const HealthData& health)
{
    return characterClass >= CLASS_FIGHTER &&
           characterClass <= CLASS_CLERIC &&
           level > 0 && health.maxHP > 0 && health.currentHP >= 0 &&
           health.currentHP <= health.maxHP;
}

bool isValidRawItemID(uint32_t rawItem, bool allowNone)
{
    if (rawItem == ITEM_NONE)
        return allowNone;

    return rawItem < ITEM_COUNT &&
           getItem(static_cast<ItemID>(rawItem)) != nullptr;
}

bool isValidInventory(const InventoryData& inventory)
{
    if (inventory.itemCount > MAX_INVENTORY)
        return false;

    for (uint8_t i = 0; i < inventory.itemCount; i++)
    {
        const InventorySlot& slot = inventory.slots[i];
        const Item* item = getItem(slot.item);

        if (slot.item == ITEM_NONE || item == nullptr ||
            slot.quantity == 0 ||
            (!item->stackable && slot.quantity != 1))
        {
            return false;
        }

        if (item->stackable)
        {
            for (uint8_t previous = 0; previous < i; previous++)
            {
                if (inventory.slots[previous].item == slot.item)
                    return false;
            }
        }
    }

    return true;
}

bool convertLegacyEquipment(const LegacyEquipmentData& source,
                            EquipmentData& destination)
{
    for (uint8_t i = 0; i < NUM_EQUIPMENT_SLOTS; i++)
    {
        if (!isValidRawItemID(source.equipped[i], true))
            return false;

        destination.equipped[i] =
            static_cast<ItemID>(source.equipped[i]);
    }

    return true;
}

bool convertLegacyInventory(const LegacyInventoryData& source,
                            InventoryData& destination)
{
    if (source.itemCount > MAX_INVENTORY)
        return false;

    clearInventory(destination);

    for (uint8_t i = 0; i < source.itemCount; i++)
    {
        if (!isValidRawItemID(source.items[i], false) ||
            !addInventoryItem(
                destination,
                static_cast<ItemID>(source.items[i])))
        {
            return false;
        }
    }

    return true;
}

bool restoreCharacter(Character& character,
                      CharacterClass characterClass,
                      uint8_t level,
                      uint32_t xp,
                      const AbilityScores& abilities,
                      const HealthData& health,
                      const EquipmentData& equipment,
                      const InventoryData& inventory)
{
    if (!isValidCharacterData(characterClass, level, health) ||
        !isValidInventory(inventory))
    {
        return false;
    }

    // The save format intentionally contains only data that can be persisted
    // safely. Rebuild the runtime-only player identity and status here.
    Character loaded = {};
    loaded.characterClass = characterClass;
    loaded.creatureType = CREATURE_PLAYER;
    loaded.team = TEAM_PLAYER;
    loaded.state = STATE_ALIVE;
    loaded.level = level;
    loaded.xp = xp;
    loaded.speed = 6;
    loaded.abilities = abilities;
    loaded.health = health;
    loaded.equipment = equipment;
    loaded.inventory = inventory;

    if (loaded.characterClass == CLASS_CLERIC)
        learnAbility(loaded, ABILITY_CHANNEL_ENERGY);

    // Class ability resources are runtime data in the current save format.
    // Reconstruct valid values rather than leaving a loaded Cleric with an
    // uninitialized feature.
    restoreClassAbilityUses(loaded);

    character = loaded;
    return true;
}
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

    size_t savedSize = preferences.getBytesLength("player");

    if (savedSize == sizeof(SavedCharacter))
    {
        SavedCharacter saved = {};
        size_t bytesRead = preferences.getBytes(
            "player", &saved, sizeof(saved));
        preferences.end();

        if (bytesRead != sizeof(saved) ||
            saved.magic != SAVE_MAGIC ||
            saved.version != SAVE_VERSION)
        {
            return false;
        }

        return restoreCharacter(
            character,
            saved.characterClass,
            saved.level,
            saved.xp,
            saved.abilities,
            saved.health,
            saved.equipment,
            saved.inventory);
    }

    if (savedSize == sizeof(LegacySavedCharacter))
    {
        LegacySavedCharacter saved = {};
        size_t bytesRead = preferences.getBytes(
            "player", &saved, sizeof(saved));
        preferences.end();

        if (bytesRead != sizeof(saved) ||
            saved.magic != SAVE_MAGIC ||
            saved.version != LEGACY_SAVE_VERSION ||
            !isValidCharacterData(
                saved.characterClass, saved.level, saved.health))
        {
            return false;
        }

        EquipmentData equipment = {};
        InventoryData inventory = {};

        if (!convertLegacyEquipment(saved.equipment, equipment) ||
            !convertLegacyInventory(saved.inventory, inventory))
        {
            return false;
        }

        return restoreCharacter(
            character,
            saved.characterClass,
            saved.level,
            saved.xp,
            saved.abilities,
            saved.health,
            equipment,
            inventory);
    }

    preferences.end();
    return false;
}
