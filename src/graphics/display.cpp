#include "display.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include "config.h"
#include "data/game.h"

#include "audio/audio.h"
#include "charcreationscreen.h"
#include "monstersprites.h"
#include "graphics/sprites.h"
#include "characters/sheet.h"
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
    for (int y = 0; y < FOREST_HEIGHT; y++)
    {
        for (int x = 0; x < FOREST_WIDTH; x++)
        {
            redrawForestTile(x, y);
        }
    }
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

void drawEntity(const Entity& entity)
{
    switch (entity.type)
    {
        case ENTITY_PLAYER:
            drawSpriteTransparent(
                entity.x * TILE_SIZE,
                entity.y * TILE_SIZE,
                fighterSprite16x16);
            break;

        case ENTITY_ENEMY:
            switch (entity.monsterID)
            {
            case MONSTER_GOBLIN_SCIMITAR:
                    drawSpriteTransparent(
                        entity.x * TILE_SIZE,
                        entity.y * TILE_SIZE,
                        goblinSprite16x16r1);
                    break;

            case MONSTER_GOBLIN_ARCHER:
                    drawSpriteTransparent(
                        entity.x * TILE_SIZE,
                        entity.y * TILE_SIZE,
                        goblinArcher16x16);
                    break;

            case MONSTER_BUGBEAR:
                    drawSpriteTransparent(
                        entity.x * TILE_SIZE,
                        entity.y * TILE_SIZE,
                        bugbear16x16);
                    break;

            default:
                    break;
            }
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

    // Draw the map tile first.
    drawForestTile(x, y);

    // Draw any entity standing on this tile.
    Entity* entity = getEntityAt(
        forestEntities,
        forestEntityCount,
        x,
        y);

    if (entity)
    {
        drawEntity(*entity);
    }

    // Draw the movement cursor if it belongs here.
    int cursorX =
        playerPosition.x +
        directionOffsets[moveDirection].dx;

    int cursorY =
        playerPosition.y +
        directionOffsets[moveDirection].dy;

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

void redrawForestCursor()
{
    int oldX =
        playerPosition.x +
        directionOffsets[previousMoveDirection].dx;

    int oldY =
        playerPosition.y +
        directionOffsets[previousMoveDirection].dy;

    int newX =
        playerPosition.x +
        directionOffsets[moveDirection].dx;

    int newY =
        playerPosition.y +
        directionOffsets[moveDirection].dy;

    redrawForestTile(oldX, oldY);
    redrawForestTile(newX, newY);
}

void redrawForestMovement()
{
    // Redraw the old player tile.
    redrawForestTile(
        previousPlayerPosition.x,
        previousPlayerPosition.y);

    // Redraw the old cursor tile.
    redrawForestTile(
        previousPlayerPosition.x +
            directionOffsets[previousMoveDirection].dx,
        previousPlayerPosition.y +
            directionOffsets[previousMoveDirection].dy);

    // Redraw the new player tile.
    redrawForestTile(
        playerPosition.x,
        playerPosition.y);

    // Redraw the new cursor tile.
    redrawForestTile(
        playerPosition.x +
            directionOffsets[moveDirection].dx,
        playerPosition.y +
            directionOffsets[moveDirection].dy);
}
void drawDungeonScreen()
{
    if (isCharacterSheetVisible())
    {
        drawCharacterSheet();
        return;
    }

    drawRoom(dungeon.rooms[dungeon.currentRoom]);

    drawEntities(dungeon);

    drawMoveCursor(dungeon);
}

void drawSpriteTransparent(int x, int y, const uint16_t* sprite)
{
    for (int py = 0; py < SPRITE_H; py++)
    {
        for (int px = 0; px < SPRITE_W; px++)
        {
            uint16_t color =
                pgm_read_word(&sprite[py * SPRITE_W + px]);

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

            switch (redrawType)
            {
            case REDRAW_FULL:
                    drawForestScreen();
                    break;

            case REDRAW_CURSOR:
                    redrawForestCursor();
                    break;

            case REDRAW_PLAYER:
                    redrawForestMovement();
                    break;

            default:
                    break;
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
    // Reset redraw flags
    //--------------------------------------------------

    redrawType = REDRAW_NONE;
    needsRedraw = false;
}