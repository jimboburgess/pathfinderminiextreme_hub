//
// Created by james on 7/16/2026.
//

#include "sheet.h"
#include "graphics/display.h"
#include "graphics/sprites.h"
#include "data/game.h"
#include <cstring>


static Character* currentCharacter = nullptr;
static int scrollOffset = 0;

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
    if (scrollOffset >= 10)
        scrollOffset -= 10;
    needsRedraw = true;
}

void scrollCharacterSheetDown()
{
    scrollOffset += 10;
    Serial.println(scrollOffset);
    needsRedraw = true;
}

void enterCharacterSheet(Character* character)
{
    currentCharacter = character;
    scrollOffset = 0;
}

void drawCharacterSheet()
{
    Serial.println("Drawing character sheet");

    if (currentCharacter == nullptr)
    {
        Serial.println("currentCharacter is NULL");
        return;
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

    drawText(70, y, getCharacterClassName(currentCharacter->characterClass));
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
    y += 12;

    drawLabelValue(LEFT_X, VALUE_X, y, "XP", currentCharacter->xp);
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
    Serial.println("Character sheet opened");
    isCharacterSheetOpen = true;
    currentCharacter = &player;
    needsRedraw = true;
}

void closeCharacterSheet(){
    isCharacterSheetOpen = false;
    needsRedraw = true;
}

bool isCharacterSheetVisible()
{
    return isCharacterSheetOpen;
}