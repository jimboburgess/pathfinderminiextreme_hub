//
// Created by james on 7/12/2026.
//

#include "characters.h"
#include "data/progression.h"
#include "graphics/sprites.h"

const uint16_t* getPlayerSprite(CharacterClass characterClass)
{
    switch (characterClass)
    {
        case CLASS_FIGHTER:
            return fighter16x16;

        case CLASS_ROGUE:
            return rogue16x16;

        case CLASS_CLERIC:
            return cleric16x16;

        case CLASS_WIZARD:
            return wizard16x16;

        default:
            return fighter16x16;
    }
}

int getAbilityModifier(int score)
{
  if (score >= 10)
    return (score - 10) / 2;

  return (score - 11) / 2;
}

int getAbilityModifier(const Character& character, AbilityScore ability) {
  int score = 10;

  switch (ability) {
    case ABILITY_STRENGTH:
      score = character.abilities.strength;
      break;

    case ABILITY_DEXTERITY:
      score = character.abilities.dexterity;
      break;

    case ABILITY_CONSTITUTION:
      score = character.abilities.constitution;
      break;

    case ABILITY_INTELLIGENCE:
      score = character.abilities.intelligence;
      break;

    case ABILITY_WISDOM:
      score = character.abilities.wisdom;
      break;

    case ABILITY_CHARISMA:
      score = character.abilities.charisma;
      break;
  }
  return getAbilityModifier(score);
}

int getMaxHP(const Character& character)
{
  return getBaseHitPoints(
      character.characterClass,
      character.level)
      + getAbilityModifier(character, ABILITY_CONSTITUTION);
}

const Weapon* getEquippedMeleeWeapon(const Character& character)
{
    ItemID item = character.equipment.equipped[SLOT_MELEE_WEAPON];

    if (item == ITEM_NONE)
        return nullptr;

    return getWeapon(item);
}

const Weapon* getEquippedRangedWeapon(const Character& character)
{
    ItemID item = character.equipment.equipped[SLOT_RANGED_WEAPON];

    if (item == ITEM_NONE)
        return nullptr;

    return getWeapon(item);
}

const Armor* getEquippedArmor(const Character& character)
{
    ItemID item = character.equipment.equipped[SLOT_ARMOR];

    if (item == ITEM_NONE)
        return nullptr;

    return getArmor(item);
}

const Shield* getEquippedShield(const Character& character)
{
    ItemID item = character.equipment.equipped[SLOT_SHIELD];

    if (item == ITEM_NONE)
        return nullptr;

    return getShield(item);
}

int getArmorClass(const Character& character, int dodgeBonus)
{
    const Armor* armor = getEquippedArmor(character);
    const Shield* shield = getEquippedShield(character);
    int armorBonus = armor ? armor->armorBonus : 0;
    int shieldBonus = shield ? shield->shieldBonus : 0;
    int naturalArmor = 0;
    int deflectionBonus = 0;
    int sizeModifier = 0;

    return 10
         + armorBonus
         + shieldBonus
         + getAbilityModifier(character,
                              ABILITY_DEXTERITY)
         + naturalArmor
         + deflectionBonus
         + dodgeBonus
         + sizeModifier
         + getConditionArmorClassModifier(character);
}

int getMeleeAttackBonus(const Character& character)
{
    int weaponEnhancement = 0;

    return getBaseAttackBonus(
               character.characterClass,
               character.level)
         + getAbilityModifier(
               character,
               ABILITY_STRENGTH)
         + weaponEnhancement
         + getConditionAttackModifier(character);
}

int getRangedAttackBonus(const Character& character)
{
    int weaponEnhancement = 0;

    return getBaseAttackBonus(
               character.characterClass,
               character.level)
         + getAbilityModifier(
               character,
               ABILITY_DEXTERITY)
         + weaponEnhancement
         + getConditionAttackModifier(character);
}

uint8_t getSneakAttackDice(const Character& character)
{
    if (character.characterClass != CLASS_ROGUE || character.level == 0)
        return 0;

    uint8_t dice = (character.level + 1) / 2;

    return dice > 10 ? 10 : dice;
}

