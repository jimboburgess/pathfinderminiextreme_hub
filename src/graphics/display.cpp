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
#include "input/inventorymenu.h"
#include "input/menu.h"
#include "town/town.h"


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
    tft.setCursor(30, 175);
    tft.println("A: New Game");

    tft.setCursor(30, 200);
    tft.println("B: Load Game");

    tft.setTextSize(1);
    tft.setCursor(30, 226);
    tft.print(getGameMessage());
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

    if (isTownHomeOpen())
    {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(3);
        tft.setCursor(72, 20);
        tft.print("Home");

        tft.setTextSize(2);
        tft.setCursor(20, 75);
        tft.print(getTownHomeSelection() == TOWN_HOME_REST ? "> " : "  ");
        tft.print("Rest");

        tft.setCursor(20, 110);
        tft.print(getTownHomeSelection() == TOWN_HOME_SAVE_GAME ? "> " : "  ");
        tft.print("Save Game");

        tft.setCursor(20, 145);
        tft.print(getTownHomeSelection() == TOWN_HOME_BACK ? "> " : "  ");
        tft.print("Back");

        tft.setTextSize(1);
        tft.setCursor(12, 205);
        tft.print(getGameMessage());
        return;
    }

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);

    tft.setTextSize(3);
    tft.setCursor(72, 20);
    tft.print("Town");

    tft.setTextSize(2);

    tft.setCursor(20, 60);
    tft.print(townSelection == TOWN_FOREST ? "> " : "  ");
    tft.print("Enter the Forest");

    tft.setCursor(20, 90);
    tft.print(townSelection == TOWN_DUNGEON ? "> " : "  ");
    tft.print("Explore Dungeon");

    tft.setCursor(20, 120);
    tft.print(townSelection == TOWN_SHOP ? "> " : "  ");
    tft.print("Shop");

    tft.setCursor(20, 150);
    tft.print(townSelection == TOWN_STAY_HOME ? "> " : "  ");
    tft.print("Stay Home");

    if (townSelection == TOWN_STAY_HOME)
    {
        tft.setTextSize(1);
        tft.setCursor(18, 205);
        tft.print("\"It's dangerous out there.\"");
    }
}

void drawMapBackground()
{
    switch (gameState)
    {
        case GAME_FOREST:

            for (int y = 0; y < FOREST_HEIGHT; y++)
            {
                for (int x = 0; x < FOREST_WIDTH; x++)
                {
                    drawForestTile(x, y);
                }
            }

            break;

        case GAME_DUNGEON:

            drawRoom(
                dungeon.rooms[dungeon.currentRoom]);

            break;

        default:

            break;
    }
}

void drawMapEntities()
{
    switch (gameState)
    {
        case GAME_FOREST:

            for (uint8_t i = 0;
                 i < forestEntityCount;
                 i++)
            {
                drawEntity(
                    forestEntities[i]);
            }

            break;

        case GAME_DUNGEON:

            for (uint8_t i = 0;
                 i < dungeon.entityCount;
                 i++)
            {
                drawEntity(
                    dungeon.entities[i]);
            }

            break;

        default:

            break;
    }
}

void drawMapCursor()
{
    if (isInspectingEntities())
    {
        Entity* entity = getInspectedEntity();

        if (entity != nullptr)
        {
            tft.drawRect(
                entity->x * TILE_SIZE,
                entity->y * TILE_SIZE,
                TILE_SIZE,
                TILE_SIZE,
                ST77XX_YELLOW);
        }

        return;
    }

    if (isPlayerTargetingAttack())
    {
        Entity* target = getSelectedAttackTarget();

        if (combat.attackType == COMBAT_ATTACK_MELEE)
        {
            Entity* player = getPlayerEntity(
                forestEntities,
                forestEntityCount);

            if (player != nullptr)
            {
                int cursorX =
                    player->x + directionOffsets[moveDirection].dx;
                int cursorY =
                    player->y + directionOffsets[moveDirection].dy;

                tft.drawRect(
                    cursorX * TILE_SIZE,
                    cursorY * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE,
                    ST77XX_YELLOW);
            }
        }
        else if (target != nullptr)
        {
            // Ranged attacks select the creature as a whole.  Outline its
            // full footprint so a large target is clearly selectable.
            tft.drawRect(
                target->x * TILE_SIZE,
                target->y * TILE_SIZE,
                target->spriteWidth,
                target->spriteHeight,
                ST77XX_YELLOW);
        }

        return;
    }

    switch (gameState)
    {
        case GAME_FOREST:

        {
            Entity* player =
                getPlayerEntity(
                    forestEntities,
                    forestEntityCount);

            if (player)
            {
                int cursorX =
                    player->x +
                    directionOffsets[moveDirection].dx;

                int cursorY =
                    player->y +
                    directionOffsets[moveDirection].dy;

                tft.drawRect(
                    cursorX * TILE_SIZE,
                    cursorY * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE,
                    ST77XX_WHITE);
            }

            break;
        }

        case GAME_DUNGEON:

            drawMoveCursor(dungeon);

            break;

        default:

            break;
    }
}

