#include "savegame.h"

#include <Preferences.h>

#include "progression.h"

namespace
{
constexpr uint32_t SAVE_MAGIC = 0x50464D45; // PFME
constexpr uint8_t SAVE_VERSION = 5;
constexpr uint8_t PREVIOUS_SAVE_VERSION = 4;
constexpr uint8_t ITEM_INSTANCE_SAVE_VERSION = 3;
constexpr uint8_t ITEM_SLOT_SAVE_VERSION = 2;
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
    int16_t currentMP;
};

// Version 4 introduced persistent gold but predates persisted current MP.
// Preserve the exact previous layout for backward-compatible loading.
struct PreviousSavedCharacter
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

static_assert(
    sizeof(SavedCharacter) != sizeof(PreviousSavedCharacter),
    "Current MP must produce a distinct save layout.");

// Version 3 introduced ItemInstance storage but predates persistent gold.
struct ItemInstanceInventoryData
{
    InventorySlot slots[MAX_INVENTORY];
    uint8_t itemCount;
};

struct ItemInstanceSavedCharacter
{
    uint32_t magic;
    uint8_t version;
    CharacterClass characterClass;
    uint8_t level;
    uint32_t xp;
    AbilityScores abilities;
    HealthData health;
    EquipmentData equipment;
    ItemInstanceInventoryData inventory;
};

// Version 2 stored compact ItemID-based equipment and inventory slots.
// Preserve its exact layout so ordinary existing saves can be migrated to
// ItemInstance values.
struct ItemSlotEquipmentData
{
    ItemID equipped[NUM_EQUIPMENT_SLOTS];
};

struct ItemSlotInventorySlot
{
    ItemID item;
    uint8_t quantity;
};

static_assert(sizeof(ItemSlotInventorySlot) == 2,
              "Version 2 inventory slot layout changed.");

struct ItemSlotInventoryData
{
    ItemSlotInventorySlot slots[MAX_INVENTORY];
    uint8_t itemCount;
};

struct ItemSlotSavedCharacter
{
    uint32_t magic;
    uint8_t version;
    CharacterClass characterClass;
    uint8_t level;
    uint32_t xp;
    AbilityScores abilities;
    HealthData health;
    ItemSlotEquipmentData equipment;
    ItemSlotInventoryData inventory;
};

// Version 1 wrote the old enum-based ItemID arrays directly. Keep this
// layout only for migration.
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

static_assert(
    sizeof(SavedCharacter) != sizeof(ItemInstanceSavedCharacter) &&
    sizeof(SavedCharacter) != sizeof(ItemSlotSavedCharacter) &&
    sizeof(SavedCharacter) != sizeof(LegacySavedCharacter),
    "Current save layout must remain distinguishable from legacy layouts.");

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

bool isValidItemInstanceData(const ItemInstance& item, bool allowNone)
{
    if (item.itemID == ITEM_NONE)
    {
        return allowNone && item.enhancementBonus == 0 &&
               item.weaponEnhancement == WEAPON_ENHANCEMENT_NONE;
    }

    return isValidRawItemID(item.itemID, false) &&
           item.weaponEnhancement >= WEAPON_ENHANCEMENT_NONE &&
           item.weaponEnhancement <= WEAPON_ENHANCEMENT_SHOCK;
}

bool isValidEquipment(const EquipmentData& equipment)
{
    for (uint8_t i = 0; i < NUM_EQUIPMENT_SLOTS; i++)
    {
        if (!isValidItemInstanceData(equipment.equipped[i], true))
            return false;
    }

    return true;
}