int getMovementSpeed(const Character& character)
{
    int movement = 30;

    // Future:
    // Armor penalties
    // Encumbrance
    // Racial bonuses
    // Magic effects

    return movement;
}

uint32_t getExperienceToNextLevel(const Character& character)
{
    if (character.level >= 20)
        return 0;

    return getExperienceForLevel(character.level + 1)
         - character.xp;
}

//==================================================
// Skills
//==================================================

SkillAptitude getSkillAptitude(CharacterClass characterClass,
                               Skill skill)
{
    switch (characterClass)
    {
        case CLASS_FIGHTER:
            switch (skill)
            {
                case SKILL_INTIMIDATE:
                    return SKILL_EXCELLENT;

                case SKILL_PERCEPTION:
                    return SKILL_GOOD;

                default:
                    return SKILL_POOR;
            }

        case CLASS_ROGUE:
            switch (skill)
            {
                case SKILL_STEALTH:
                case SKILL_ACROBATICS:
                case SKILL_DISABLE_DEVICE:
                    return SKILL_EXCELLENT;

                case SKILL_DIPLOMACY:
                case SKILL_PERCEPTION:
                    return SKILL_GOOD;

                default:
                    return SKILL_AVERAGE;
            }

        case CLASS_WIZARD:
            switch (skill)
            {
                case SKILL_PERCEPTION:
                    return SKILL_GOOD;

                default:
                    return SKILL_POOR;
            }

        case CLASS_CLERIC:
            switch (skill)
            {
                case SKILL_DIPLOMACY:
                case SKILL_PERCEPTION:
                    return SKILL_GOOD;

                default:
                    return SKILL_AVERAGE;
            }
    }

    return SKILL_POOR;
}

int getSkillRank(SkillAptitude aptitude,
                 uint8_t level)
{
    switch (aptitude)
    {
        case SKILL_NONE:
            return 0;

        case SKILL_POOR:
            return level / 4;

        case SKILL_AVERAGE:
            return level / 2;

        case SKILL_GOOD:
            return level;

        case SKILL_EXCELLENT:
            return level + 2;
    }

    return 0;
}

static AbilityScore getSkillAbility(Skill skill)
{
    switch (skill)
    {
        case SKILL_ACROBATICS:
        case SKILL_STEALTH:
            return ABILITY_DEXTERITY;

        case SKILL_DIPLOMACY:
        case SKILL_INTIMIDATE:
            return ABILITY_CHARISMA;

        case SKILL_DISABLE_DEVICE:
            return ABILITY_INTELLIGENCE;

        case SKILL_PERCEPTION:
            return ABILITY_WISDOM;
    }

    return ABILITY_INTELLIGENCE;
}

int getSkillBonus(const Character& character,
                  Skill skill)
{
    AbilityScore ability = getSkillAbility(skill);

        SkillAptitude aptitude =
        getSkillAptitude(character.characterClass,
                         skill);

    return getSkillRank(aptitude,
                        character.level)
         + getAbilityModifier(character,
                              ability);
}

//==================================================
// Saving Throws
//==================================================

int getFortitudeSave(const Character& character)
{
    return getBaseSave(
               character.characterClass,
               SAVE_FORTITUDE,
               character.level)
         + getAbilityModifier(
               character,
               ABILITY_CONSTITUTION);
}

int getReflexSave(const Character& character)
{
    return getBaseSave(
               character.characterClass,
               SAVE_REFLEX,
               character.level)
         + getAbilityModifier(
               character,
               ABILITY_DEXTERITY);
}

int getWillSave(const Character& character)
{
    return getBaseSave(
               character.characterClass,
               SAVE_WILL,
               character.level)
         + getAbilityModifier(
               character,
               ABILITY_WISDOM);
}

//==================================================
// Status
//==================================================

bool isAlive(const Character& character)
{
    return character.state == STATE_ALIVE;
}

bool isDead(const Character& character)
{
    return character.state == STATE_DEAD;
}

bool isConscious(const Character& character)
{
    return character.state == STATE_ALIVE;
}

bool canAct(const Character& character)
{
    return canCharacterAct(character);
}

