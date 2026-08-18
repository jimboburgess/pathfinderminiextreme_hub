//
// Created by james on 7/12/2026.
//

#include "buttons.h"
#include "Arduino.h"
#include "menu.h"
#include "characters/sheet.h"
#include "data/entityspawn.h"
#include "dungeon/activemap.h"
#include "dungeon/combat.h"
#include "dungeon/forest.h"
#include "dungeon/interaction.h"
#include "dungeon/turns.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "town/town.h"
#include "town/shop.h"
#include "data/savegame.h"
#include "input/inventorymenu.h"
#include "input/encoderdecoder.h"

//======================================
// Input State
//======================================

QuadratureDecoderState encoderDecoder;

bool selectLast = HIGH;
bool aLast = HIGH;
bool bLast = HIGH;

bool encoderSelectReady = true;
bool encoderSelectSuppressed = false;
unsigned long encoderSelectReleasedAt = 0;
unsigned long encoderSelectSuppressedAt = 0;

unsigned long encoderPressStart = 0;
bool encoderLongPressHandled = false;

constexpr unsigned long LONG_PRESS_TIME = 750;

constexpr unsigned long ENCODER_SELECT_RELEASE_DEBOUNCE_MS = 25;
constexpr unsigned long ENCODER_MENU_OPEN_GUARD_MS = 200;

void suppressEncoderSelectUntilRelease()
{
    bool now = digitalRead(ENCODER_SW);

    // Require a stable release before the select switch can be armed again.
    // This absorbs both a held click and its mechanical bounce.
    selectLast = now;
    encoderSelectReady = false;
    encoderSelectSuppressed = true;
    encoderSelectSuppressedAt = millis();

    if (now == HIGH)
        encoderSelectReleasedAt = encoderSelectSuppressedAt;
}

bool encoderPressed()
{
    bool now = digitalRead(ENCODER_SW);
    unsigned long nowMillis = millis();

    if (now == HIGH)
    {
        if (selectLast == LOW)
            encoderSelectReleasedAt = nowMillis;

        if (!encoderSelectReady &&
            nowMillis - encoderSelectReleasedAt >=
                ENCODER_SELECT_RELEASE_DEBOUNCE_MS)
        {
            encoderSelectReady = true;
        }
    }

    if (encoderSelectSuppressed)
    {
        selectLast = now;

        if (now == HIGH && encoderSelectReady &&
            nowMillis - encoderSelectSuppressedAt >=
                ENCODER_MENU_OPEN_GUARD_MS)
        {
            encoderSelectSuppressed = false;
        }

        return false;
    }

    bool pressed = now == LOW && selectLast == HIGH &&
                   encoderSelectReady;

    if (pressed)
        encoderSelectReady = false;

    selectLast = now;

    return pressed;
}

bool buttonAPressed()
{
    bool now = digitalRead(BUTTON_A);

    bool pressed = (now == LOW && aLast == HIGH);

    aLast = now;

    return pressed;
}

bool buttonBPressed()
{
    bool now = digitalRead(BUTTON_B);

    bool pressed = (now == LOW && bLast == HIGH);

    bLast = now;

    return pressed;
}

bool encoderButtonLongPressed()
{
    if (encoderSelectSuppressed)
    {
        encoderPressStart = 0;
        encoderLongPressHandled = false;
        return false;
    }

    bool pressed = (digitalRead(ENCODER_SW) == LOW);

    if (pressed)
    {
        if (encoderPressStart == 0)
        {
            encoderPressStart = millis();
            encoderLongPressHandled = false;
        }

        if (!encoderLongPressHandled &&
            millis() - encoderPressStart >= LONG_PRESS_TIME)
        {
            encoderLongPressHandled = true;
            return true;
        }
    }
    else
    {
        encoderPressStart = 0;
        encoderLongPressHandled = false;
    }

    return false;
}

EncoderDirection readEncoder()
{
    const int8_t step = updateQuadratureDecoder(
        encoderDecoder,
        digitalRead(ENCODER_CLK) == HIGH,
        digitalRead(ENCODER_DT) == HIGH);

    if (step == QUADRATURE_CLOCKWISE_STEP)
        return ENCODER_CLOCKWISE;

    if (step == QUADRATURE_COUNTERCLOCKWISE_STEP)
        return ENCODER_COUNTERCLOCKWISE;

    return ENCODER_NONE;
}


//======================================================
// Start Screen
//======================================================

