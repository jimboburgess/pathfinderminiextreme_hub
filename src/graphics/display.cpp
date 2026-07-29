#include "display.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include "config.h"
#include "data/game.h"

#include "audio/audio.h"
#include "charcreationscreen.h"
#include "messagelog.h"
#include "monstersprites.h"
#include "graphics/sprites.h"
#include "characters/sheet.h"
#include "data/entityspawn.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "dungeon/dungeonplayer.h"
#include "dungeon/roomdraw.h"
#include "dungeon/forest.h"
#include "graphics/tiles.h"
#include "input/menu.h"


Adafruit_ST7789 tft(
    TFT_CS,
    TFT_DC,
    TFT_RST);

void drawStartScreen()
{
    tft.fillScreen(ST77XX_BLACK);

    playSound(SoundEffect::TITLE_THEME);

    drawSpriteTransparent64(8,   95, ROGUE64X64);
    drawSpriteTransparent64(64,  95, FIGHTERWALK1);
    drawSpriteTransparent64(120, 95, CLERIC64X64);
    drawSpriteTransparent64(176, 95, WIZARD64X64);

    tft.setTextColor(ST77XX_WHITE);

    tft.setTextSize(3);
    tft.setCursor(30, 30);
    tft.println("Pathfinder");

    tft.setCursor(5, 60);
    tft.println("Mini EXTREME!");

    tft.setTextSize(2);
    tft.setCursor(30, 205);
    tft.println("Press A Button");
}

void drawStartAnimation()
{
    static bool lastFrame = false;

    bool frame = (millis() / 800) % 2;

    if (frame == lastFrame)
        return;

    lastFrame = frame;

    const uint16_t* fighterSprite =
        frame ? FIGHTERWALK1 : FIGHTERWALK2;

    tft.fillRect(64, 95, 64, 64, ST77XX_BLACK);

    drawSpriteTransparent64(
        64,
        95,
        fighterSprite);
}

void drawTownScreen()
{
    if (isCharacterSheetVisible())
    {
        drawCharacterSheet();
        return;
    }
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);

    tft.setTextSize(3);
    tft.setCursor(72, 20);
    tft.print("Town");

    tft.setTextSize(2);

    tft.setCursor(20, 70);
    tft.print(townSelection == TOWN_GOBLINS ? "> " : "  ");
    tft.print("Enter the forest");

    tft.setCursor(20, 100);
    tft.print(townSelection == TOWN_STAY_HOME ? "> " : "  ");
    tft.print("Stay Home");

    tft.setCursor(20, 130);
    tft.print(townSelection == TOWN_DUNGEON ? "> " : "  ");
    tft.print("Explore Dungeon");

    if (townSelection == TOWN_STAY_HOME)
    {
        tft.setTextSize(1);
        tft.setCursor(18, 205);
        tft.print("\"It's dangerous out there.\"");
    }
}

void drawForestScreen()
{
    Serial.println("DRAWING ENTIRE FOREST");

    for (int y = 0; y < FOREST_HEIGHT; y++)
    {
        for (int x = 0; x < FOREST_WIDTH; x++)
        {
            redrawForestTile(x, y);
        }
    }

    redrawForestMessage();
}