bool isLootable(const Character& character)
{
    return character.state == STATE_DEAD;
}

const char* getCharacterClassName(CharacterClass characterClass)
{
    switch (characterClass)
    {
        case CLASS_FIGHTER:
            return "Fighter";

        case CLASS_ROGUE:
            return "Rogue";

        case CLASS_WIZARD:
            return "Wizard";

        case CLASS_CLERIC:
            return "Cleric";

        default:
            return "Unknown";
    }
}

//==================================================
// Inventory & Equipment
//==================================================

bool canEquip(ItemID item)
{
    const Item* itemInfo = getItem(item);

    if (itemInfo == nullptr)
        return false;

    switch (itemInfo->type)
    {
        case ITEMTYPE_WEAPON:
        case ITEMTYPE_ARMOR:
        case ITEMTYPE_SHIELD:

        case ITEMTYPE_RING:
        case ITEMTYPE_HEAD:
        case ITEMTYPE_HEADBAND:
        case ITEMTYPE_NECK:
        case ITEMTYPE_CHEST:
        case ITEMTYPE_BODY:
        case ITEMTYPE_BELT:
        case ITEMTYPE_HANDS:
        case ITEMTYPE_FEET:
        case ITEMTYPE_SHOULDERS:
        case ITEMTYPE_WRISTS:
            return true;

        default:
            return false;
    }
}

EquipmentSlot getEquipmentSlot(ItemID item)
{
    const Item* itemInfo = getItem(item);

    if (itemInfo == nullptr)
        return NUM_EQUIPMENT_SLOTS;

    switch (itemInfo->type)
    {
        case ITEMTYPE_WEAPON:
        {
            const Weapon* weapon = getWeapon(item);

            if (weapon == nullptr)
                return NUM_EQUIPMENT_SLOTS;

            return (weapon->type == WEAPON_MELEE)
                 ? SLOT_MELEE_WEAPON
                 : SLOT_RANGED_WEAPON;
        }

        case ITEMTYPE_ARMOR:
            return SLOT_ARMOR;

        case ITEMTYPE_SHIELD:
            return SLOT_SHIELD;

        default:
            return NUM_EQUIPMENT_SLOTS;
    }
}

bool equipItem(Character& character, ItemID item)
{
    if (!hasItem(character, item))
        return false;

    EquipmentSlot slot = getEquipmentSlot(item);

    if (slot == NUM_EQUIPMENT_SLOTS)
        return false;

    ItemID oldItem = character.equipment.equipped[slot];

    if (oldItem != ITEM_NONE)
    {
        if (!addItem(character, oldItem))
            return false;
    }

    character.equipment.equipped[slot] = item;

    removeItem(character, item);

    return true;
}

bool unequipItem(Character& character, EquipmentSlot slot)
{
    ItemID item = character.equipment.equipped[slot];

    if (item == ITEM_NONE)
        return false;

    if (!addItem(character, item))
        return false;

    character.equipment.equipped[slot] = ITEM_NONE;

    return true;
}

static bool isValidInventoryItem(ItemID item)
{
    return item > ITEM_NONE && item < ITEM_COUNT && getItem(item) != nullptr;
}

static bool isStackableItem(ItemID item)
{
    const Item* itemInfo = getItem(item);

    return itemInfo != nullptr && itemInfo->stackable;
}

void clearInventory(InventoryData& inventory)
{
    for (uint8_t i = 0; i < MAX_INVENTORY; i++)
    {
        inventory.slots[i].item = ITEM_NONE;
        inventory.slots[i].quantity = 0;
    }

    inventory.itemCount = 0;
}

const InventorySlot* getInventorySlot(
    const InventoryData& inventory,
    uint8_t index)
{
    return index < inventory.itemCount ? &inventory.slots[index] : nullptr;
}

InventorySlot* getInventorySlot(InventoryData& inventory, uint8_t index)
{
    return index < inventory.itemCount ? &inventory.slots[index] : nullptr;
}

uint16_t getItemQuantity(const InventoryData& inventory, ItemID item)
{
    return getItemQuantity(inventory.slots, inventory.itemCount, item);
}