void handleStartButtons()
{
    if (buttonAPressed())
    {
        playSound(SoundEffect::MENU_SELECT);
        gameState = GAME_CHARACTER_CREATION;
        enterCharacterCreation();
        return;
    }

    if (buttonBPressed())
    {
        if (!loadGame(player))
        {
            playSound(SoundEffect::ERROR);
            setGameMessage("No saved game found.");
            return;
        }

        // Dungeon runs are runtime-only and belong to the character that
        // created them. Loading a character starts with no retained run.
        abortCombat();
        resetDungeonRun(dungeon);

        playSound(SoundEffect::MENU_SELECT);
        clearGameMessage();
        resetButtonStates();

        townSelection = TOWN_STAY_HOME;
        redrawType = REDRAW_FULL;
        gameState = GAME_TOWN;
        needsRedraw = true;
    }
}

//======================================================
// Character Creation
//======================================================

void handleCharacterCreationButtons()
{

    EncoderDirection direction = readEncoder();

    if (direction != ENCODER_NONE)
    {
        switch (getCharacterCreationState())
        {
            case CCS_CLASS_SELECT:

                if (direction == ENCODER_CLOCKWISE)
                    rotateCharacterClassCW();
                else
                    rotateCharacterClassCCW();

                playSound(SoundEffect::MENU_MOVE);
                break;

            case CCS_VIEW_STATS:

                if (direction == ENCODER_CLOCKWISE)
                    scrollCharacterSheetDown();
                else
                    scrollCharacterSheetUp();

                needsRedraw = true;
                playSound(SoundEffect::MENU_MOVE);
                break;

            case CCS_MENU:

                if (direction == ENCODER_CLOCKWISE)
                    menuDown();
                else
                    menuUp();

                playSound(SoundEffect::MENU_MOVE);
                break;
        }
    }

    //--------------------------------------------------
    // Encoder Click
    //--------------------------------------------------

    if (encoderPressed())
    {
        playSound(SoundEffect::MENU_SELECT);

        switch (getCharacterCreationState())
        {
            case CCS_CLASS_SELECT:

                createPreviewCharacter();
                break;

            case CCS_VIEW_STATS:

                // Nothing to select.
                break;

            case CCS_MENU:

                menuSelect();
                break;
        }
    }

    //--------------------------------------------------
    // A Button
    //--------------------------------------------------

    if (buttonAPressed())
    {
        switch (getCharacterCreationState())
        {
            case CCS_CLASS_SELECT:

                createPreviewCharacter();
                playSound(SoundEffect::MENU_SELECT);
                break;

            case CCS_VIEW_STATS:

                openCharacterMenu();
                playSound(SoundEffect::MENU_SELECT);
                break;

            case CCS_MENU:

                // Reserved for future submenus.
                break;
        }
    }

    //--------------------------------------------------
    // B Button
    //--------------------------------------------------

    if (buttonBPressed())
    {
        switch (getCharacterCreationState())
        {
            case CCS_CLASS_SELECT:

                // Nothing.
                break;

            case CCS_VIEW_STATS:

                enterCharacterCreation();

                playSound(SoundEffect::MENU_BACK);
                break;

            case CCS_MENU:

                closeCharacterMenu();
                playSound(SoundEffect::MENU_BACK);
                break;
        }
    }
}

//======================================================
// Town
//======================================================

void handleTownButtons() {
    if (isCharacterSheetVisible())
    {
        handleCharacterSheetButtons();
        return;
    }

    if (menuState.isOpen)
    {
        EncoderDirection menuDirection = readEncoder();

        if (menuDirection == ENCODER_CLOCKWISE)
        {
            menuCursorDown();
            playSound(SoundEffect::MENU_MOVE);
        }
        else if (menuDirection == ENCODER_COUNTERCLOCKWISE)
        {
            menuCursorUp();
            playSound(SoundEffect::MENU_MOVE);
        }

        if (encoderPressed() || buttonAPressed())
        {
            playSound(SoundEffect::MENU_SELECT);
            menuActivate();
        }

        if (buttonBPressed())
        {
            playSound(SoundEffect::MENU_BACK);
            menuCancel();
        }

        return;
    }

    if (isTownRestActive())
        return;

    EncoderDirection direction = readEncoder();

    if (isTownHomeOpen())
    {
        if (direction == ENCODER_CLOCKWISE)
        {
            rotateTownHomeSelection(true);
            playSound(SoundEffect::MENU_MOVE);
        }
        else if (direction == ENCODER_COUNTERCLOCKWISE)
        {
            rotateTownHomeSelection(false);
            playSound(SoundEffect::MENU_MOVE);
        }

        if (encoderPressed())
        {
            playSound(SoundEffect::MENU_SELECT);

            switch (getTownHomeSelection())
            {
                case TOWN_HOME_REST:
                    beginTownRest();
                    break;

                case TOWN_HOME_SAVE_GAME:
                    setGameMessage(saveGame(player)
                        ? "Game saved."
                        : "Unable to save game.");
                    needsRedraw = true;
                    break;

                case TOWN_HOME_BACK:
                    closeTownHome();
                    break;
            }
        }

        if (buttonBPressed())
        {
            playSound(SoundEffect::MENU_BACK);
            closeTownHome();
        }

        return;
    }

    if (direction == ENCODER_CLOCKWISE)
    {
        townSelection =
            (TownOption)((townSelection + 1) % TOWN_OPTION_COUNT);

        playSound(SoundEffect::MENU_MOVE);
        needsRedraw = true;
    }
    else if (direction == ENCODER_COUNTERCLOCKWISE)
    {
        townSelection =
            (TownOption)((townSelection - 1 + TOWN_OPTION_COUNT) % TOWN_OPTION_COUNT);

        playSound(SoundEffect::MENU_MOVE);
        needsRedraw = true;
    }

    if (encoderPressed())
    {
        playSound(SoundEffect::MENU_SELECT);

        switch (townSelection)
        {
            case TOWN_FOREST:
                enterForest();
                break;

            case TOWN_STAY_HOME:
                openTownHome();
                break;

            case TOWN_DUNGEON:
                enterDungeon();
                break;

            case TOWN_SHOP:
                openTownShop();
                break;
        }
    }
}

