//
// Created by james on 7/16/2026.
//

#include "sheet.h"
#include "characters/conditions.h"
#include "graphics/display.h"
#include "graphics/sprites.h"
#include "data/game.h"
#include "data/entityspawn.h"
#include "dungeon/dungeon.h"
#include "dungeon/combat.h"
#include "forest/forest.h"
#include "input/buttons.h"
#include <cstdio>
#include <cstring>


static Character* currentCharacter = nullptr;
static int scrollOffset = 0;
static CharacterView characterView = CHARACTER_VIEW_SHEET;
enum EquipmentEditState { EQUIPMENT_SELECT_SLOT, EQUIPMENT_SELECT_ITEM };
static EquipmentEditState equipmentEditState = EQUIPMENT_SELECT_SLOT;
static uint8_t equipmentSlotCursor = 0;
static uint8_t equipmentItemCursor = 0;
static char equipmentStatus[48] = {};
static const EquipmentSlot editableEquipmentSlots[] =
{
    SLOT_MELEE_WEAPON, SLOT_RANGED_WEAPON, SLOT_ARMOR, SLOT_SHIELD
};

static void resetEquipmentEditing()
{
    equipmentEditState = EQUIPMENT_SELECT_SLOT;
    equipmentSlotCursor = 0;
    equipmentItemCursor = 0;
    equipmentStatus[0] = '\0';
}

static void setEquipmentStatus(const char* message)
{
    snprintf(equipmentStatus, sizeof(equipmentStatus), "%s", message);
}

static uint8_t getCompatibleInventoryCount(EquipmentSlot slot);

static void drawText(int x, int y, const char* text)
{
    y -= scrollOffset;

    if (y < -12 || y > 240)
        return;

    tft.setCursor(x, y);
    tft.print(text);
}

static void drawValue(int x, int y, int value)
{
    y -= scrollOffset;

    if (y < -12 || y > 240)
        return;

    tft.setCursor(x, y);
    tft.print(value);
}

static void drawDivider(int y)
{
    y -= scrollOffset;

    if (y < 0 || y > 240)
        return;

    tft.drawFastHLine(0, y, 240, ST77XX_WHITE);
}

static void drawLabelValue(int labelX,
                           int valueX,
                           int y,
                           const char* label,
                           int value)
{
    drawText(labelX, y, label);
    drawValue(valueX, y, value);
}

void scrollCharacterSheetUp()
{
    if (characterView == CHARACTER_VIEW_EQUIPMENT)
    {
        uint8_t& cursor = equipmentEditState == EQUIPMENT_SELECT_SLOT
            ? equipmentSlotCursor : equipmentItemCursor;
        if (cursor > 0)
            cursor--;
        needsRedraw = true;
        return;
    }

    if (scrollOffset >= 10)
        scrollOffset -= 10;
    needsRedraw = true;
}

void scrollCharacterSheetDown()
{
    if (characterView == CHARACTER_VIEW_EQUIPMENT)
    {
        uint8_t maximum = 3;
        if (equipmentEditState == EQUIPMENT_SELECT_ITEM)
        {
            EquipmentSlot slot = editableEquipmentSlots[equipmentSlotCursor];
            maximum = getCompatibleInventoryCount(slot);
            if (currentCharacter->equipment.equipped[slot].itemID != ITEM_NONE)
                maximum++;
        }
        uint8_t& cursor = equipmentEditState == EQUIPMENT_SELECT_SLOT
            ? equipmentSlotCursor : equipmentItemCursor;
        if (cursor < maximum)
            cursor++;
        needsRedraw = true;
        return;
    }

    scrollOffset += 10;
    Serial.println(scrollOffset);
    needsRedraw = true;
}

void enterCharacterSheet(Character* character)
{
    currentCharacter = character;
    scrollOffset = 0;
}

static void drawViewHeader(const char* title)
{
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(5, 5);
    tft.print(title);
    tft.drawFastHLine(0, 17, 240, ST77XX_WHITE);
}

