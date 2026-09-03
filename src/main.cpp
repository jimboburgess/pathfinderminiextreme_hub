#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "config.h"
#include "data/game.h"
#include "audio/audio.h"
#include "characters/characters.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "map/awareness.h"
#include "map/monsteridle.h"
#include "graphics/elementalvisual.h"
#include "map/playermovement.h"
#include "dungeon/roomdraw.h"
#include "dungeon/roomgen.h"
#include "graphics/charcreationscreen.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "graphics/sprites.h"
#include "forest/forest.h"
#include "town/town.h"

#include "input/buttons.h"

void setup()
{
    Serial.begin(115200);

    // Turn on backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Start SPI
    SPI.begin(
        TFT_SCL,
        -1,
        TFT_SDA,
        TFT_CS);

    // Initialize display
    tft.init(240, 240);
    tft.setSPISpeed(40000000);
    tft.setRotation(0);

    // Initialize audio
    initAudio();

    // Seed random number generator
    randomSeed(esp_random());

    // No dungeon run exists until the player chooses Explore Dungeon.
    resetDungeonRun(dungeon);

    // Initialize controls
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);

    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);

    resetButtonStates();

    needsRedraw = true;
}

void loop()
{
    handleButtons();

    updateAwareness();
    updateElementalTrapCharges();
    updateElementalVisualEffect();
    updateMonsterIdleBehavior();

    if (combat.active)
    {
        updateCombat();
    }

    updateMonsterVisibility();

    updateGameMessage();
    updateTownRest();

    updateAudio();

    if (needsRedraw)
    {
        refreshDisplay();
    }

    if (gameState == GAME_START)
    {
        drawStartAnimation();
    }
}
