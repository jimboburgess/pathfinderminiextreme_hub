#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
#include "native_rooms.h"
#endif
//
// Created by james on 7/12/2026.
//

#include "buttons.h"
#include "Arduino.h"
#include "config.h"
#include "audio/audio.h"
#include "graphics/charcreationscreen.h"
#include "dungeon/dungeon.h"
#include "map/playermovement.h"
#include "data/game.h"
#include "menu.h"
#include "characters/sheet.h"
#include "data/entityspawn.h"
#include "map/activemap.h"
#include "dungeon/combat.h"
#include "dungeon/bellpuzzle.h"
#include "forest/forest.h"
#include "map/interaction.h"
#include "dungeon/turns.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "town/town.h"
#include "multiplayer/multiplayer_activity.h"
#include "town/shop.h"
#include "data/savegame.h"
#include "data/settings.h"
#include "input/inventorymenu.h"
#include "input/savemenu.h"
#include "input/buttonstate.h"

//======================================
// Input State
//======================================

namespace { ButtonInputState handheldInput; }

void suppressEncoderSelectUntilRelease() { handheldInput.suppressEncoderSelectUntilRelease(digitalRead(ENCODER_SW), millis()); }
bool encoderPressed() { return handheldInput.encoderPressed(digitalRead(ENCODER_SW), millis()); }
bool buttonAPressed() { return handheldInput.buttonAPressed(digitalRead(BUTTON_A), millis()); }
bool buttonBPressed() { return handheldInput.buttonBPressed(digitalRead(BUTTON_B), millis()); }
void suppressMenuInputUntilRelease() { handheldInput.suppressMenuInputUntilRelease(digitalRead(ENCODER_SW), digitalRead(BUTTON_A), digitalRead(BUTTON_B), millis()); }
bool encoderButtonLongPressed() { return handheldInput.encoderButtonLongPressed(digitalRead(ENCODER_SW), millis()); }
EncoderDirection readEncoder()
{
    const int8_t step=handheldInput.readEncoder(digitalRead(ENCODER_CLK)==HIGH, digitalRead(ENCODER_DT)==HIGH, isEncoderRotationInverted());
    return step==QUADRATURE_CLOCKWISE_STEP?ENCODER_CLOCKWISE:step==QUADRATURE_COUNTERCLOCKWISE_STEP?ENCODER_COUNTERCLOCKWISE:ENCODER_NONE;
}

//======================================================
// Start Screen
//======================================================

