//
// Created by james on 7/12/2026.
//

#include "buttons.h"
#include "Arduino.h"
#include "menu.h"
#include "characters/sheet.h"
#include "data/entityspawn.h"
#include "dungeon/combat.h"
#include "dungeon/forest.h"
#include "dungeon/turns.h"
#include "graphics/display.h"

//======================================
// Input State
//======================================

bool encoderLastCLK = HIGH;
unsigned long encoderLastMove = 0;

bool selectLast = HIGH;
bool aLast = HIGH;
bool bLast = HIGH;

unsigned long encoderPressStart = 0;
bool encoderLongPressHandled = false;

constexpr unsigned long LONG_PRESS_TIME = 750;

const uint16_t ENCODER_DEBOUNCE = 3;

bool encoderPressed()
{
    bool now = digitalRead(ENCODER_SW);

    bool pressed = (now == LOW && selectLast == HIGH);

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
    bool clkNow = digitalRead(ENCODER_CLK);
    bool dtNow  = digitalRead(ENCODER_DT);

    EncoderDirection direction = ENCODER_NONE;

    if (clkNow != encoderLastCLK && clkNow == LOW)
    {
        if (millis() - encoderLastMove > ENCODER_DEBOUNCE)
        {
            encoderLastMove = millis();

            if (dtNow == clkNow)
                direction = ENCODER_CLOCKWISE;
            else
                direction = ENCODER_COUNTERCLOCKWISE;
        }
    }

    encoderLastCLK = clkNow;

    return direction;
}


//======================================================
// Start Screen
//======================================================

void handleStartButtons()
{
    if (encoderPressed() ||
    buttonAPressed() ||
    buttonBPressed())
    {
        playSound(SoundEffect::MENU_SELECT);
        resetButtonStates();
        gameState = GAME_CHARACTER_CREATION;
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

                // No action.
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

                playSound(SoundEffect::MENU_SELECT);
                break;

            case CCS_MENU:

                closeCharacterMenu();
                playSound(SoundEffect::MENU_SELECT);
                break;
        }
    }
}

//======================================================
// Town
//======================================================

void handleTownButtons()
{

    if (isCharacterSheetVisible())
    {
        handleCharacterSheetButtons();
        return;
    }
     bool clkNow = digitalRead(ENCODER_CLK);

    if (clkNow != encoderLastCLK && clkNow == LOW)
    {
        if (millis() - encoderLastMove > ENCODER_DEBOUNCE)
        {
            encoderLastMove = millis();

            if (digitalRead(ENCODER_DT) != clkNow)
            {
                townSelection =
                    (TownOption)((townSelection + 1) % TOWN_OPTION_COUNT);
            }
            else
            {
                townSelection =
                    (TownOption)((townSelection - 1 + TOWN_OPTION_COUNT) % TOWN_OPTION_COUNT);
            }

            playSound(SoundEffect::MENU_MOVE);
            needsRedraw = true;
        }
    }

    encoderLastCLK = clkNow;

    if (encoderPressed())
    {
        playSound(SoundEffect::MENU_SELECT);

        switch (townSelection)
        {
            case TOWN_GOBLINS:

                enterForest();

                break;


            case TOWN_STAY_HOME:

                break;


            case TOWN_DUNGEON:

                generateDungeon(dungeon);
                gameState = GAME_DUNGEON;

                break;
        }

        needsRedraw = true;
    }
}

//======================================================
// Dungeon and forest and town all maps
//======================================================
void handleMapButtons()
{
    if (combat.active)
    {
        updateCombat();
    }

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
            menuSelect();
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
    // Facing Direction
    //--------------------------------------------------

    bool clkNow = digitalRead(ENCODER_CLK);

    if (clkNow != encoderLastCLK && clkNow == LOW)
    {
        if (millis() - encoderLastMove > ENCODER_DEBOUNCE)
        {
            encoderLastMove = millis();

            previousMoveDirection = moveDirection;

            if (digitalRead(ENCODER_DT) != clkNow)
                rotateDirectionCW();
            else
                rotateDirectionCCW();

            playSound(SoundEffect::MENU_MOVE);

            Entity* player = getPlayerEntity(
            forestEntities,
            forestEntityCount);

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
    }

    encoderLastCLK = clkNow;

    //--------------------------------------------------
    // Move Player
    //--------------------------------------------------

    if (encoderPressed())
    {
        Serial.println("Encoder Pressed");

        if (gameState == GAME_FOREST)
        {
            tryMoveForestPlayer();
        }
        else if (gameState == GAME_DUNGEON)
        {
            tryMovePlayer(dungeon);
        }
    }

    //--------------------------------------------------
    // A Button
    //--------------------------------------------------
    if (buttonAPressed())
    {
        if (combat.active && isPlayerTurn())
        {
            endPlayerTurn();
        }
        else
        {
            openMenu(&combatMenu);
            menuState.redrawType = MENU_REDRAW_FULL;
        }

        return;
    }
}

void handleCharacterSheetButtons()
{
    EncoderDirection direction = readEncoder();

    if (direction == ENCODER_CLOCKWISE)
    {
        scrollCharacterSheetDown();
        needsRedraw = true;
        playSound(SoundEffect::MENU_MOVE);
    }
    else if (direction == ENCODER_COUNTERCLOCKWISE)
    {
        scrollCharacterSheetUp();
        needsRedraw = true;
        playSound(SoundEffect::MENU_MOVE);
    }
    //--------------------------------------------------
    // Exit
    //--------------------------------------------------

    if (buttonBPressed())
    {
        closeCharacterSheet();
        return;
    }

    if (encoderButtonLongPressed())
    {
        closeCharacterSheet();
        return;
    }
}

//======================================================
// Main Button Handler
//======================================================

void resetButtonStates()
{
    encoderLastCLK = digitalRead(ENCODER_CLK);

    selectLast = digitalRead(ENCODER_SW);
    aLast = digitalRead(BUTTON_A);
    bLast = digitalRead(BUTTON_B);

    encoderLastMove = millis();
}

void handleButtons()
{
    if ((gameState == GAME_TOWN || gameState == GAME_DUNGEON) &&
    encoderButtonLongPressed())
    {
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