static void drawInventoryView()
{
    drawViewHeader("Inventory");

    if (currentCharacter->inventory.itemCount == 0)
    {
        drawText(5, 32, "Empty");
        return;
    }

    int y = 32;

    for (uint8_t i = 0; i < currentCharacter->inventory.itemCount; i++)
    {
        const InventorySlot& slot = currentCharacter->inventory.slots[i];
        const Item* item = getItem(slot.item.itemID);

        if (item != nullptr)
        {
            drawText(5, y, item->name);

            if (slot.quantity > 1)
            {
                char quantity[8];
                snprintf(quantity, sizeof(quantity), "x%u", slot.quantity);
                drawText(200, y, quantity);
            }

            y += 12;
        }
    }
}

static const char* getEquipmentSlotName(EquipmentSlot slot)
{
    static const char* names[NUM_EQUIPMENT_SLOTS] =
    {
        "Melee", "Ranged", "Shield", "Armor", "Belt", "Body",
        "Chest", "Eyes", "Hands", "Head", "Headband", "Neck",
        "Ring 1", "Ring 2", "Shoulders", "Wrists"
    };

    return names[slot];
}

static uint8_t getCompatibleInventoryCount(EquipmentSlot slot)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < currentCharacter->inventory.itemCount; i++)
        if (isItemCompatibleWithEquipmentSlot(
                currentCharacter->inventory.slots[i].item, slot))
            count++;
    return count;
}

static const ItemInstance* getCompatibleInventoryItem(EquipmentSlot slot,
                                                       uint8_t index)
{
    for (uint8_t i = 0; i < currentCharacter->inventory.itemCount; i++)
    {
        const ItemInstance& item = currentCharacter->inventory.slots[i].item;
        if (!isItemCompatibleWithEquipmentSlot(item, slot))
            continue;
        if (index-- == 0)
            return &item;
    }
    return nullptr;
}

static void formatEquipmentItem(const ItemInstance& item,
                                char* buffer, size_t size)
{
    const Item* definition = getItem(item.itemID);
    if (definition == nullptr)
        snprintf(buffer, size, "None");
    else if (item.enhancementBonus != 0)
        snprintf(buffer, size, "%+d %s", item.enhancementBonus,
                 definition->name);
    else
        snprintf(buffer, size, "%s", definition->name);
}

static void drawEquipmentView()
{
    drawViewHeader("Equipment");

    if (equipmentEditState == EQUIPMENT_SELECT_SLOT)
    {
        int y = 32;
        for (uint8_t i = 0; i < 4; i++)
        {
            EquipmentSlot slot = editableEquipmentSlots[i];
            drawText(5, y, equipmentSlotCursor == i ? ">" : " ");
            drawText(15, y, getEquipmentSlotName(slot));
            drawText(75, y, getEquippedItemName(*currentCharacter, slot));
            y += 18;
        }
        char stats[48];
        snprintf(stats, sizeof(stats), "AC %d  Melee %+d  Ranged %+d",
                 getArmorClass(*currentCharacter),
                 getMeleeAttackBonus(*currentCharacter),
                 getRangedAttackBonus(*currentCharacter));
        drawText(5, 112, stats);
        drawText(5, 130, equipmentStatus);
        drawText(5, 220, "A/click: change   B: close");
        return;
    }

    EquipmentSlot slot = editableEquipmentSlots[equipmentSlotCursor];
    char title[32];
    snprintf(title, sizeof(title), "Change %s", getEquipmentSlotName(slot));
    drawViewHeader(title);
    uint8_t compatibleCount = getCompatibleInventoryCount(slot);
    bool occupied = currentCharacter->equipment.equipped[slot].itemID != ITEM_NONE;
    uint8_t optionCount = compatibleCount + (occupied ? 1 : 0) + 1;
    uint8_t first = equipmentItemCursor > 12 ? equipmentItemCursor - 12 : 0;
    int y = 32;

    for (uint8_t option = first; option < optionCount && y <= 200; option++)
    {
        drawText(5, y, equipmentItemCursor == option ? ">" : " ");
        if (option < compatibleCount)
        {
            const ItemInstance* item = getCompatibleInventoryItem(slot, option);
            char name[40];
            formatEquipmentItem(*item, name, sizeof(name));
            drawText(15, y, name);
        }
        else if (occupied && option == compatibleCount)
            drawText(15, y, "Unequip");
        else
            drawText(15, y, "Back");
        y += 13;
    }
    drawText(5, 220, equipmentStatus);
}