//======================================================
// Dungeon and forest and town all maps
//======================================================
void handleMapButtons()
{
    //--------------------------------------------------
    // Entity inspection
    //--------------------------------------------------

    if (isInspectingEntities())
    {
        EncoderDirection direction = readEncoder();

        if (direction == ENCODER_CLOCKWISE)
        {
            rotateInspectedEntity(true);
            playSound(SoundEffect::MENU_MOVE);
        }
        else if (direction == ENCODER_COUNTERCLOCKWISE)
        {
            rotateInspectedEntity(false);
            playSound(SoundEffect::MENU_MOVE);
        }

        if (buttonAPressed())
        {
            confirmInspection();
            playSound(SoundEffect::MENU_SELECT);
        }

        if (buttonBPressed())
        {
            cancelInspection();
            playSound(SoundEffect::MENU_BACK);
        }

        return;
    }

    //--------------------------------------------------
    // Combat ability targeting
    //--------------------------------------------------

    if (isPlayerTargetingAbility())
    {
        // Ground targeting uses the shaft click as a spatial step. Give that
        // click priority so switch movement cannot also rotate the arrow in
        // the same input frame.
        if (isPlayerTargetingGroundAbility() && encoderPressed())
        {
            if (moveGroundAbilityTarget())
                playSound(SoundEffect::MENU_MOVE);
            else
                playSound(SoundEffect::BUMP);

            return;
        }

        EncoderDirection direction = readEncoder();

        if (direction == ENCODER_CLOCKWISE)
        {
            rotateAbilityTarget(true);
            playSound(SoundEffect::MENU_MOVE);
        }
        else if (direction == ENCODER_COUNTERCLOCKWISE)
        {
            rotateAbilityTarget(false);
            playSound(SoundEffect::MENU_MOVE);
        }

        if (buttonAPressed())
            confirmPlayerAbility();

        if (buttonBPressed())
        {
            cancelPlayerAbility();
            playSound(SoundEffect::MENU_BACK);
        }

        return;
    }

    //--------------------------------------------------
    // Combat attack targeting
    //--------------------------------------------------

    if (isPlayerTargetingAttack())
    {
        EncoderDirection direction = readEncoder();

        if (direction == ENCODER_CLOCKWISE)
        {
            rotateAttackTarget(true);
            playSound(SoundEffect::MENU_MOVE);
        }
        else if (direction == ENCODER_COUNTERCLOCKWISE)
        {
            rotateAttackTarget(false);
            playSound(SoundEffect::MENU_MOVE);
        }

        if (buttonAPressed())
        {
            confirmPlayerAttack();
        }

        if (buttonBPressed())
        {
            cancelPlayerAttack();
        }

        return;
    }

    if (isPlayerAttackResolving())
        return;

    if (isAbilityResolving())
        return;

    //--------------------------------------------------
    // Menu Open
    //--------------------------------------------------

    if (menuState.isOpen)
    {
        EncoderDirection direction = readEncoder();

        if (direction == ENCODER_CLOCKWISE)
        {
            menuCursorDown();
            playSound(SoundEffect::MENU_MOVE);
        }
        else if (direction == ENCODER_COUNTERCLOCKWISE)
        {
            menuCursorUp();
            playSound(SoundEffect::MENU_MOVE);
        }

        if (encoderPressed() || buttonAPressed())
        {
            playSound(SoundEffect::MENU_SELECT);
            menuActivate();
        }

        if (buttonBPressed())
        {
            playSound(SoundEffect::MENU_SELECT);
            menuCancel();
        }

        return;
    }

    //--------------------------------------------------
    // Character Sheet
    //--------------------------------------------------

    if (isCharacterSheetVisible())
    {
        handleCharacterSheetButtons();
        return;
    }

    //--------------------------------------------------
    // Move Player
    //--------------------------------------------------

    // Give a shaft click priority over rotary input. Mechanical movement
    // while pressing the encoder can disturb CLK/DT; it must not change the
    // selected direction immediately before this movement attempt.
    if (encoderPressed())
    {
        Serial.println("Encoder Pressed");

        bool moved = false;

        if (gameState == GAME_FOREST)
            moved = tryMoveForestPlayer();
        else if (gameState == GAME_DUNGEON)
            moved = tryMovePlayer(dungeon);

        if (moved)
            playSound(SoundEffect::WALK);

        return;
    }

    //--------------------------------------------------
    // Facing Direction
    //--------------------------------------------------
    EncoderDirection direction = readEncoder();

    if (direction != ENCODER_NONE)
    {
        previousMoveDirection = moveDirection;

        if (direction == ENCODER_CLOCKWISE)
            rotateDirectionCW();
        else
            rotateDirectionCCW();

        playSound(SoundEffect::MENU_MOVE);

        Entity* player = getActiveMapPlayer();

        if (player)
        {
            markTileDirty(
                player->x + directionOffsets[previousMoveDirection].dx,
                player->y + directionOffsets[previousMoveDirection].dy);

            markTileDirty(
                player->x + directionOffsets[moveDirection].dx,
                player->y + directionOffsets[moveDirection].dy);
        }
    }

    //--------------------------------------------------
    // A Button
    //--------------------------------------------------
    if (buttonAPressed())
    {
            // A dead monster at the normal facing cursor is a context
            // interaction, not a reason to open the general menu.
            if (tryInteractWithFacingEntity())
            {
                playSound(SoundEffect::MENU_SELECT);
                return;
            }

            openMenu(&mainMenu);
            menuState.redrawType = MENU_REDRAW_FULL;
            return;
    }
    //--------------------------------------------------
    // B Button
    //--------------------------------------------------

    if (buttonBPressed())
    {
        if (combat.active && isPlayerTurn())
        {
            endPlayerTurn();
        }
    }
}

