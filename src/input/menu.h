//
// Created by james on 7/24/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_MENU_H
#define PATHFINDERMINIEXTREME_025_MENU_H
#pragma once

#include <stdint.h>
#include "dungeon/combat.h"

//--------------------------------------------------
// Menu Colors
//--------------------------------------------------

constexpr uint16_t MENU_BG          = 0x2945;   // Dark slate
constexpr uint16_t MENU_HEADER_BG   = 0x3186;   // Header/footer
constexpr uint16_t MENU_BORDER      = 0x7BEF;   // Medium gray

constexpr uint16_t MENU_TEXT        = 0xFFFF;   // White
constexpr uint16_t MENU_DIM_TEXT    = 0xBDF7;   // Light gray

constexpr uint16_t MENU_HIGHLIGHT   = 0x5ACB;   // Selected row
constexpr uint16_t MENU_CURSOR      = 0xFFE0;   // Gold arrow
constexpr uint16_t MENU_DISABLED    = 0x8410;   // Disabled entries

//--------------------------------------------------
// Menu Layout
//--------------------------------------------------

constexpr int MENU_X = 10;
constexpr int MENU_Y = 10;

constexpr int MENU_WIDTH = 220;
constexpr int MENU_HEIGHT = 220;

constexpr int MENU_HEADER_HEIGHT = 22;
constexpr int MENU_FOOTER_HEIGHT = 18;
constexpr int MENU_DESCRIPTION_HEIGHT = 42;

constexpr int MENU_PADDING = 6;

constexpr int MENU_LINE_HEIGHT = 16;
constexpr uint8_t MENU_VISIBLE_ITEMS = 8;

constexpr int MENU_CURSOR_X = 16;
constexpr int MENU_TEXT_X   = 28;


enum MenuAction
{
    MENU_NONE,

    // Combat
    MENU_ATTACK,
    MENU_MELEE_ATTACK,
    MENU_RANGED_ATTACK,
    MENU_INSPECT,
    MENU_CAST_SPELL,
    MENU_CAST_ABILITY,
    MENU_SPECIAL_ABILITY,
    MENU_POWER_ATTACK,
    MENU_CHANNEL_ENERGY,
    MENU_TURN_UNDEAD,
    MENU_USE_ITEM,
    MENU_DOUBLE_MOVE,
    MENU_TOTAL_DEFENSE,
    MENU_DELAY,
    MENU_END_TURN,

    // Active map skills
    MENU_USE_SKILL,
    MENU_SKILL_ACROBATICS,
    MENU_SKILL_DISABLE_DEVICE,
    MENU_SKILL_INTIMIDATE,
    MENU_SKILL_PERCEPTION,
    MENU_SKILL_STEALTH,
    MENU_SKILL_BACK,
    MENU_CUT_FREE,
    MENU_IGNITE_WEB,

    // Locked chest interaction
    MENU_CHEST_PICK_LOCK,
    MENU_CHEST_FORCE_OPEN,
    MENU_CHEST_BACK,
    MENU_RIDDLE_DOOR_PICK_LOCK,
    MENU_RIDDLE_DOOR_BACK,
    MENU_FOUNTAIN_DRINK,
    MENU_FOUNTAIN_BACK,

    // Neutral NPC dialogue
    MENU_RIDDLE_ANSWER,
    MENU_RIDDLE_RETRY_PAY,
    MENU_RIDDLE_RETRY_CAT,
    MENU_RIDDLE_RETRY_LEAVE,

    // Character
    MENU_CHARACTER_SHEET,
    MENU_INVENTORY,
    MENU_EQUIPMENT,
    MENU_SKILLS,
    MENU_QUESTS,

    MENU_CHARACTER,

    // Game
    MENU_GAME,
    MENU_RETURN_TO_TOWN,
    MENU_TOGGLE_AUDIO,
    MENU_SAVE_GAME,
    MENU_OPTIONS,
    MENU_EXIT_TITLE,

    // Town dungeon entry
    MENU_DUNGEON_RESUME,
    MENU_DUNGEON_START_NEW,
    MENU_DUNGEON_START_NEW_NO,
    MENU_DUNGEON_START_NEW_YES,
    MENU_DUNGEON_BACK,

    // Town shop
    MENU_SHOP_BUY,
    MENU_SHOP_SELL,
    MENU_SHOP_LEAVE,
    MENU_SHOP_BUY_ITEM,
    MENU_SHOP_SELL_ITEM,

    MENU_RESIST_FIRE,
    MENU_RESIST_COLD,
    MENU_RESIST_ELECTRICITY,
    MENU_RESIST_ACID,
    MENU_RESIST_CANCEL
};

enum MenuClassMask : uint16_t
{
    MENU_CLASS_ALL      = 0xFFFF,

    MENU_CLASS_FIGHTER  = 1 << 0,
    MENU_CLASS_ROGUE    = 1 << 1,
    MENU_CLASS_WIZARD   = 1 << 2,
    MENU_CLASS_CLERIC   = 1 << 3
};

enum MenuRedrawType
{
    MENU_REDRAW_FULL,        // Draw entire menu window
    MENU_REDRAW_VISIBLE_ITEMS,        // Redraw all visible menu items
    MENU_REDRAW_SELECTION    // Redraw only old/new highlighted rows
};

struct Menu;

struct MenuItem
{
    const char* title;
    const char* description;

    MenuAction action;

    const Menu* child;

    uint16_t classMask;

    // Dynamic spell rows carry the AbilityID selected from knownAbilities.
    // Static rows omit this trailing field and initialize to ABILITY_NONE.
    AbilityID abilityID;
};

struct Menu
{
    const char* title;

    const MenuItem* items;

    uint8_t itemCount;

    // Optional fixed-buffer text used by dynamic menus such as the shop.
    const char* headerText;
    const char* statusText;

    // Optional wrapped body shown between the title and selectable rows.
    const char* bodyText;
    uint8_t bodyHeight;
};

struct MenuState
{
    const Menu* menuStack[16];

    uint8_t depth;

    uint8_t cursorIndex;

    uint8_t previousCursorIndex;

    uint8_t firstVisibleIndex;

    MenuRedrawType redrawType;

    bool isOpen;
};


extern MenuState menuState;
extern const Menu mainMenu;

bool isMenuItemVisible(MenuAction action);
bool isMenuItemEnabled(MenuAction action);

//--------------------------------------------------
// Menu Functions
//--------------------------------------------------

void openMenu(const Menu* menu);
void openResistEnergyMenu();
bool pushMenu(const Menu* menu);
void closeMenu();
void openDungeonEntryMenu();

void updateMenu();
void drawMenu();

//--------------------------------------------------
// Navigation
//--------------------------------------------------

void menuCursorUp();
void menuCursorDown();

void menuActivate();
void menuCancel();


#endif //PATHFINDERMINIEXTREME_025_MENU_H