static Entity* getSheetCombatPlayer()
{
    Entity* entity = getCurrentCombatant();
    return entity != nullptr && entity->type == ENTITY_PLAYER &&
           &entity->character == currentCharacter ? entity : nullptr;
}

static bool canChangeEquipmentNow(EquipmentSlot slot, Entity*& combatPlayer)
{
    combatPlayer = nullptr;
    if (!combat.active)
        return true;
    if (slot == SLOT_ARMOR)
    {
        setEquipmentStatus("Cannot change armor during combat.");
        return false;
    }

    combatPlayer = getSheetCombatPlayer();
    if (!isPlayerTurn() || !combat.waitingForPlayer || combatPlayer == nullptr ||
        !canCharacterAct(combatPlayer->character) ||
        combatPlayer->turn.standardActionUsed)
    {
        setEquipmentStatus("No standard action available.");
        return false;
    }
    return true;
}

bool activateCharacterSheetSelection()
{
    if (characterView != CHARACTER_VIEW_EQUIPMENT || currentCharacter == nullptr)
        return false;

    if (equipmentEditState == EQUIPMENT_SELECT_SLOT)
    {
        equipmentEditState = EQUIPMENT_SELECT_ITEM;
        equipmentItemCursor = 0;
        equipmentStatus[0] = '\0';
        needsRedraw = true;
        return true;
    }

    EquipmentSlot slot = editableEquipmentSlots[equipmentSlotCursor];
    uint8_t compatibleCount = getCompatibleInventoryCount(slot);
    bool occupied = currentCharacter->equipment.equipped[slot].itemID != ITEM_NONE;

    if (equipmentItemCursor >= compatibleCount + (occupied ? 1 : 0))
    {
        equipmentEditState = EQUIPMENT_SELECT_SLOT;
        equipmentStatus[0] = '\0';
        needsRedraw = true;
        return true;
    }

    Entity* combatPlayer = nullptr;
    if (!canChangeEquipmentNow(slot, combatPlayer))
    {
        needsRedraw = true;
        return true;
    }

    bool success = false;
    if (equipmentItemCursor < compatibleCount)
    {
        const ItemInstance selected =
            *getCompatibleInventoryItem(slot, equipmentItemCursor);
        EquipResult result = equipItemWithResult(*currentCharacter, selected);
        success = result == EQUIP_SUCCESS;
        if (result == EQUIP_TWO_HANDED_CONFLICT)
            setEquipmentStatus(slot == SLOT_SHIELD
                ? "Cannot use with two-handed weapon."
                : "Cannot use with shield.");
        else if (!success)
            setEquipmentStatus("Equipment change failed.");
    }
    else
    {
        success = unequipItem(*currentCharacter, slot);
        if (!success)
            setEquipmentStatus("Inventory is full.");
    }

    if (success)
    {
        setEquipmentStatus("Equipment changed.");
        equipmentEditState = EQUIPMENT_SELECT_SLOT;
        equipmentItemCursor = 0;
        if (combatPlayer != nullptr)
        {
            combatPlayer->turn.standardActionUsed = true;
            combat.waitingForPlayer = true;
            checkEndPlayerTurn();
        }
    }
    needsRedraw = true;
    return true;
}

bool backCharacterSheetSelection()
{
    if (characterView != CHARACTER_VIEW_EQUIPMENT ||
        equipmentEditState != EQUIPMENT_SELECT_ITEM)
        return false;

    equipmentEditState = EQUIPMENT_SELECT_SLOT;
    equipmentItemCursor = 0;
    equipmentStatus[0] = '\0';
    needsRedraw = true;
    return true;
}

static void drawSkillsView()
{
    drawViewHeader("Skills");

    static const char* names[SKILL_COUNT] =
    {
        "Acrobatics", "Diplomacy", "Disable Dev.",
        "Intimidate", "Perception", "Stealth"
    };

    int y = 32;

    for (uint8_t i = 0; i < SKILL_COUNT; i++)
    {
        drawLabelValue(5, 140, y, names[i],
                       getSkillBonus(*currentCharacter,
                                     static_cast<Skill>(i)));
        y += 12;
    }
}

