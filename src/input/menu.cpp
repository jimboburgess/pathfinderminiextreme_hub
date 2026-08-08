//
// Created by james on 7/24/2026.
//

#include "menu.h"
#include "data/game.h"
#include "graphics/display.h"
#include "input/buttons.h"
#include "audio/audio.h"
#include "characters/sheet.h"
#include "data/entityspawn.h"
#include "dungeon/dungeon.h"
#include "dungeon/forest.h"
#include "input/inventorymenu.h"

MenuState menuState =
{
    {},
    0,
    0,
    0,
    false
};

static const Menu* getCurrentMenu()
{
    if (menuState.depth == 0)
        return nullptr;

    return menuState.menuStack[menuState.depth - 1];
}

static Character* getMenuCharacter()
{
    if (gameState == GAME_FOREST)
    {
        Entity* playerEntity = getPlayerEntity(
            forestEntities, forestEntityCount);

        if (playerEntity != nullptr)
            return &playerEntity->character;
    }
    else if (gameState == GAME_DUNGEON)
    {
        Entity* playerEntity = getPlayerEntity(
            dungeon.entities, dungeon.entityCount);

        if (playerEntity != nullptr)
            return &playerEntity->character;
    }

    return &player;
}

static bool isPlayerCombatTurn()
{
    return combat.active && isPlayerTurn() && combat.waitingForPlayer;
}

static bool hasSpecialAbilities(const Character& character)
{
    for (uint8_t i = 0; i < character.magic.knownAbilityCount; i++)
    {
        AbilityID ability = character.magic.knownAbilities[i];

        if (ability != ABILITY_NONE &&
            ability != ABILITY_MELEE_ATTACK &&
            ability != ABILITY_RANGED_ATTACK)
        {
            return true;
        }
    }

    return false;
}

static uint16_t getCharacterMenuClassMask(const Character& character)
{
    switch (character.characterClass)
    {
        case CLASS_FIGHTER: return MENU_CLASS_FIGHTER;
        case CLASS_ROGUE:   return MENU_CLASS_ROGUE;
        case CLASS_WIZARD:  return MENU_CLASS_WIZARD;
        case CLASS_CLERIC:  return MENU_CLASS_CLERIC;
    }

    return 0;
}

static bool isMenuItemVisible(const MenuItem& item)
{
    Character* character = getMenuCharacter();

    return character != nullptr &&
           (item.classMask & getCharacterMenuClassMask(*character)) != 0 &&
           isMenuItemVisible(item.action);
}

static uint8_t getVisibleItemCount(const Menu* menu)
{
    if (menu == nullptr)
        return 0;

    uint8_t count = 0;

    for (uint8_t i = 0; i < menu->itemCount; i++)
    {
        if (isMenuItemVisible(menu->items[i]))
        {
            count++;
        }
    }

    return count;
}

static const MenuItem* getVisibleMenuItem(
    const Menu* menu,
    uint8_t visibleIndex)
{
    if (menu == nullptr)
        return nullptr;

    uint8_t count = 0;

    for (uint8_t i = 0; i < menu->itemCount; i++)
    {
        if (!isMenuItemVisible(menu->items[i]))
            continue;

        if (count == visibleIndex)
            return &menu->items[i];

        count++;
    }

    return nullptr;
}

static void drawMenuItem(uint8_t visibleRow, bool highlighted);


//
//--------------------------------------------------
// Attack Menu
//--------------------------------------------------
//

const MenuItem attackMenuItems[] =
{
    {
        "Melee Attack",
        "Attack with your equipped melee weapon.",
        MENU_MELEE_ATTACK,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Ranged Attack",
        "Attack with your equipped ranged weapon.",
        MENU_RANGED_ATTACK,
        nullptr,
        MENU_CLASS_ALL
    }
};