uint16_t getItemQuantity(
    const InventorySlot slots[],
    uint8_t itemCount,
    ItemID item)
{
    uint16_t quantity = 0;

    for (uint8_t i = 0; i < itemCount; i++)
    {
        const InventorySlot& slot = slots[i];

        if (slot.item == item)
            quantity += slot.quantity;
    }

    return quantity;
}

bool inventoryFull(const InventoryData& inventory)
{
    return inventory.itemCount >= MAX_INVENTORY;
}

bool addInventoryItem(
    InventoryData& inventory,
    ItemID item,
    uint8_t quantity)
{
    return addItemToSlots(
        inventory.slots,
        inventory.itemCount,
        MAX_INVENTORY,
        item,
        quantity);
}

bool addItemToSlots(
    InventorySlot slots[],
    uint8_t& itemCount,
    uint8_t capacity,
    ItemID item,
    uint8_t quantity)
{
    if (!isValidInventoryItem(item) || quantity == 0 ||
        itemCount > capacity)
    {
        return false;
    }

    if (isStackableItem(item))
    {
        for (uint8_t i = 0; i < itemCount; i++)
        {
            InventorySlot& slot = slots[i];

            if (slot.item != item)
                continue;

            if (quantity > UINT8_MAX - slot.quantity)
                return false;

            slot.quantity += quantity;
            return true;
        }

        if (itemCount >= capacity)
            return false;

        InventorySlot& slot = slots[itemCount++];
        slot.item = item;
        slot.quantity = quantity;
        return true;
    }

    if (quantity > capacity - itemCount)
        return false;

    for (uint8_t i = 0; i < quantity; i++)
    {
        InventorySlot& slot = slots[itemCount++];
        slot.item = item;
        slot.quantity = 1;
    }

    return true;
}

bool removeInventoryItem(
    InventoryData& inventory,
    ItemID item,
    uint8_t quantity)
{
    return removeItemFromSlots(
        inventory.slots,
        inventory.itemCount,
        MAX_INVENTORY,
        item,
        quantity);
}

bool removeItemFromSlots(
    InventorySlot slots[],
    uint8_t& itemCount,
    uint8_t capacity,
    ItemID item,
    uint8_t quantity)
{
    if (!isValidInventoryItem(item) || quantity == 0 ||
        itemCount > capacity ||
        getItemQuantity(slots, itemCount, item) < quantity)
    {
        return false;
    }

    for (uint8_t i = 0; i < itemCount && quantity > 0;)
    {
        InventorySlot& slot = slots[i];

        if (slot.item != item)
        {
            i++;
            continue;
        }

        uint8_t removed = slot.quantity < quantity
            ? slot.quantity
            : quantity;
        slot.quantity -= removed;
        quantity -= removed;

        if (slot.quantity > 0)
        {
            i++;
            continue;
        }

        for (uint8_t j = i; j + 1 < itemCount; j++)
            slots[j] = slots[j + 1];

        itemCount--;
        slots[itemCount].item = ITEM_NONE;
        slots[itemCount].quantity = 0;
    }

    return true;
}

bool addItem(Character& character, ItemID item, uint8_t quantity)
{
    return addInventoryItem(character.inventory, item, quantity);
}

bool removeItem(Character& character, ItemID item, uint8_t quantity)
{
    return removeInventoryItem(character.inventory, item, quantity);
}

bool hasItem(const Character& character, ItemID item)
{
    return getItemQuantity(character.inventory, item) > 0;
}

uint16_t getItemQuantity(const Character& character, ItemID item)
{
    return getItemQuantity(character.inventory, item);
}

bool inventoryFull(const Character& character)
{
    return inventoryFull(character.inventory);
}

ItemID getEquippedItem(const Character& character, EquipmentSlot slot)
{
    return character.equipment.equipped[slot];
}

const char* getEquippedItemName(const Character& character,
                                EquipmentSlot slot)
{
    ItemID item = getEquippedItem(character, slot);

    if (item == ITEM_NONE)
        return "None";

    const Item* itemInfo = getItem(item);

    if (itemInfo == nullptr)
        return "Unknown";

    return itemInfo->name;
}
