#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "config.h"
#include "data/game.h"
#include "audio/audio.h"
#include "characters/characters.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "dungeon/dungeonplayer.h"
#include "dungeon/roomdraw.h"
#include "dungeon/roomgen.h"
#include "graphics/charcreationscreen.h"
#include "graphics/display.h"
#include "graphics/sprites.h"
#include "dungeon/forest.h"

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

    // Generate dungeon
    generateDungeon(dungeon);

    // Create monsters
    drawRoom(dungeon.rooms[dungeon.currentRoom]);
    printRoom(dungeon.rooms[dungeon.currentRoom]);

    Serial.println("Dungeon:");

    for (int i = 0; i < MAX_ROOMS; i++)
    {
        Serial.print("Room ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(roomTypeName(dungeon.rooms[i].type));
    }

    // Initialize controls
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);

    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);

    needsRedraw = true;
}

void loop()
{
    handleButtons();

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