void handleCharacterSheetButtons() {
    EncoderDirection direction = readEncoder();

    if (direction == ENCODER_CLOCKWISE)
    {
        scrollCharacterSheetDown();
        needsRedraw = true;
        playSound(SoundEffect::MENU_SELECT);
    }
    else if (direction == ENCODER_COUNTERCLOCKWISE)
    {
        scrollCharacterSheetUp();
        needsRedraw = true;
        playSound(SoundEffect::MENU_SELECT);
    }
    //--------------------------------------------------
    // Exit
    //--------------------------------------------------

    if (buttonBPressed())
    {
        playSound(SoundEffect::MENU_BACK);
        closeCharacterSheet();
        return;
    }

    if (encoderButtonLongPressed())
    {
        playSound(SoundEffect::MENU_BACK);
        closeCharacterSheet();
        return;
    }
}

//======================================================
// Main Button Handler
//======================================================

void resetButtonStates()
{
    resetQuadratureDecoder(
        encoderDecoder,
        digitalRead(ENCODER_CLK) == HIGH,
        digitalRead(ENCODER_DT) == HIGH);

    selectLast = digitalRead(ENCODER_SW);
    aLast = digitalRead(BUTTON_A);
    bLast = digitalRead(BUTTON_B);

    encoderSelectReady = selectLast == HIGH;
    encoderSelectReleasedAt = millis();
    encoderSelectSuppressed = false;
    encoderSelectSuppressedAt = 0;
}

void handleButtons()
{
    // The dynamic inventory and corpse-loot screens own the encoder and A/B
    // controls while open, before global long-press behavior can intervene.
    if (isInventoryMenuOpen())
    {
        handleInventoryMenuButtons();
        return;
    }

    if ((gameState == GAME_TOWN ||
     gameState == GAME_FOREST ||
     gameState == GAME_DUNGEON) &&
    encoderButtonLongPressed())
    {
        if (menuState.isOpen)
            closeMenu();

        if (isCharacterSheetVisible())
            closeCharacterSheet();
        else
            openCharacterSheet();

        return;
    }

    switch (gameState)
    {
        case GAME_START:
            handleStartButtons();
            break;

        case GAME_CHARACTER_CREATION:
            handleCharacterCreationButtons();
            break;

        case GAME_TOWN:
            handleTownButtons();
            break;

        case GAME_FOREST:

        case GAME_DUNGEON:
            handleMapButtons();
            break;
    }
}