void handleStartButtons()
{
    if (menuState.isOpen)
    {
        const EncoderDirection direction = readEncoder();
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
            playSound(SoundEffect::MENU_BACK);
            menuCancel();
        }
        return;
    }

    if (buttonAPressed())
    {
        playSound(SoundEffect::MENU_SELECT);
        gameState = GAME_CHARACTER_CREATION;
        enterCharacterCreation();
        return;
    }

    if (buttonBPressed())
    {
        playSound(SoundEffect::MENU_SELECT);
        openLoadGameMenu();
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

            case CCS_WEAPON_STYLE_SELECT:
                if (direction == ENCODER_CLOCKWISE)
                    rotateFighterWeaponStyleCW();
                else
                    rotateFighterWeaponStyleCCW();
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

            case CCS_NAME_ENTRY:
                if (direction == ENCODER_CLOCKWISE)
                    rotateCharacterNameCW();
                else
                    rotateCharacterNameCCW();
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

            case CCS_WEAPON_STYLE_SELECT:
                createPreviewCharacter();
                break;

            case CCS_VIEW_STATS:

                // Nothing to select.
                break;

            case CCS_MENU:

                // Menu options are confirmed with the A button so the
                // encoder remains navigation-only during character creation.
                break;

            case CCS_NAME_ENTRY:
                acceptCharacter();
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

                menuSelect();
                playSound(SoundEffect::MENU_SELECT);
                break;

            case CCS_WEAPON_STYLE_SELECT:
                createPreviewCharacter();
                playSound(SoundEffect::MENU_SELECT);
                break;

            case CCS_NAME_ENTRY:
                appendSelectedCharacterNameCharacter();
                playSound(SoundEffect::MENU_SELECT);
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

            case CCS_WEAPON_STYLE_SELECT:
                cancelFighterWeaponStyleSelection();
                playSound(SoundEffect::MENU_BACK);
                break;

            case CCS_VIEW_STATS:

                enterCharacterCreation();

                playSound(SoundEffect::MENU_BACK);
                break;

            case CCS_MENU:

                closeCharacterMenu();
                playSound(SoundEffect::MENU_BACK);
                break;

            case CCS_NAME_ENTRY:
                backspaceCharacterName();
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

        if (buttonAPressed())
        {
            playSound(SoundEffect::MENU_SELECT);

            switch (getTownHomeSelection())
            {
                case TOWN_HOME_REST:
                    beginTownRest();
                    break;

                case TOWN_HOME_SAVE_GAME:
                    openSaveGameMenu();
                    break;

                case TOWN_HOME_CHARACTER_SHEET:
                    openCharacterSheet();
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

    if (buttonAPressed())
    {
        playSound(SoundEffect::MENU_SELECT);

        switch (townSelection)
        {
            case TOWN_FOREST:
                if (!requestMultiplayerTravel(
                        MultiplayerActivityType::FOREST))
                    enterForest();
                break;

            case TOWN_STAY_HOME:
                openTownHome();
                break;

            case TOWN_DUNGEON:
                if (hasResumableDungeon(dungeon))
                    openDungeonEntryMenu();
                else if (!requestMultiplayerTravel(
                             MultiplayerActivityType::DUNGEON))
                    enterDungeon();
                break;

            case TOWN_SHOP:
                openTownShop();
                break;

            case TOWN_BULLETIN_BOARD:
                openMultiplayerMenu();
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
            playSound(SoundEffect::MENU_BACK);
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
            moved = tryMovePlayer(dungeon);
        else if (gameState == GAME_DUNGEON)
            moved = tryMovePlayer(dungeon);

        if (moved)
        {
            Entity* player = getActiveMapPlayer();
            if (gameState != GAME_DUNGEON || player == nullptr ||
                !isCurrentBellListeningRuneAt(player->x, player->y))
                playSound(SoundEffect::WALK);
        }

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
            if (isMultiplayerExplorationActive())
            {
                setGameMessage(
                    "World interactions are paused in multiplayer.");
                openMenu(&mainMenu);
                menuState.redrawType = MENU_REDRAW_FULL;
                return;
            }

            Entity* activePlayer = getActiveMapPlayer();
            if (activePlayer != nullptr &&
                (activePlayer->character.health.currentHP <= 0 ||
                 activePlayer->character.state == STATE_UNCONSCIOUS))
            {
                openMenu(&mainMenu);
                menuState.redrawType = MENU_REDRAW_FULL;
                return;
            }

            // A dead monster at the normal facing cursor is a context
            // interaction, not a reason to open the general menu.
            if (
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
                !(pfme::runtime::nativeRoomsActive() && combat.active && !isPlayerTurn()) &&
#endif
                tryInteractWithFacingEntity())
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

    if (encoderPressed() || buttonAPressed())
    {
        if (activateCharacterSheetSelection())
        {
            playSound(SoundEffect::MENU_SELECT);
            return;
        }
    }
    //--------------------------------------------------
    // Exit
    //--------------------------------------------------

    if (buttonBPressed())
    {
        playSound(SoundEffect::MENU_BACK);
        if (backCharacterSheetSelection())
            return;
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
    handheldInput.reset(digitalRead(ENCODER_CLK)==HIGH, digitalRead(ENCODER_DT)==HIGH,
        digitalRead(ENCODER_SW), digitalRead(BUTTON_A), digitalRead(BUTTON_B), millis());
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

#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
#include "context_fields.h"
namespace pfme { namespace runtime { ButtonInputState& productionButtonState() {return handheldInput;} }}
#endif
