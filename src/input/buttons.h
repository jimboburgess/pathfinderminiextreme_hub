//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_BUTTONS_H
#define PATHFINDERMINIEXTREME_025_BUTTONS_H

#include <Arduino.h>
#include "config.h"
#include "audio/audio.h"
#include "graphics/charcreationscreen.h"
#include "dungeon/dungeon.h"
#include "dungeon/dungeonplayer.h"
#include "data/game.h"

enum EncoderDirection
{
    ENCODER_NONE,
    ENCODER_CLOCKWISE,
    ENCODER_COUNTERCLOCKWISE
};

EncoderDirection readEncoder();

// Main input handler
void handleButtons();

bool encoderPressed();
bool buttonAPressed();
bool buttonBPressed();
bool encoderButtonLongPressed();

// Stops the encoder's select switch from activating a freshly opened menu
// with the same physical press that opened it.
void suppressEncoderSelectUntilRelease();

// Individual state handlers
void handleStartButtons();
void handleCharacterCreationButtons();
void handleTownButtons();
void handleMapButtons();
void handleCharacterSheetButtons();
void resetButtonStates();

#endif // PATHFINDERMINIEXTREME_025_BUTTONS_H
