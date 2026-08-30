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
#include "map/activemap.h"
#include "map/interaction.h"
#include "map/skillactions.h"
#include "dungeon/abilityresolver.h"
#include "dungeon/dungeon.h"
#include "forest/forest.h"
#include "graphics/messagelog.h"
#include "input/inventorymenu.h"
#include "town/shop.h"
#include "data/progression.h"

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

static bool hasKnownAbilityInCategory(
    const Character& character,
    AbilityCategory category)
{
    uint8_t abilityCount = character.magic.knownAbilityCount;

    if (abilityCount > MAX_KNOWN_ABILITIES)
        abilityCount = MAX_KNOWN_ABILITIES;

    for (uint8_t i = 0; i < abilityCount; i++)
    {
        const Ability* ability = getAbility(
            character.magic.knownAbilities[i]);

        if (ability != nullptr && ability->category == category)
            return true;
    }

    return false;
}

static bool isSpellMenuAbility(const Ability& ability)
{
    return ability.category == ABILITY_CATEGORY_SPELL &&
           (ability.type == ABILITY_ARCANE ||
            ability.type == ABILITY_DIVINE);
}

static bool hasSupportedKnownSpell(const Character& character)
{
    if (character.characterClass == CLASS_CLERIC)
    {
        const uint8_t accessLevel = getClericSpellAccessLevel(character);
        for (uint16_t id = ABILITY_NONE + 1; id < ABILITY_MAX; id++)
        {
            const Ability* ability = getAbility(static_cast<AbilityID>(id));
            if (ability != nullptr && ability->type == ABILITY_DIVINE &&
                isSpellMenuAbility(*ability) && ability->level <= accessLevel &&
                isAbilitySupported(ability->id))
                return true;
        }
        return false;
    }

    uint8_t abilityCount = character.magic.knownAbilityCount;

    if (abilityCount > MAX_KNOWN_ABILITIES)
        abilityCount = MAX_KNOWN_ABILITIES;

    for (uint8_t i = 0; i < abilityCount; i++)
    {
        AbilityID abilityID = character.magic.knownAbilities[i];
        const Ability* ability = getAbility(abilityID);

        if (ability != nullptr && isSpellMenuAbility(*ability) &&
            isAbilitySupported(abilityID))
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

MenuItem spellMenuItems[MAX_KNOWN_ABILITIES] = {};

Menu spellMenu =
{
    "Cast Spell",
    spellMenuItems,
    0
};

const MenuItem resistEnergyMenuItems[] =
{
    { "Fire", "Resist fire damage.", MENU_RESIST_FIRE, nullptr, MENU_CLASS_ALL },
    { "Cold", "Resist cold damage.", MENU_RESIST_COLD, nullptr, MENU_CLASS_ALL },
    { "Electricity", "Resist electricity damage.", MENU_RESIST_ELECTRICITY, nullptr, MENU_CLASS_ALL },
    { "Acid", "Resist acid damage.", MENU_RESIST_ACID, nullptr, MENU_CLASS_ALL },
    { "Cancel", "Cancel without casting.", MENU_RESIST_CANCEL, nullptr, MENU_CLASS_ALL }
};

const Menu resistEnergyMenu =
{
    "Resist Energy",
    resistEnergyMenuItems,
    sizeof(resistEnergyMenuItems) / sizeof(MenuItem)
};

static void rebuildSpellMenu(const Character& character)
{
    spellMenu.itemCount = 0;

    if (character.characterClass == CLASS_CLERIC)
    {
        const uint8_t accessLevel = getClericSpellAccessLevel(character);
        for (uint16_t id = ABILITY_NONE + 1;
             id < ABILITY_MAX && spellMenu.itemCount < MAX_KNOWN_ABILITIES;
             id++)
        {
            AbilityID abilityID = static_cast<AbilityID>(id);
            const Ability* ability = getAbility(abilityID);
            if (ability == nullptr || ability->type != ABILITY_DIVINE ||
                !isSpellMenuAbility(*ability) || ability->level > accessLevel ||
                !isAbilitySupported(abilityID))
                continue;
            spellMenuItems[spellMenu.itemCount++] =
            { ability->name, "Cast this Divine spell.", MENU_CAST_ABILITY,
              nullptr, MENU_CLASS_CLERIC, abilityID };
        }
        return;
    }
    uint8_t abilityCount = character.magic.knownAbilityCount;

    if (abilityCount > MAX_KNOWN_ABILITIES)
        abilityCount = MAX_KNOWN_ABILITIES;

    for (uint8_t i = 0;
         i < abilityCount && spellMenu.itemCount < MAX_KNOWN_ABILITIES;
         i++)
    {
        AbilityID abilityID = character.magic.knownAbilities[i];
        const Ability* ability = getAbility(abilityID);

        if (ability == nullptr || !isSpellMenuAbility(*ability) ||
            !isAbilitySupported(abilityID))
        {
            continue;
        }

        spellMenuItems[spellMenu.itemCount++] =
        {
            ability->name,
            "Cast this known spell.",
            MENU_CAST_ABILITY,
            nullptr,
            MENU_CLASS_ALL,
            abilityID
        };
    }
}

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
        "Power Attack",
        "Trade melee accuracy for damage.",
        MENU_POWER_ATTACK,
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
        "Heal living allies within 30 feet.",
        MENU_CHANNEL_ENERGY,
        nullptr,
        MENU_CLASS_CLERIC
    },
    {
        "Turn Undead",
        "Turn nearby undead with divine power.",
        MENU_TURN_UNDEAD,
        nullptr,
        MENU_CLASS_CLERIC,
        ABILITY_TURN_UNDEAD
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
        "Toggle Audio",
        "Mute or enable game audio.",
        MENU_TOGGLE_AUDIO,
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

const MenuItem defeatedMenuItems[] =
{
    {
        "Return to Town",
        "Return to town at 1 HP.",
        MENU_RETURN_TO_TOWN,
        nullptr,
        MENU_CLASS_ALL
    }
};

const Menu defeatedMenu =
{
    "Defeated",
    defeatedMenuItems,
    sizeof(defeatedMenuItems) / sizeof(MenuItem)
};

const MenuItem useSkillMenuItems[] =
{
    { "Acrobatics", "Use Acrobatics when the situation permits.",
      MENU_SKILL_ACROBATICS, nullptr, MENU_CLASS_ALL },
    { "Disable Device", "Disarm a discovered adjacent trap.",
      MENU_SKILL_DISABLE_DEVICE, nullptr, MENU_CLASS_ALL },
    { "Intimidate", "Frighten one visible enemy.",
      MENU_SKILL_INTIMIDATE, nullptr, MENU_CLASS_ALL },
    { "Perception", "Actively search the current area.",
      MENU_SKILL_PERCEPTION, nullptr, MENU_CLASS_ALL },
    { "Stealth", "Attempt to remain unnoticed.",
      MENU_SKILL_STEALTH, nullptr, MENU_CLASS_ALL },
    { "Back", "Return to the previous menu.",
      MENU_SKILL_BACK, nullptr, MENU_CLASS_ALL }
};

const Menu useSkillMenu =
{
    "Use Skill",
    useSkillMenuItems,
    sizeof(useSkillMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Town Dungeon Entry
//--------------------------------------------------
//

const MenuItem startNewDungeonConfirmItems[] =
{
    {
        "No",
        "Keep the current dungeon progress.",
        MENU_DUNGEON_START_NEW_NO,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Yes",
        "Discard progress and generate a new dungeon.",
        MENU_DUNGEON_START_NEW_YES,
        nullptr,
        MENU_CLASS_ALL
    }
};

const Menu startNewDungeonConfirmMenu =
{
    "Start new dungeon?",
    startNewDungeonConfirmItems,
    sizeof(startNewDungeonConfirmItems) /
        sizeof(MenuItem),
    nullptr,
    "Current progress will be lost."
};

const MenuItem dungeonEntryMenuItems[] =
{
    {
        "Resume Dungeon",
        "Continue the saved dungeon run.",
        MENU_DUNGEON_RESUME,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Start New Dungeon",
        "Discard the saved dungeon and begin again.",
        MENU_DUNGEON_START_NEW,
        &startNewDungeonConfirmMenu,
        MENU_CLASS_ALL
    },
    {
        "Back",
        "Return to town.",
        MENU_DUNGEON_BACK,
        nullptr,
        MENU_CLASS_ALL
    }
};

const Menu dungeonEntryMenu =
{
    "Dungeon",
    dungeonEntryMenuItems,
    sizeof(dungeonEntryMenuItems) / sizeof(MenuItem)
};

//
//--------------------------------------------------
// Combat Menu
//--------------------------------------------------
//

const MenuItem combatMenuItems[] =
{
    {
        "Use Skill",
        "Use an active character skill.",
        MENU_USE_SKILL,
        &useSkillMenu,
        MENU_CLASS_ALL
    },
    {
        "Cut Free",
        "Use a physical attack to escape a web.",
        MENU_CUT_FREE,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Burn Web",
        "Ignite this web with a flaming weapon.",
        MENU_IGNITE_WEB,
        nullptr,
        MENU_CLASS_ALL
    },
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

    Character* character = getMenuCharacter();
    if (menu == &mainMenu && character != nullptr &&
        (character->health.currentHP <= 0 ||
         character->state == STATE_UNCONSCIOUS))
    {
        menu = &defeatedMenu;
    }

    menuState.menuStack[0] = menu;
    menuState.depth = 1;
    menuState.cursorIndex = 0;
    menuState.previousCursorIndex = 0;
    menuState.firstVisibleIndex = 0;
    menuState.redrawType = MENU_REDRAW_FULL;
    menuState.isOpen = true;

    suppressMenuInputUntilRelease();
    needsRedraw = true;     // <-- add this
}

void openResistEnergyMenu()
{
    openMenu(&resistEnergyMenu);
}

bool pushMenu(const Menu* menu)
{
    if (menu == nullptr ||
        menuState.depth >=
            sizeof(menuState.menuStack) / sizeof(menuState.menuStack[0]))
    {
        return false;
    }

    menuState.menuStack[menuState.depth++] = menu;
    menuState.cursorIndex = 0;
    menuState.previousCursorIndex = 0;
    menuState.firstVisibleIndex = 0;
    menuState.redrawType = MENU_REDRAW_FULL;

    suppressMenuInputUntilRelease();
    needsRedraw = true;
    return true;
}

void closeMenu()
{
    menuState.depth = 0;
    menuState.cursorIndex = 0;
    menuState.firstVisibleIndex = 0;
    menuState.isOpen = false;

    suppressMenuInputUntilRelease();

    backgroundNeedsRedraw = true;

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

void openDungeonEntryMenu()
{
    // Town only opens this selector for an unfinished run. Keep the helper
    // defensive so a completed/stale run cannot gain a Resume option.
    if (!hasResumableDungeon(dungeon))
    {
        enterDungeon();
        return;
    }

    openMenu(&dungeonEntryMenu);
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
        if (item->action == MENU_CAST_SPELL)
        {
            Character* character = getMenuCharacter();

            if (character == nullptr)
                return;

            rebuildSpellMenu(*character);

            if (spellMenu.itemCount == 0)
            {
                setGameMessage("No supported spells known.");
                playSound(SoundEffect::SPELL_FAIL);
                return;
            }
        }

        pushMenu(item->child);
        return;
    }

    //--------------------------------------------------
    // Execute action
    //--------------------------------------------------

    switch (item->action)
    {
        case MENU_MELEE_ATTACK:

            closeMenu();
            if (combat.active)
                beginPlayerAttack(COMBAT_ATTACK_MELEE);
            else
                beginOutOfCombatAttack(COMBAT_ATTACK_MELEE);
            break;

        case MENU_RANGED_ATTACK:

            closeMenu();
            if (combat.active)
                beginPlayerAttack(COMBAT_ATTACK_RANGED);
            else
                beginOutOfCombatAttack(COMBAT_ATTACK_RANGED);
            break;

        case MENU_CAST_ABILITY:

            closeMenu();
            beginPlayerAbility(item->abilityID);
            break;

        case MENU_RESIST_FIRE:
        case MENU_RESIST_COLD:
        case MENU_RESIST_ELECTRICITY:
        case MENU_RESIST_ACID:
        {
            DamageType type = DAMAGE_FIRE;
            if (item->action == MENU_RESIST_COLD)
                type = DAMAGE_COLD;
            else if (item->action == MENU_RESIST_ELECTRICITY)
                type = DAMAGE_ELECTRIC;
            else if (item->action == MENU_RESIST_ACID)
                type = DAMAGE_ACID;

            closeMenu();
            continuePlayerAbilityWithDamageType(type);
            break;
        }

        case MENU_RESIST_CANCEL:
            cancelPlayerAbility();
            closeMenu();
            break;

        case MENU_INSPECT:

            closeMenu();
            beginInspection();
            break;

        case MENU_POWER_ATTACK:
        {
            closeMenu();

            Entity* fighter = getActiveMapPlayer();

            if (fighter != nullptr)
            {
                togglePowerAttack(*fighter);
            }
            else
            {
                setGameMessage("Power Attack unavailable.");
                playSound(SoundEffect::ERROR);
            }

            break;
        }

        case MENU_CHANNEL_ENERGY:
        {
            closeMenu();

            Entity* cleric = getActiveMapPlayer();

            if (cleric != nullptr)
            {
                useChannelEnergy(*cleric);
            }
            else
            {
                setGameMessage("Channel Energy unavailable.");
                playSound(SoundEffect::ERROR);
            }

            break;
        }

        case MENU_TURN_UNDEAD:
            closeMenu();
            beginPlayerAbility(ABILITY_TURN_UNDEAD);
            break;

        case MENU_SKILL_ACROBATICS:
        case MENU_SKILL_DISABLE_DEVICE:
        case MENU_SKILL_INTIMIDATE:
        case MENU_SKILL_PERCEPTION:
        case MENU_SKILL_STEALTH:
        {
            Skill skill = SKILL_ACROBATICS;
            if (item->action == MENU_SKILL_DISABLE_DEVICE)
                skill = SKILL_DISABLE_DEVICE;
            else if (item->action == MENU_SKILL_INTIMIDATE)
                skill = SKILL_INTIMIDATE;
            else if (item->action == MENU_SKILL_PERCEPTION)
                skill = SKILL_PERCEPTION;
            else if (item->action == MENU_SKILL_STEALTH)
                skill = SKILL_STEALTH;

            closeMenu();
            useSkill(skill);
            break;
        }

        case MENU_SKILL_BACK:
            menuCancel();
            break;

        case MENU_CUT_FREE:
            closeMenu();
            cutFreeFromWeb();
            break;

        case MENU_IGNITE_WEB:
            closeMenu();
            igniteWeb();
            break;

        case MENU_CHEST_PICK_LOCK:
            pickLockedChest();
            break;

        case MENU_CHEST_FORCE_OPEN:
            forceOpenLockedChest();
            break;

        case MENU_CHEST_BACK:
            menuCancel();
            break;

        case MENU_FOUNTAIN_DRINK:
            drinkFromFountain();
            break;

        case MENU_FOUNTAIN_BACK:
            menuCancel();
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

                // Returning home after a defeat revives the character just
                // enough to use the existing town healing/rest flow. Keep
                // conditions and every other persistent character field.
                if (currentCharacter != nullptr &&
                    (currentCharacter->health.currentHP <= 0 ||
                     currentCharacter->state == STATE_UNCONSCIOUS))
                {
                    currentCharacter->health.currentHP = 1;
                    updateCharacterStateFromHP(*currentCharacter);
                }

                if (currentCharacter != nullptr && currentCharacter != &player)
                    player = *currentCharacter;
            }

            // Leaving an encounter is not combat victory: discard only the
            // combat session, with no XP/loot/death side effects.
            abortCombat();

            if (gameState == GAME_DUNGEON)
            {
                updateCurrentDungeonRoomCompletion(dungeon);
                suspendDungeonRun(dungeon);

                // Final-encounter victory becomes a completed run only when
                // the player has safely returned to town.
                markDungeonCompletedOnTownReturn(dungeon);
            }

            gameState = GAME_TOWN;
            townSelection = TOWN_STAY_HOME;

            redrawType = REDRAW_FULL;
            needsRedraw = true;

            break;

        case MENU_TOGGLE_AUDIO:
            setAudioMuted(!isAudioMuted());
            setGameMessage(isAudioMuted() ? "Audio muted." : "Audio enabled.");
            break;

        case MENU_DUNGEON_RESUME:
            closeMenu();
            enterDungeon();
            break;

        case MENU_DUNGEON_START_NEW_NO:
            // The confirmation is a child of the dungeon submenu, so popping
            // it restores that submenu with the retained run untouched.
            menuCancel();
            break;

        case MENU_DUNGEON_START_NEW_YES:
            closeMenu();
            resetDungeonRun(dungeon);
            enterDungeon();
            break;

        case MENU_DUNGEON_BACK:
            closeMenu();
            break;

        case MENU_SHOP_BUY:
            openShopBuyMenu();
            break;

        case MENU_SHOP_SELL:
            openShopSellMenu();
            break;

        case MENU_SHOP_LEAVE:
            closeMenu();
            break;

        case MENU_SHOP_BUY_ITEM:
            buyShopItem(menuState.cursorIndex);
            break;

        case MENU_SHOP_SELL_ITEM:
            sellShopItem(menuState.cursorIndex);
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
    if (getCurrentMenu() == &resistEnergyMenu)
    {
        cancelPlayerAbility();
        closeMenu();
        return;
    }

    if (menuState.depth > 1)
    {
        menuState.depth--;

        menuState.cursorIndex = 0;
        menuState.previousCursorIndex = 0;
        menuState.firstVisibleIndex = 0;

        menuState.redrawType = MENU_REDRAW_FULL;
        suppressMenuInputUntilRelease();
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

    const Menu* menu = getCurrentMenu();
    const char* description = item->description;

    if (menu != nullptr && menu->statusText != nullptr &&
        menu->statusText[0] != '\0')
    {
        description = menu->statusText;
    }

    if (description != nullptr)
        tft.print(description);
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
            drawMenuList();

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

    if (menu->headerText != nullptr && menu->headerText[0] != '\0')
    {
        tft.setTextSize(1);
        tft.setCursor(MENU_X + 120, MENU_Y + 8);
        tft.print(menu->headerText);
    }

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
            return (isPlayerCombatTurn() ||
                    canPlayerAttackOutsideCombat(COMBAT_ATTACK_MELEE) ||
                    canPlayerAttackOutsideCombat(COMBAT_ATTACK_RANGED)) &&
                   canCharacterAct(*character) &&
                   (getEquippedMeleeWeapon(*character) != nullptr ||
                    getEquippedRangedWeapon(*character) != nullptr);

        case MENU_MELEE_ATTACK:
            return (isPlayerCombatTurn() ||
                    canPlayerAttackOutsideCombat(COMBAT_ATTACK_MELEE)) &&
                   canCharacterAct(*character) &&
                   getEquippedMeleeWeapon(*character) != nullptr;

        case MENU_RANGED_ATTACK:
            return (isPlayerCombatTurn() ||
                    canPlayerAttackOutsideCombat(COMBAT_ATTACK_RANGED)) &&
                   canCharacterAct(*character) &&
                   getEquippedRangedWeapon(*character) != nullptr;

        case MENU_CAST_SPELL:
            return isPlayerCombatTurn() &&
                   canCharacterAct(*character) &&
                   !getCurrentCombatant()->turn.standardActionUsed &&
                   hasSupportedKnownSpell(*character);

        case MENU_SPECIAL_ABILITY:
            if (character->characterClass == CLASS_FIGHTER)
            {
                return hasKnownAbilityInCategory(
                           *character,
                           ABILITY_CATEGORY_CLASS_FEATURE) &&
                       isMenuItemVisible(MENU_POWER_ATTACK);
            }

            return hasKnownAbilityInCategory(
                *character, ABILITY_CATEGORY_CLASS_FEATURE);

        case MENU_POWER_ATTACK:
        {
            Entity* fighter = getActiveMapPlayer();

            return fighter != nullptr &&
                   &fighter->character == character &&
                   knowsAbility(*character, ABILITY_POWER_ATTACK) &&
                   canTogglePowerAttack(*fighter);
        }

        case MENU_CHANNEL_ENERGY:
        {
            if (character->characterClass != CLASS_CLERIC)
                return false;

            if (!combat.active)
                return true;

            Entity* combatant = getCurrentCombatant();

            return isPlayerCombatTurn() &&
                   combatant != nullptr &&
                   &combatant->character == character &&
                   canCharacterAct(*character) &&
                   !combatant->turn.standardActionUsed;
            }

        case MENU_TURN_UNDEAD:
        {
            Entity* cleric = getActiveMapPlayer();

            return cleric != nullptr &&
                   &cleric->character == character &&
                   knowsAbility(*character, ABILITY_TURN_UNDEAD) &&
                   combat.active &&
                   isPlayerCombatTurn() &&
                   getCurrentCombatant() == cleric &&
                   canCharacterAct(*character) &&
                   !cleric->turn.standardActionUsed;
        }

        case MENU_USE_SKILL:
            return (gameState == GAME_DUNGEON || gameState == GAME_FOREST) &&
                   (!combat.active ||
                    (isPlayerCombatTurn() && canCharacterAct(*character) &&
                     !getCurrentCombatant()->turn.standardActionUsed));

        case MENU_CUT_FREE:
        {
            Entity* entity = getActiveMapPlayer();
            return entity != nullptr && canCutFreeFromWeb(*entity) &&
                   (!combat.active ||
                    (isPlayerCombatTurn() &&
                     !entity->turn.standardActionUsed));
        }

        case MENU_IGNITE_WEB:
        {
            Entity* entity = getActiveMapPlayer();
            return entity != nullptr && canIgniteWeb(*entity) &&
                   (!combat.active ||
                    (isPlayerCombatTurn() &&
                     !entity->turn.standardActionUsed));
        }

        case MENU_USE_ITEM:
            return character->inventory.itemCount > 0 &&
                   (!combat.active ||
                    (isPlayerCombatTurn() &&
                     canCharacterAct(*character) &&
                     !getCurrentCombatant()->turn.standardActionUsed));

        case MENU_DOUBLE_MOVE:
            return isPlayerCombatTurn() &&
                   character->state == STATE_ALIVE &&
                   canCharacterAct(*character) &&
                   !getCurrentCombatant()->turn.standardActionUsed &&
                   getCurrentCombatant()->turn.movementRemaining ==
                       character->speed;

        case MENU_TOTAL_DEFENSE:
            return isPlayerCombatTurn() &&
                   canCharacterAct(*character) &&
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