static void drawQuestsView()
{
    drawViewHeader("Quests");
    drawText(5, 32, "No quests available yet.");
}

void drawCharacterSheet()
{
    Serial.println("Drawing character sheet");

    if (currentCharacter == nullptr)
    {
        Serial.println("currentCharacter is NULL");
        return;
    }

    switch (characterView)
    {
        case CHARACTER_VIEW_INVENTORY:
            drawInventoryView();
            return;

        case CHARACTER_VIEW_EQUIPMENT:
            drawEquipmentView();
            return;

        case CHARACTER_VIEW_SKILLS:
            drawSkillsView();
            return;

        case CHARACTER_VIEW_QUESTS:
            drawQuestsView();
            return;

        case CHARACTER_VIEW_SHEET:
            break;
    }

    const int LEFT_X  = 5;
    const int VALUE_X = 120;

    int y = 5;

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);

    //--------------------------------------------------
    // Header
    //--------------------------------------------------

    drawText(LEFT_X, y,
             getCharacterClassName(currentCharacter->characterClass));

    char xpText[32];
    uint32_t nextLevelXP = currentCharacter->xp +
                           getExperienceToNextLevel(*currentCharacter);

    snprintf(xpText, sizeof(xpText), "XP %lu/%lu",
             static_cast<unsigned long>(currentCharacter->xp),
             static_cast<unsigned long>(nextLevelXP));
    drawText(75, y, xpText);
    y += 16;

    drawDivider(y);
    y += 8;

    drawLabelValue(LEFT_X, VALUE_X, y,
               "HP",
               currentCharacter->health.currentHP);

    drawText(145, y, "/");
    drawValue(155, y,
              currentCharacter->health.maxHP);
    y += 12;

    drawLabelValue(LEFT_X, VALUE_X, y, "Armor Class", getArmorClass(*currentCharacter));
    y += 12;

    drawLabelValue(LEFT_X, VALUE_X, y, "Level", currentCharacter->level);
    y += 18;

    //--------------------------------------------------
    // Abilities
    //--------------------------------------------------

    drawDivider(y);
    y += 8;

    drawText(LEFT_X, y, "Abilities");
    y += 14;

    drawLabelValue(LEFT_X, VALUE_X, y, "Strength", currentCharacter->abilities.strength); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Dexterity", currentCharacter->abilities.dexterity); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Constitution", currentCharacter->abilities.constitution); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Intelligence", currentCharacter->abilities.intelligence); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Wisdom", currentCharacter->abilities.wisdom); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Charisma", currentCharacter->abilities.charisma);
    y += 18;

    //--------------------------------------------------
    // Combat
    //--------------------------------------------------

    drawDivider(y);
    y += 8;

    drawText(LEFT_X, y, "Combat");
    y += 14;

    drawLabelValue(LEFT_X, VALUE_X, y, "Melee Attack", getMeleeAttackBonus(*currentCharacter)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Ranged Attack", getRangedAttackBonus(*currentCharacter)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Movement", getMovementSpeed(*currentCharacter));
    y += 18;

    if (currentCharacter->characterClass == CLASS_FIGHTER)
    {
        const WeaponGroup trainedGroup =
            getFighterTrainedWeaponGroup(*currentCharacter);
        const Weapon* trainedWeapon = getWeapon(
            getFighterStartingMeleeWeapon(trainedGroup));
        drawText(LEFT_X, y, "Weapon Style");
        drawText(VALUE_X, y, getWeaponGroupName(trainedGroup));
        y += 10;

        if (trainedWeapon != nullptr)
        {
            drawLabelValue(LEFT_X, VALUE_X, y, "Group Attack",
                getFighterWeaponAttackBonus(*currentCharacter, *trainedWeapon));
            y += 10;
            drawLabelValue(LEFT_X, VALUE_X, y, "Group Damage",
                getFighterWeaponDamageBonus(*currentCharacter, *trainedWeapon));
            y += 10;
        }

        drawLabelValue(LEFT_X, VALUE_X, y, "Toughness HP",
            getFighterBonusMaxHP(*currentCharacter));
        y += 18;
    }

    //--------------------------------------------------
    // Saving Throws
    //--------------------------------------------------

    drawDivider(y);
    y += 8;

    drawText(LEFT_X, y, "Saving Throws");
    y += 14;

    drawLabelValue(LEFT_X, VALUE_X, y, "Fortitude", getFortitudeSave(*currentCharacter)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Reflex", getReflexSave(*currentCharacter)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Will", getWillSave(*currentCharacter));
    y += 18;

    //--------------------------------------------------
    // Equipment
    //--------------------------------------------------

    drawDivider(y);
    y += 8;

    drawText(LEFT_X, y, "Equipment");
    y += 14;

    drawText(LEFT_X, y, "Melee");
    drawText(VALUE_X, y,
             getEquippedItemName(*currentCharacter, SLOT_MELEE_WEAPON));
    y += 10;

    drawText(LEFT_X, y, "Ranged");
    drawText(VALUE_X, y,
             getEquippedItemName(*currentCharacter, SLOT_RANGED_WEAPON));
    y += 10;

    drawText(LEFT_X, y, "Armor");
    drawText(VALUE_X, y,
             getEquippedItemName(*currentCharacter, SLOT_ARMOR));
    y += 10;

    drawText(LEFT_X, y, "Shield");
    drawText(VALUE_X, y,
             getEquippedItemName(*currentCharacter, SLOT_SHIELD));
    y += 18;

    //--------------------------------------------------
    // Skills
    //--------------------------------------------------

    drawDivider(y);
    y += 8;

    drawText(LEFT_X, y, "Skills");
    y += 14;

    drawLabelValue(LEFT_X, VALUE_X, y, "Acrobatics", getSkillBonus(*currentCharacter, SKILL_ACROBATICS)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Diplomacy", getSkillBonus(*currentCharacter, SKILL_DIPLOMACY)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Disable Dev.", getSkillBonus(*currentCharacter, SKILL_DISABLE_DEVICE)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Intimidate", getSkillBonus(*currentCharacter, SKILL_INTIMIDATE)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Perception", getSkillBonus(*currentCharacter, SKILL_PERCEPTION)); y += 10;
    drawLabelValue(LEFT_X, VALUE_X, y, "Stealth", getSkillBonus(*currentCharacter, SKILL_STEALTH));
    y += 18;

    //--------------------------------------------------
    // Inventory
    //--------------------------------------------------

    drawDivider(y);
    y += 8;

    drawText(LEFT_X, y, "Inventory");
    y += 14;

    drawLabelValue(LEFT_X, VALUE_X, y, "Items",
               currentCharacter->inventory.itemCount);
}
bool isCharacterSheetOpen = false;