bool isValidInventory(const InventoryData& inventory)
{
    if (inventory.itemCount > MAX_INVENTORY)
        return false;

    for (uint8_t i = 0; i < inventory.itemCount; i++)
    {
        const InventorySlot& slot = inventory.slots[i];
        const Item* item = getItem(slot.item.itemID);

        if (!isValidItemInstanceData(slot.item, false) || item == nullptr ||
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

        destination.equipped[i] = makeItemInstance(
            static_cast<ItemID>(source.equipped[i]));
    }

    return true;
}

bool convertItemSlotEquipment(const ItemSlotEquipmentData& source,
                              EquipmentData& destination)
{
    for (uint8_t i = 0; i < NUM_EQUIPMENT_SLOTS; i++)
    {
        if (!isValidRawItemID(source.equipped[i], true))
            return false;

        destination.equipped[i] = makeItemInstance(source.equipped[i]);
    }

    return true;
}

bool convertItemSlotInventory(const ItemSlotInventoryData& source,
                              InventoryData& destination)
{
    if (source.itemCount > MAX_INVENTORY)
        return false;

    clearInventory(destination);

    for (uint8_t i = 0; i < source.itemCount; i++)
    {
        const ItemSlotInventorySlot& slot = source.slots[i];
        const Item* item = getItem(slot.item);

        if (!isValidRawItemID(slot.item, false) || item == nullptr ||
            slot.quantity == 0 ||
            (!item->stackable && slot.quantity != 1) ||
            !addInventoryItem(destination, slot.item, slot.quantity))
        {
            return false;
        }
    }

    return true;
}

bool convertItemInstanceInventory(
    const ItemInstanceInventoryData& source,
    InventoryData& destination)
{
    if (source.itemCount > MAX_INVENTORY)
        return false;

    clearInventory(destination);

    for (uint8_t i = 0; i < source.itemCount; i++)
    {
        const InventorySlot& slot = source.slots[i];

        if (!isValidItemInstanceData(slot.item, false) ||
            slot.quantity == 0 ||
            !addInventoryItem(destination, slot.item, slot.quantity))
        {
            return false;
        }
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
                      const InventoryData& inventory,
                      bool hasSavedCurrentMP = false,
                      int savedCurrentMP = 0)
{
    if (!isValidCharacterData(characterClass, level, health) ||
        !isValidEquipment(equipment) ||
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

    // Max MP and known spells are class/level-derived. Version 5 also restores
    // spent MP; older saves safely migrate with a full derived pool.
    if (hasSavedCurrentMP)
        restoreCharacterMagic(loaded, savedCurrentMP);
    else
        initializeCharacterMagic(loaded);

    if (loaded.characterClass == CLASS_FIGHTER)
        learnAbility(loaded, ABILITY_POWER_ATTACK);
    else if (loaded.characterClass == CLASS_CLERIC)
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
        character.inventory,
        static_cast<int16_t>(clampCurrentMPForCharacter(
            character, character.magic.currentMP))
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
            saved.inventory,
            true,
            saved.currentMP);
    }

    if (savedSize == sizeof(PreviousSavedCharacter))
    {
        PreviousSavedCharacter saved = {};
        size_t bytesRead = preferences.getBytes(
            "player", &saved, sizeof(saved));
        preferences.end();

        if (bytesRead != sizeof(saved) ||
            saved.magic != SAVE_MAGIC ||
            saved.version != PREVIOUS_SAVE_VERSION)
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

    if (savedSize == sizeof(ItemInstanceSavedCharacter))
    {
        ItemInstanceSavedCharacter saved = {};
        size_t bytesRead = preferences.getBytes(
            "player", &saved, sizeof(saved));
        preferences.end();

        if (bytesRead != sizeof(saved) ||
            saved.magic != SAVE_MAGIC ||
            saved.version != ITEM_INSTANCE_SAVE_VERSION ||
            !isValidCharacterData(
                saved.characterClass, saved.level, saved.health))
        {
            return false;
        }

        InventoryData inventory = {};

        if (!convertItemInstanceInventory(saved.inventory, inventory))
            return false;

        return restoreCharacter(
            character,
            saved.characterClass,
            saved.level,
            saved.xp,
            saved.abilities,
            saved.health,
            saved.equipment,
            inventory);
    }

    if (savedSize == sizeof(ItemSlotSavedCharacter))
    {
        ItemSlotSavedCharacter saved = {};
        size_t bytesRead = preferences.getBytes(
            "player", &saved, sizeof(saved));
        preferences.end();

        if (bytesRead != sizeof(saved) ||
            saved.magic != SAVE_MAGIC ||
            saved.version != ITEM_SLOT_SAVE_VERSION ||
            !isValidCharacterData(
                saved.characterClass, saved.level, saved.health))
        {
            return false;
        }

        EquipmentData equipment = {};
        InventoryData inventory = {};

        if (!convertItemSlotEquipment(saved.equipment, equipment) ||
            !convertItemSlotInventory(saved.inventory, inventory))
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