void drawMapMessage()
{
    switch (gameState)
    {
        case GAME_FOREST:

            redrawForestMessage();

            break;

        case GAME_DUNGEON:

            redrawDungeonMessage();

            break;

        default:

            break;
    }
}

void redrawDungeonTile(int x, int y)
{
    if (x < 0 || x >= ROOM_SIZE ||
        y < 0 || y >= ROOM_SIZE)
    {
        return;
    }

    //--------------------------------------------------
    // Draw the map tile.
    //--------------------------------------------------

    drawTile(
        x,
        y,
        dungeon.rooms[dungeon.currentRoom].map.tiles[y][x]);

    //--------------------------------------------------
    // Draw any entity on this tile.
    //--------------------------------------------------

    Entity* entity = getEntityAt(
        dungeon.entities,
        dungeon.entityCount,
        x,
        y);

    if (entity != nullptr)
    {
        drawEntity(*entity);
    }

    if (isInspectingEntities())
    {
        Entity* inspected = getInspectedEntity();

        if (inspected != nullptr &&
            inspected->x == x && inspected->y == y)
        {
            tft.drawRect(x * TILE_SIZE, y * TILE_SIZE,
                         TILE_SIZE, TILE_SIZE, ST77XX_YELLOW);
        }

        return;
    }

    //--------------------------------------------------
    // Draw the movement cursor if it is on this tile.
    //--------------------------------------------------

    Entity* player = getPlayerEntity(
        dungeon.entities,
        dungeon.entityCount);

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

void redrawDungeonMessage()
{
    tft.fillRect(0, 224, 240, 16, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(2, 228);
    tft.print(getGameMessage());
}

void drawDungeonScreen()
{
    if (isCharacterSheetVisible())
    {
        drawCharacterSheet();
        return;
    }

    if (backgroundNeedsRedraw)
    {
        drawMapBackground();
        backgroundNeedsRedraw = false;
    }

    drawMapEntities();
    drawMapCursor();
    drawMapMessage();
}


void drawForestScreen()
{
    if (isCharacterSheetVisible())
    {
        drawCharacterSheet();
        return;
    }

    if (backgroundNeedsRedraw)
    {
        Serial.println("DRAWING FOREST BACKGROUND");
        drawMapBackground();
        backgroundNeedsRedraw = false;
    }

    drawMapEntities();
    drawMapCursor();
    drawMapMessage();
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

    if (isInspectingEntities())
    {
        Entity* inspected = getInspectedEntity();

        if (inspected != nullptr &&
            inspected->x == x && inspected->y == y)
        {
            tft.drawRect(x * TILE_SIZE, y * TILE_SIZE,
                         TILE_SIZE, TILE_SIZE, ST77XX_YELLOW);
        }

        return;
    }

    if (isPlayerTargetingAttack())
    {
        Entity* target = getSelectedAttackTarget();

        if (combat.attackType == COMBAT_ATTACK_MELEE)
        {
            Entity* player = getPlayerEntity(
                forestEntities, forestEntityCount);

            if (player != nullptr &&
                player->x + directionOffsets[moveDirection].dx == x &&
                player->y + directionOffsets[moveDirection].dy == y)
            {
                // Keep the cursor on the exact square selected by the
                // player, even when that square is part of a larger sprite.
                tft.drawRect(x * TILE_SIZE, y * TILE_SIZE,
                             TILE_SIZE, TILE_SIZE, ST77XX_YELLOW);
            }
        }
        else if (target != nullptr && entityOccupiesTile(*target, x, y))
        {
            tft.drawRect(target->x * TILE_SIZE, target->y * TILE_SIZE,
                         target->spriteWidth, target->spriteHeight,
                         ST77XX_YELLOW);
        }

        return;
    }

    //--------------------------------------------------
    // Draw the movement cursor if it is on this tile.
    //--------------------------------------------------

    Entity* player = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    if (player != nullptr)
    {
        int cursorX = player->x + directionOffsets[moveDirection].dx;
        int cursorY = player->y + directionOffsets[moveDirection].dy;

        if (cursorX == x && cursorY == y)
        {
            tft.drawRect(x * TILE_SIZE, y * TILE_SIZE,
                         TILE_SIZE, TILE_SIZE, ST77XX_WHITE);
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

    if (entity.character.state == STATE_DEAD)
    {
        int x = entity.x * TILE_SIZE;
        int y = entity.y * TILE_SIZE;

        tft.drawLine(x + 2, y + 2,
                     x + entity.spriteWidth - 3,
                     y + entity.spriteHeight - 3,
                     ST77XX_RED);
        tft.drawLine(x + entity.spriteWidth - 3, y + 2,
                     x + 2, y + entity.spriteHeight - 3,
                     ST77XX_RED);
    }
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

            case GAME_DUNGEON:
                redrawDungeonTile(
                    dirtyTiles[i].x,
                    dirtyTiles[i].y);
                break;

            default:
                break;
        }
    }

    dirtyTileCount = 0;

    switch (gameState)
    {
        case GAME_FOREST:
            redrawForestMessage();
            break;

        case GAME_DUNGEON:
            redrawDungeonMessage();
            break;

        default:
            break;
    }
}

DirtyTile dirtyTiles[MAX_DIRTY_TILES];
uint8_t dirtyTileCount = 0;

bool backgroundNeedsRedraw = true;

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

void markEntityFootprintDirtyAt(const Entity& entity, int x, int y)
{
    for (uint8_t offsetY = 0;
         offsetY < getEntityTileHeight(entity);
         offsetY++)
    {
        for (uint8_t offsetX = 0;
             offsetX < getEntityTileWidth(entity);
             offsetX++)
        {
            markTileDirty(x + offsetX, y + offsetY);
        }
    }
}

void markEntityFootprintDirty(const Entity& entity)
{
    markEntityFootprintDirtyAt(entity, entity.x, entity.y);
}


void drawSpriteTransparent(
    int x,
    int y,
    const uint16_t* sprite,
    uint8_t width,
    uint8_t height)
{
    tft.startWrite();

    for (uint8_t row = 0; row < height; row++)
    {
        for (uint8_t col = 0; col < width; col++)
        {
            uint16_t color =
                pgm_read_word(&sprite[row * width + col]);

            if (color != 0xF81F)
            {
                tft.writePixel(
                    x + col,
                    y + row,
                    color);
            }
        }
    }

    tft.endWrite();
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

void drawSpriteTransparent64(
    int x,
    int y,
    const uint16_t* sprite)
{
    drawSpriteTransparent(
        x,
        y,
        sprite,
        START_W,
        START_H);
}

void refreshDisplay()
{
    // Inventory/loot is an opaque, full-screen modal. It owns the display
    // until it closes, at which point it schedules a normal full map redraw.
    if (isInventoryMenuOpen())
    {
        drawInventoryMenu();
        redrawType = REDRAW_NONE;
        needsRedraw = false;
        return;
    }

    // The sheet is an overlay-independent full-screen view. Drawing it here
    // lets both the menu action and the encoder long-press work from every
    // map state, even when no map redraw has been requested.
    if (isCharacterSheetVisible())
    {
        drawCharacterSheet();
        redrawType = REDRAW_NONE;
        needsRedraw = false;
        return;
    }

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
            if (!menuState.isOpen ||
                menuState.redrawType == MENU_REDRAW_FULL)
            {
                drawTownScreen();
            }
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

            if (redrawType == REDRAW_FULL)
            {
                drawDungeonScreen();
                dirtyTileCount = 0;
            }
            else
            {
                redrawDirtyTiles();
            }

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