void redrawForestMessage()
{
    tft.fillRect(0, 224, 240, 16, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(2, 228);
    tft.print(getGameMessage());
}

void drawForestTile(int x, int y)
{

    int screenX = x * TILE_SIZE;
    int screenY = y * TILE_SIZE;
    // Draw the ground first.
    drawSpriteTransparent(screenX, screenY, grassTile);

    switch (getForestTile(x, y))
    {
        case TILE_TREE:
            drawSpriteTransparent(screenX, screenY, treeTile);
            break;

        default:
            break;
    }
}

void redrawForestTile(int x, int y)
{
    if (x < 0 || x >= FOREST_WIDTH ||
        y < 0 || y >= FOREST_HEIGHT)
    {
        return;
    }

    //--------------------------------------------------
    // Draw the map tile.
    //--------------------------------------------------

    drawForestTile(x, y);

    //--------------------------------------------------
    // Draw any entity on this tile.
    //--------------------------------------------------

    Entity* entity = getEntityAt(
        forestEntities,
        forestEntityCount,
        x,
        y);

    if (entity != nullptr)
    {
        drawEntity(*entity);
    }

    //--------------------------------------------------
    // Draw the movement cursor if it is on this tile.
    //--------------------------------------------------

    Entity* player = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    if (player != nullptr)
    {
        int cursorX =
            player->x + directionOffsets[moveDirection].dx;

        int cursorY =
            player->y + directionOffsets[moveDirection].dy;

        if (cursorX == x &&
            cursorY == y)
        {
            tft.drawRect(
                x * TILE_SIZE,
                y * TILE_SIZE,
                TILE_SIZE,
                TILE_SIZE,
                ST77XX_WHITE);
        }
    }
}

void drawEntity(const Entity& entity)
{
    if (!entity.active)
        return;

    if (entity.sprite == nullptr)
        return;

    drawSpriteTransparent(
        entity.x * TILE_SIZE,
        entity.y * TILE_SIZE,
        entity.sprite,
        entity.spriteWidth,
        entity.spriteHeight);
}

void redrawDirtyTiles()
{
    for (uint8_t i = 0; i < dirtyTileCount; i++)
    {
        switch (gameState)
        {
            case GAME_FOREST:
                redrawForestTile(
                    dirtyTiles[i].x,
                    dirtyTiles[i].y);
                break;

                // case GAME_DUNGEON:
                //     redrawDungeonTile(
                //         dirtyTiles[i].x,
                //         dirtyTiles[i].y);
                //     break;

            default:
                break;
        }
    }

    dirtyTileCount = 0;

    if (gameState == GAME_FOREST)
    {
        redrawForestMessage();
    }
}

DirtyTile dirtyTiles[MAX_DIRTY_TILES];
uint8_t dirtyTileCount = 0;

void markTileDirty(int x, int y)
{
    if (gameState == GAME_FOREST)
    {
        if (x < 0 || x >= FOREST_WIDTH ||
            y < 0 || y >= FOREST_HEIGHT)
        {
            return;
        }
    }

    // Don't add duplicates.
    for (uint8_t i = 0; i < dirtyTileCount; i++)
    {
        if (dirtyTiles[i].x == x &&
            dirtyTiles[i].y == y)
        {
            return;
        }
    }

    if (dirtyTileCount >= MAX_DIRTY_TILES)
        return;

    dirtyTiles[dirtyTileCount].x = x;
    dirtyTiles[dirtyTileCount].y = y;
    dirtyTileCount++;

    needsRedraw = true;
}

void drawDungeonScreen()
{
    if (isCharacterSheetVisible())
    {
        drawCharacterSheet();
        return;
    }
    drawRoom(dungeon.rooms[dungeon.currentRoom]);

    for (uint8_t i = 0; i < dungeon.entityCount; i++)
    {
        drawEntity(dungeon.entities[i]);
    }

    drawMoveCursor(dungeon);


}

void drawSpriteTransparent(
    int x,
    int y,
    const uint16_t* sprite,
    uint8_t width,
    uint8_t height)
{
    for (uint8_t row = 0; row < height; row++)
    {
        for (uint8_t col = 0; col < width; col++)
        {
            uint16_t color = pgm_read_word(&sprite[row * width + col]);

            if (color != 0xF81F)
            {
                tft.drawPixel(x + col, y + row, color);
            }
        }
    }
}

void drawSpriteTransparent(int x, int y, const uint16_t* sprite)
{
    drawSpriteTransparent(
        x,
        y,
        sprite,
        SPRITE_W,
        SPRITE_H);
}

void drawSpriteTransparent64(int x, int y, const uint16_t* sprite)
{
    for (int py = 0; py < 64; py++)
    {
        for (int px = 0; px < 64; px++)
        {
            uint16_t color =
                pgm_read_word(&sprite[py * 64 + px]);

            if (color != 0xF81F)
            {
                tft.drawPixel(
                    x + px,
                    y + py,
                    color);
            }
        }
    }
}



void refreshDisplay()
{
    //--------------------------------------------------
    // Draw the current game screen
    //--------------------------------------------------

    switch (gameState)
    {
        case GAME_START:
            drawStartScreen();
            break;

        case GAME_CHARACTER_CREATION:
            drawCharacterCreationScreen();
            break;

        case GAME_TOWN:
            drawTownScreen();
            break;

        case GAME_FOREST:

            if (redrawType == REDRAW_FULL)
            {
                drawForestScreen();
                dirtyTileCount = 0;
            }
            else
            {
                redrawDirtyTiles();
            }

            break;

        case GAME_DUNGEON:
            drawDungeonScreen();
            break;

        default:
            break;
    }

    //--------------------------------------------------
    // Draw menu overlay
    //--------------------------------------------------

    if (menuState.isOpen)
    {
        drawMenu();
    }
    //--------------------------------------------------
    // combat border
    //--------------------------------------------------

    if (combat.active)
    {
        tft.drawRect(
            0,
            0,
            240,
            240,
            ST77XX_RED);
    }

    //--------------------------------------------------
    // Reset redraw flags
    //--------------------------------------------------

    redrawType = REDRAW_NONE;
    needsRedraw = false;
}