void openCharacterSheet()
{
    openCharacterView(CHARACTER_VIEW_SHEET);
}

void openCharacterView(CharacterView view)
{
    Serial.println("Character sheet opened");
    isCharacterSheetOpen = true;
    characterView = view;
    resetEquipmentEditing();
    currentCharacter = &player;

    // Combat changes the character embedded in the map's player entity.
    // Show that live character so current HP reflects damage immediately.
    if (gameState == GAME_FOREST)
    {
        Entity* playerEntity = getPlayerEntity(
            forestEntities,
            forestEntityCount);

        if (playerEntity != nullptr)
            currentCharacter = &playerEntity->character;
    }
    else if (gameState == GAME_DUNGEON)
    {
        Entity* playerEntity = getPlayerEntity(
            dungeon.entities,
            dungeon.entityCount);

        if (playerEntity != nullptr)
            currentCharacter = &playerEntity->character;
    }

    scrollOffset = 0;
    suppressMenuInputUntilRelease();
    needsRedraw = true;
}

void closeCharacterSheet(){
    resetEquipmentEditing();
    isCharacterSheetOpen = false;
    suppressMenuInputUntilRelease();
    backgroundNeedsRedraw = true;
    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

bool isCharacterSheetVisible()
{
    return isCharacterSheetOpen;
}
