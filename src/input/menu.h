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
    MENU_SPECIAL_ABILITY,
    MENU_CHANNEL_ENERGY,
    MENU_USE_ITEM,
    MENU_DOUBLE_MOVE,
    MENU_TOTAL_DEFENSE,
    MENU_DELAY,
    MENU_END_TURN,

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
    MENU_SAVE_GAME,
    MENU_OPTIONS,
    MENU_EXIT_TITLE
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
};

struct Menu
{
    const char* title;

    const MenuItem* items;

    uint8_t itemCount;
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
void closeMenu();

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