const Menu attackMenu =
{
    "Attack",
    attackMenuItems,
    sizeof(attackMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Spell Menu
//--------------------------------------------------
//

const MenuItem spellMenuItems[] =
{
    {
        "No Spells",
        "You have no spells available.",
        MENU_NONE,
        nullptr,
        MENU_CLASS_ALL
    }
};

const Menu spellMenu =
{
    "Cast Spell",
    spellMenuItems,
    sizeof(spellMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Use Item Menu
//--------------------------------------------------
//

const MenuItem useItemMenuItems[] =
{
    {
        "Inventory",
        "Choose an item to use.",
        MENU_USE_ITEM,
        nullptr,
        MENU_CLASS_ALL
    }
};

const Menu useItemMenu =
{
    "Use Item",
    useItemMenuItems,
    sizeof(useItemMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Fighter Special Menu
//--------------------------------------------------
//

const MenuItem fighterSpecialMenuItems[] =
{
    {
        "Combat Feats",
        "Use one of your combat feats.",
        MENU_NONE,
        nullptr,
        MENU_CLASS_FIGHTER
    }
};

const Menu fighterSpecialMenu =
{
    "Special Ability",
    fighterSpecialMenuItems,
    sizeof(fighterSpecialMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Rogue Special Menu
//--------------------------------------------------
//

const MenuItem rogueSpecialMenuItems[] =
{
    {
        "Rogue Talents",
        "Use one of your rogue talents.",
        MENU_NONE,
        nullptr,
        MENU_CLASS_ROGUE
    }
};

const Menu rogueSpecialMenu =
{
    "Special Ability",
    rogueSpecialMenuItems,
    sizeof(rogueSpecialMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Cleric Special Menu
//--------------------------------------------------
//

const MenuItem clericSpecialMenuItems[] =
{
    {
        "Channel Energy",
        "Heal or harm with divine energy.",
        MENU_NONE,
        nullptr,
        MENU_CLASS_CLERIC
    },
    {
        "Domain Powers",
        "Use one of your domain abilities.",
        MENU_NONE,
        nullptr,
        MENU_CLASS_CLERIC
    }
};

const Menu clericSpecialMenu =
{
    "Special Ability",
    clericSpecialMenuItems,
    sizeof(clericSpecialMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Wizard Special Menu
//--------------------------------------------------
//

const MenuItem wizardSpecialMenuItems[] =
{
    {
        "Arcane School",
        "Use one of your school powers.",
        MENU_NONE,
        nullptr,
        MENU_CLASS_WIZARD
    }
};

const Menu wizardSpecialMenu =
{
    "Special Ability",
    wizardSpecialMenuItems,
    sizeof(wizardSpecialMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Character Menu
//--------------------------------------------------
//

const MenuItem characterMenuItems[] =
{
    {
        "Character Sheet",
        "View your character statistics.",
        MENU_CHARACTER_SHEET,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Inventory",
        "View carried items.",
        MENU_INVENTORY,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Equipment",
        "View equipped gear.",
        MENU_EQUIPMENT,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Skills",
        "View trained skills.",
        MENU_SKILLS,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Quests",
        "View active quests.",
        MENU_QUESTS,
        nullptr,
        MENU_CLASS_ALL
    }
};

const Menu characterMenu =
{
    "Character",
    characterMenuItems,
    sizeof(characterMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Game Menu
//--------------------------------------------------
//

const MenuItem gameMenuItems[] =
{
    {
        "Return to Town",
        "Leave the current adventure.",
        MENU_RETURN_TO_TOWN,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Save Game",
        "Save your progress.",
        MENU_SAVE_GAME,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Options",
        "Game settings.",
        MENU_OPTIONS,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Exit to Title",
        "Return to the title screen.",
        MENU_EXIT_TITLE,
        nullptr,
        MENU_CLASS_ALL
    }
};

const Menu gameMenu =
{
    "Game",
    gameMenuItems,
    sizeof(gameMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Combat Menu
//--------------------------------------------------
//

const MenuItem combatMenuItems[] =
{
    {
        "Attack",
        "Perform a melee or ranged attack.",
        MENU_ATTACK,
        &attackMenu,
        MENU_CLASS_ALL
    },
{
    "Cast Spell",
    "Cast one of your prepared spells.",
    MENU_CAST_SPELL,
    &spellMenu,
    MENU_CLASS_WIZARD | MENU_CLASS_CLERIC
},
{
    "Inspect",
    "Examine a creature's condition.",
    MENU_INSPECT,
    nullptr,
    MENU_CLASS_ALL
},
{
    "Special Ability",
    "Use one of your class abilities.",
    MENU_SPECIAL_ABILITY,
    &fighterSpecialMenu,
    MENU_CLASS_FIGHTER
},
{
    "Special Ability",
    "Use one of your class abilities.",
    MENU_SPECIAL_ABILITY,
    &rogueSpecialMenu,
    MENU_CLASS_ROGUE
},
{
    "Special Ability",
    "Use one of your class abilities.",
    MENU_SPECIAL_ABILITY,
    &clericSpecialMenu,
    MENU_CLASS_CLERIC
},
{
    "Special Ability",
    "Use one of your class abilities.",
    MENU_SPECIAL_ABILITY,
    &wizardSpecialMenu,
    MENU_CLASS_WIZARD
},
{
    "Use Item",
    "Use an item from your inventory.",
    MENU_USE_ITEM,
    &useItemMenu,
    MENU_CLASS_ALL
},
    {
        "Double Move",
        "Move up to twice your speed.",
        MENU_DOUBLE_MOVE,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Total Defense",
        "Gain +4 AC until your next turn.",
        MENU_TOTAL_DEFENSE,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Delay",
        "Act later in the initiative order.",
        MENU_DELAY,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "End Turn",
        "End your current turn.",
        MENU_END_TURN,
        nullptr,
        MENU_CLASS_ALL
    },
    {
    "Character",
    "View character information.",
    MENU_CHARACTER,
        &characterMenu,
        MENU_CLASS_ALL
    },
    {
    "Game",
    "Game options.",
    MENU_GAME,
        &gameMenu,
        MENU_CLASS_ALL
    }
};

const Menu mainMenu =
{
    "Menu",
    combatMenuItems,
    sizeof(combatMenuItems) / sizeof(MenuItem)
};

void openMenu(const Menu* menu)
{
    if (menu == nullptr)
        return;

    menuState.menuStack[0] = menu;
    menuState.depth = 1;
    menuState.cursorIndex = 0;
    menuState.previousCursorIndex = 0;
    menuState.firstVisibleIndex = 0;
    menuState.redrawType = MENU_REDRAW_VISIBLE_ITEMS;
    menuState.isOpen = true;

    suppressEncoderSelectUntilRelease();
    needsRedraw = true;     // <-- add this
}

void closeMenu()
{
    menuState.depth = 0;
    menuState.cursorIndex = 0;
    menuState.firstVisibleIndex = 0;
    menuState.isOpen = false;

    backgroundNeedsRedraw = true;

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}
void updateMenu()
{
    // Reserved for future menu animation.
}

void menuCursorDown()
{
    const Menu* menu = getCurrentMenu();

    if (menu == nullptr)
        return;

    uint8_t visibleCount = getVisibleItemCount(menu);

    // Already at the bottom.
    if (menuState.cursorIndex >= visibleCount - 1)
        return;

    menuState.previousCursorIndex = menuState.cursorIndex;
    menuState.cursorIndex++;

    // Did the visible window scroll?
    if (menuState.cursorIndex >=
        menuState.firstVisibleIndex + MENU_VISIBLE_ITEMS)
    {
        menuState.firstVisibleIndex++;
        menuState.redrawType = MENU_REDRAW_VISIBLE_ITEMS;
    }
    else
    {
        menuState.redrawType = MENU_REDRAW_SELECTION;
    }

    needsRedraw = true;
}
void menuCursorUp()
{
    // Already at the top.
    if (menuState.cursorIndex == 0)
        return;

    menuState.previousCursorIndex = menuState.cursorIndex;
    menuState.cursorIndex--;

    // Did the visible window scroll?
    if (menuState.cursorIndex < menuState.firstVisibleIndex)
    {
        menuState.firstVisibleIndex--;
        menuState.redrawType = MENU_REDRAW_VISIBLE_ITEMS;
    }
    else
    {
        menuState.redrawType = MENU_REDRAW_SELECTION;
    }

    needsRedraw = true;
}

void menuActivate()
{
    const Menu* menu = getCurrentMenu();

    if (menu == nullptr)
        return;

    const MenuItem* item =
        getVisibleMenuItem(menu, menuState.cursorIndex);

    if (item == nullptr)
        return;

    //--------------------------------------------------
    // Open submenu
    //--------------------------------------------------

    if (item->child != nullptr)
    {
        menuState.menuStack[menuState.depth] = item->child;
        menuState.depth++;

        menuState.cursorIndex = 0;
        menuState.previousCursorIndex = 0;
        menuState.firstVisibleIndex = 0;

        menuState.redrawType = MENU_REDRAW_FULL;
        suppressEncoderSelectUntilRelease();
        needsRedraw = true;

        return;
    }

    //--------------------------------------------------
    // Execute action
    //--------------------------------------------------

    switch (item->action)
    {
        case MENU_MELEE_ATTACK:

            closeMenu();
            beginPlayerAttack(COMBAT_ATTACK_MELEE);
            break;

        case MENU_RANGED_ATTACK:

            closeMenu();
            beginPlayerAttack(COMBAT_ATTACK_RANGED);
            break;

        case MENU_INSPECT:

            closeMenu();
            beginInspection();
            break;

        case MENU_USE_ITEM:
        case MENU_INVENTORY:

            closeMenu();
            openPlayerInventoryMenu();
            break;

        case MENU_EQUIPMENT:

            closeMenu();
            openCharacterView(CHARACTER_VIEW_EQUIPMENT);
            break;

        case MENU_SKILLS:

            closeMenu();
            openCharacterView(CHARACTER_VIEW_SKILLS);
            break;

        case MENU_QUESTS:

            closeMenu();
            openCharacterView(CHARACTER_VIEW_QUESTS);
            break;

        case MENU_DOUBLE_MOVE:

            closeMenu();
            beginDoubleMove();
            break;

        case MENU_TOTAL_DEFENSE:

            closeMenu();
            beginTotalDefense();
            break;

        case MENU_CHARACTER_SHEET:

            closeMenu();
            openCharacterSheet();
            break;

        case MENU_RETURN_TO_TOWN:

            closeMenu();

            // Town owns the persistent player character. Preserve changes
            // made to the map entity before returning home to rest or save.
            {
                Character* currentCharacter = getMenuCharacter();

                if (currentCharacter != &player)
                    player = *currentCharacter;
            }

            gameState = GAME_TOWN;
            townSelection = TOWN_STAY_HOME;

            redrawType = REDRAW_FULL;
            needsRedraw = true;

            break;

        case MENU_END_TURN:

            closeMenu();

            endPlayerTurn();

            break;

        default:
            break;
    }
}
void menuCancel()
{
    if (menuState.depth > 1)
    {
        menuState.depth--;

        menuState.cursorIndex = 0;
        menuState.previousCursorIndex = 0;
        menuState.firstVisibleIndex = 0;

        menuState.redrawType = MENU_REDRAW_FULL;
        needsRedraw = true;
    }
    else
    {
        closeMenu();
    }
}
static void drawMenuWindow()
{
    // Main window
    tft.fillRect(
        MENU_X,
        MENU_Y,
        MENU_WIDTH,
        MENU_HEIGHT,
        MENU_BG);

    tft.drawRect(
        MENU_X,
        MENU_Y,
        MENU_WIDTH,
        MENU_HEIGHT,
        MENU_BORDER);

    //--------------------------------------------------
    // Header
    //--------------------------------------------------

    tft.fillRect(
        MENU_X,
        MENU_Y,
        MENU_WIDTH,
        MENU_HEADER_HEIGHT,
        MENU_HEADER_BG);

    //--------------------------------------------------
    // Description
    //--------------------------------------------------

    int descriptionY =
        MENU_Y +
        MENU_HEIGHT -
        MENU_FOOTER_HEIGHT -
        MENU_DESCRIPTION_HEIGHT;

    tft.drawLine(
        MENU_X,
        descriptionY,
        MENU_X + MENU_WIDTH,
        descriptionY,
        MENU_BORDER);

    //--------------------------------------------------
    // Footer
    //--------------------------------------------------

    int footerY =
        MENU_Y +
        MENU_HEIGHT -
        MENU_FOOTER_HEIGHT;

    tft.fillRect(
        MENU_X,
        footerY,
        MENU_WIDTH,
        MENU_FOOTER_HEIGHT,
        MENU_HEADER_BG);

    tft.drawLine(
        MENU_X,
        footerY,
        MENU_X + MENU_WIDTH,
        footerY,
        MENU_BORDER);
}
static void drawMenuItems()
{
    const Menu* menu = getCurrentMenu();

    if (menu == nullptr)
        return;

    uint8_t visibleCount = getVisibleItemCount(menu);

    uint8_t rows =
        min((uint8_t)MENU_VISIBLE_ITEMS,
            (uint8_t)(visibleCount - menuState.firstVisibleIndex));

    for (uint8_t row = 0; row < rows; row++)
    {
        bool highlighted =
            (menuState.firstVisibleIndex + row ==
             menuState.cursorIndex);

        drawMenuItem(row, highlighted);
    }
}

static void drawMenuItem(uint8_t row, bool highlighted)
{
    tft.setTextSize(1);
    const Menu* menu = getCurrentMenu();

    if (menu == nullptr)
        return;

    const MenuItem* item =
        getVisibleMenuItem(
            menu,
            menuState.firstVisibleIndex + row);

    if (item == nullptr)
        return;

    int y =
        MENU_Y +
        MENU_HEADER_HEIGHT +
        2 +
        row * MENU_LINE_HEIGHT;

    //--------------------------------------------------
    // Row background
    //--------------------------------------------------

    tft.fillRect(
        MENU_X + 1,
        y,
        MENU_WIDTH - 2,
        MENU_LINE_HEIGHT,
        highlighted ? MENU_HIGHLIGHT : MENU_BG);

    //--------------------------------------------------
    // Cursor
    //--------------------------------------------------

    if (highlighted)
    {
        tft.setTextColor(MENU_CURSOR);

        tft.setCursor(
            MENU_CURSOR_X,
            y + 3);

        tft.print(">");
    }

    //--------------------------------------------------
    // Text
    //--------------------------------------------------

    tft.setTextColor(
    MENU_TEXT,
    highlighted ? MENU_HIGHLIGHT : MENU_BG);

    tft.setCursor(
        MENU_TEXT_X,
        y + 3);

    tft.print(item->title);
}

static void drawMenuList()
{
    int listY =
        MENU_Y +
        MENU_HEADER_HEIGHT +
        2;

    int listHeight =
        MENU_VISIBLE_ITEMS * MENU_LINE_HEIGHT;

    //--------------------------------------------------
    // Clear the list area
    //--------------------------------------------------

    tft.fillRect(
        MENU_X + 1,
        listY,
        MENU_WIDTH - 2,
        listHeight,
        MENU_BG);

    //--------------------------------------------------
    // Draw every visible row
    //--------------------------------------------------

    drawMenuItems();
}

static void drawDescription(const MenuItem* item)
{
    if (item == nullptr)
        return;

    int descriptionY =
        MENU_Y +
        MENU_HEIGHT -
        MENU_FOOTER_HEIGHT -
        MENU_DESCRIPTION_HEIGHT;

    //--------------------------------------------------
    // Clear description area
    //--------------------------------------------------

    tft.fillRect(
        MENU_X + 1,
        descriptionY + 1,
        MENU_WIDTH - 2,
        MENU_DESCRIPTION_HEIGHT - 2,
        MENU_BG);

    //--------------------------------------------------
    // Description text
    //--------------------------------------------------

    tft.setTextColor(MENU_TEXT);
    tft.setTextSize(1);

    tft.setCursor(
        MENU_X + MENU_PADDING,
        descriptionY + 8);

    tft.print(item->description);
}

static void drawMenuSelection()
{
    uint8_t oldRow =
        menuState.previousCursorIndex -
        menuState.firstVisibleIndex;

    uint8_t newRow =
        menuState.cursorIndex -
        menuState.firstVisibleIndex;

    drawMenuItem(oldRow, false);
    drawMenuItem(newRow, true);

    const Menu* menu = getCurrentMenu();

    if (menu == nullptr)
        return;

    const MenuItem* selected =
        getVisibleMenuItem(
            menu,
            menuState.cursorIndex);

    drawDescription(selected);
}

void drawMenu()
{
    if (!menuState.isOpen)
        return;

    const Menu* menu = getCurrentMenu();

    if (menu == nullptr)
        return;

    switch (menuState.redrawType)
    {
        case MENU_REDRAW_SELECTION:
        {
            drawMenuSelection();
            return;
        }

        case MENU_REDRAW_VISIBLE_ITEMS:
        {
            drawMenuItems();

            const MenuItem* selected =
                getVisibleMenuItem(
                    menu,
                    menuState.cursorIndex);

            drawDescription(selected);
            return;
        }

        case MENU_REDRAW_FULL:
            break;
    }

    //--------------------------------------------------
    // Full menu redraw
    //--------------------------------------------------

    drawMenuWindow();
    drawMenuItems();

    //--------------------------------------------------
    // Title
    //--------------------------------------------------

    tft.setTextColor(MENU_TEXT);
    tft.setTextSize(2);

    tft.setCursor(
        MENU_X + MENU_PADDING,
        MENU_Y + 4);

    tft.print(menu->title);

    //--------------------------------------------------
    // Description
    //--------------------------------------------------

    const MenuItem* selected =
        getVisibleMenuItem(menu, menuState.cursorIndex);

    drawDescription(selected);

    //--------------------------------------------------
    // Footer
    //--------------------------------------------------

    tft.setTextSize(1);

    int footerY =
        MENU_Y +
        MENU_HEIGHT -
        MENU_FOOTER_HEIGHT +
        5;

    tft.setCursor(
        MENU_X + MENU_PADDING,
        footerY);

    tft.print("A Select");

    tft.setCursor(
        MENU_X + 140,
        footerY);

    tft.print("B Back");
}


bool isMenuItemVisible(MenuAction action)
{
    Character* character = getMenuCharacter();

    if (character == nullptr)
        return false;

    switch (action)
    {
        case MENU_ATTACK:
            return isPlayerCombatTurn() &&
                   (getEquippedMeleeWeapon(*character) != nullptr ||
                    getEquippedRangedWeapon(*character) != nullptr);

        case MENU_MELEE_ATTACK:
            return isPlayerCombatTurn() &&
                   getEquippedMeleeWeapon(*character) != nullptr;

        case MENU_RANGED_ATTACK:
            return isPlayerCombatTurn() &&
                   getEquippedRangedWeapon(*character) != nullptr;

        case MENU_CAST_SPELL:
            // The character model supports known spells, but spell casting
            // itself has not been implemented yet.
            return false;

        case MENU_SPECIAL_ABILITY:
            return hasSpecialAbilities(*character);

        case MENU_USE_ITEM:
            return character->inventory.itemCount > 0 &&
                   (!combat.active ||
                    (isPlayerCombatTurn() &&
                     !getCurrentCombatant()->turn.standardActionUsed));

        case MENU_DOUBLE_MOVE:
            return isPlayerCombatTurn() &&
                   character->state == STATE_ALIVE &&
                   !getCurrentCombatant()->turn.standardActionUsed &&
                   getCurrentCombatant()->turn.movementRemaining ==
                       character->speed;

        case MENU_TOTAL_DEFENSE:
            return isPlayerCombatTurn() &&
                   !getCurrentCombatant()->turn.standardActionUsed &&
                   !getCurrentCombatant()->turn.moveActionUsed;

        case MENU_DELAY:
            return false;

        case MENU_END_TURN:
            return isPlayerCombatTurn();

        case MENU_RETURN_TO_TOWN:
            return gameState == GAME_FOREST || gameState == GAME_DUNGEON;

        case MENU_SAVE_GAME:
        case MENU_OPTIONS:
        case MENU_EXIT_TITLE:
            return false;

        default:
            return true;
    }
}

bool isMenuItemEnabled(MenuAction action)
{
    return isMenuItemVisible(action);
}
