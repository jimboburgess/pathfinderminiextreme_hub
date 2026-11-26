#include "townscene.h"

#include <Adafruit_ST7789.h>
#include <Arduino.h>

#include "data/game.h"
#include "town/town.h"

namespace
{
constexpr uint16_t SKY_TOP = 0x32B5;
constexpr uint16_t SKY_LOW = 0x6538;
constexpr uint16_t HILL_FAR = 0x3B68;
constexpr uint16_t GRASS = 0x34C6;
constexpr uint16_t GRASS_DARK = 0x1B83;
constexpr uint16_t GRASS_LIGHT = 0x5688;
constexpr uint16_t PATH = 0xA514;
constexpr uint16_t PATH_DARK = 0x7B8E;
constexpr uint16_t STONE = 0x8C51;
constexpr uint16_t STONE_DARK = 0x4A49;
constexpr uint16_t STONE_LIGHT = 0xBDF7;
constexpr uint16_t WOOD = 0x82C8;
constexpr uint16_t WOOD_DARK = 0x4964;
constexpr uint16_t WOOD_LIGHT = 0xB48C;
constexpr uint16_t ROOF = 0xA1C5;
constexpr uint16_t ROOF_DARK = 0x60E3;
constexpr uint16_t PLASTER = 0xDE59;
constexpr uint16_t WINDOW = 0xFD24;
constexpr uint16_t WATER = 0x2D5F;
constexpr uint16_t WATER_LIGHT = 0x8E7F;
constexpr uint16_t SHADOW = 0x20A8;
constexpr uint16_t PURPLE_SHADOW = 0x31CC;
constexpr uint16_t TORCH = 0xFD20;

void drawPixelCluster(int16_t x, int16_t y, uint16_t color)
{
    tft.fillRect(x, y, 2, 2, color);
}

void drawTownBackground()
{
    tft.fillRect(0, 0, 240, 28, SKY_TOP);
    tft.fillRect(0, 28, 240, 28, SKY_LOW);
    tft.fillTriangle(0, 56, 61, 22, 118, 56, HILL_FAR);
    tft.fillTriangle(77, 56, 151, 18, 218, 56, HILL_FAR);
    tft.fillTriangle(168, 56, 221, 27, 239, 56, HILL_FAR);
    tft.fillRect(0, 56, 240, TOWN_SCENE_HEIGHT - 56, GRASS);

    tft.fillTriangle(15, 80, 112, 126, 123, 138, PATH);
    tft.fillTriangle(55, 86, 111, 126, 122, 140, PATH);
    tft.fillTriangle(210, 84, 138, 126, 120, 142, PATH);
    tft.fillTriangle(31, 137, 109, 128, 121, 146, PATH);
    tft.fillTriangle(199, 194, 151, 155, 132, 145, PATH);
    tft.fillRect(87, 122, 78, 61, PATH);

    for (int16_t y = 126; y < 181; y += 9)
    {
        const int16_t offset = ((y / 9) & 1) ? 5 : 0;
        for (int16_t x = 90 + offset; x < 163; x += 11)
        {
            tft.drawFastHLine(x, y, 6, PATH_DARK);
            tft.drawPixel(x, y + 1, STONE_LIGHT);
        }
    }

    const int16_t grassDetails[][2] =
    {
        {7, 88}, {33, 93}, {77, 88}, {149, 92}, {229, 96},
        {18, 151}, {61, 164}, {178, 113}, {221, 132}, {74, 188}
    };
    for (const auto& detail : grassDetails)
    {
        drawPixelCluster(detail[0], detail[1], GRASS_DARK);
        tft.drawPixel(detail[0] + 2, detail[1] - 2, GRASS_LIGHT);
    }

    tft.fillCircle(139, 151, 15, STONE_DARK);
    tft.fillCircle(139, 149, 12, STONE_LIGHT);
    tft.fillCircle(139, 149, 9, WATER);
    tft.fillRect(136, 137, 7, 14, STONE);
    tft.fillCircle(139, 136, 5, STONE_LIGHT);
    tft.drawFastHLine(134, 147, 10, WATER_LIGHT);
    tft.drawPixel(137, 142, WATER_LIGHT);
    tft.drawPixel(141, 144, WATER_LIGHT);
}

void drawForest(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    const uint8_t treeX[] = {3, 13, 21, 6, 19};
    const uint8_t treeY[] = {17, 4, 16, 31, 32};
    for (uint8_t index = 0; index < 5; ++index)
    {
        const int16_t treeLeft = x + treeX[index];
        const int16_t treeTop = y + treeY[index];
        tft.fillRect(treeLeft + 6, treeTop + 16, 5, 20, WOOD_DARK);
        tft.fillCircle(treeLeft + 8, treeTop + 12, 11, GRASS_DARK);
        tft.fillCircle(treeLeft + 3, treeTop + 16, 8, 0x2504);
        tft.fillCircle(treeLeft + 13, treeTop + 17, 8, 0x4566);
        tft.drawPixel(treeLeft + 7, treeTop + 7, GRASS_LIGHT);
    }
    tft.fillTriangle(x + 15, y + 68, x + 22, y + 47, x + 29, y + 68, PATH);
    tft.drawFastVLine(x + 14, y + 48, 20, GRASS_DARK);
    tft.drawFastVLine(x + 30, y + 48, 20, GRASS_DARK);
}

void drawBarn(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    tft.fillRect(x + 4, y + 17, 35, 25, 0xA34A);
    tft.fillTriangle(x, y + 18, x + 21, y + 2, x + 43, y + 18, ROOF_DARK);
    tft.fillTriangle(x + 5, y + 17, x + 21, y + 6, x + 38, y + 17, ROOF);
    tft.fillRect(x + 16, y + 23, 12, 19, WOOD_DARK);
    tft.drawLine(x + 16, y + 23, x + 28, y + 42, WOOD_LIGHT);
    tft.drawLine(x + 28, y + 23, x + 16, y + 42, WOOD_LIGHT);
    tft.fillRect(x + 5, y + 24, 7, 7, WINDOW);
    tft.drawRect(x + 5, y + 24, 7, 7, WOOD_DARK);
    tft.fillRect(x, y + 39, 15, 3, WOOD_LIGHT);
    tft.drawFastVLine(x + 2, y + 35, 9, WOOD_DARK);
    tft.drawFastVLine(x + 12, y + 35, 9, WOOD_DARK);
    tft.fillRect(x + 32, y + 34, 8, 6, 0xDDA4);
}

void drawHome(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    tft.fillRect(x + 5, y + 18, 41, 29, PLASTER);
    tft.fillTriangle(x, y + 19, x + 25, y + 1, x + 51, y + 19, ROOF_DARK);
    tft.fillTriangle(x + 5, y + 17, x + 25, y + 5, x + 46, y + 17, ROOF);
    tft.fillRect(x + 39, y + 2, 6, 12, STONE_DARK);
    tft.fillRect(x + 23, y + 28, 10, 19, 0x2453);
    tft.fillRect(x + 9, y + 25, 9, 9, WINDOW);
    tft.fillRect(x + 37, y + 25, 7, 9, WINDOW);
    tft.drawFastVLine(x + 19, y + 18, 29, WOOD_DARK);
    tft.drawFastVLine(x + 36, y + 18, 29, WOOD_DARK);
    tft.drawFastHLine(x + 5, y + 37, 41, WOOD_DARK);
    tft.drawLine(x + 6, y + 19, x + 19, y + 37, WOOD);
    tft.drawLine(x + 46, y + 19, x + 36, y + 37, WOOD);
    tft.drawPixel(x + 30, y + 37, WOOD_LIGHT);
}

void drawCastle(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    tft.fillRect(x + 6, y + 23, 61, 48, STONE);
    tft.fillRect(x + 2, y + 13, 18, 58, STONE_DARK);
    tft.fillRect(x + 53, y + 13, 18, 58, STONE_DARK);
    for (uint8_t tower = 0; tower < 2; ++tower)
    {
        const int16_t towerX = x + (tower == 0 ? 2 : 53);
        tft.fillRect(towerX, y + 8, 6, 8, STONE_LIGHT);
        tft.fillRect(towerX + 12, y + 8, 6, 8, STONE_LIGHT);
        tft.fillRect(towerX + 7, y + 8, 5, 5, STONE_LIGHT);
        tft.fillRect(towerX + 7, y + 29, 6, 12, SHADOW);
    }
    for (int16_t wallX = x + 22; wallX < x + 53; wallX += 12)
        tft.fillRect(wallX, y + 18, 7, 9, STONE_LIGHT);
    tft.fillRoundRect(x + 30, y + 44, 14, 27, 7, SHADOW);
    tft.drawFastVLine(x + 37, y + 48, 23, WOOD);
    tft.fillRect(x + 21, y + 29, 5, 20, 0xB102);
    tft.fillRect(x + 47, y + 29, 5, 20, 0xB102);
    tft.drawFastHLine(x + 6, y + 70, 61, STONE_LIGHT);
}

void drawShop(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    tft.fillRect(x + 4, y + 15, 52, 32, 0xBD11);
    tft.fillTriangle(x, y + 17, x + 29, y, x + 60, y + 17, ROOF_DARK);
    tft.fillTriangle(x + 6, y + 15, x + 29, y + 4, x + 54, y + 15, ROOF);
    for (int16_t stripe = 0; stripe < 5; ++stripe)
    {
        const uint16_t color = (stripe & 1) ? 0xFFFF : 0xC986;
        tft.fillRect(x + 8 + stripe * 9, y + 21, 9, 7, color);
        tft.fillTriangle(x + 8 + stripe * 9, y + 28,
                         x + 12 + stripe * 9, y + 32,
                         x + 17 + stripe * 9, y + 28, color);
    }
    tft.fillRect(x + 12, y + 32, 13, 15, WINDOW);
    tft.fillRect(x + 39, y + 31, 10, 16, WOOD_DARK);
    tft.drawRect(x + 12, y + 32, 13, 15, WOOD_DARK);
    tft.fillCircle(x + 55, y + 42, 5, WOOD);
    tft.drawCircle(x + 55, y + 42, 5, WOOD_LIGHT);
    tft.fillRect(x + 1, y + 40, 8, 7, WOOD);
}

void drawBulletinBoard(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    tft.fillRect(x + 2, y + 5, 23, 21, WOOD_DARK);
    tft.fillRect(x + 5, y + 8, 17, 15, WOOD_LIGHT);
    tft.fillRect(x + 6, y + 9, 6, 7, 0xFF38);
    tft.fillRect(x + 14, y + 12, 7, 9, 0xEED3);
    tft.drawPixel(x + 8, y + 11, 0x0000);
    tft.drawPixel(x + 17, y + 14, 0x0000);
    tft.fillRect(x + 5, y + 26, 4, 9, WOOD_DARK);
    tft.fillRect(x + 19, y + 26, 4, 9, WOOD_DARK);
}

void drawAlley(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    tft.fillRect(x, y + 4, 32, 43, PURPLE_SHADOW);
    tft.fillTriangle(x, y + 4, x + 10, y, x + 10, y + 47, STONE_DARK);
    tft.fillTriangle(x + 32, y + 4, x + 22, y, x + 22, y + 47, STONE_DARK);
    tft.fillTriangle(x + 8, y + 47, x + 16, y + 22, x + 24, y + 47,
                     SHADOW);
    tft.drawFastVLine(x + 6, y + 9, 34, 0x624E);
    tft.drawFastVLine(x + 25, y + 8, 35, 0x624E);
    tft.drawPixel(x + 15, y + 27, 0x801F);
    tft.drawPixel(x + 18, y + 27, 0x801F);
}

void drawDungeon(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    tft.fillTriangle(x, y + 45, x + 34, y + 2, x + 68, y + 45,
                     STONE_DARK);
    tft.fillRoundRect(x + 15, y + 15, 38, 32, 16, STONE);
    tft.fillRoundRect(x + 21, y + 21, 26, 27, 12, SHADOW);
    for (int16_t step = 0; step < 4; ++step)
        tft.fillRect(x + 14 - step * 3, y + 44 + step * 2,
                     40 + step * 6, 2, STONE_LIGHT);
    tft.fillCircle(x + 34, y + 15, 5, 0xFFFF);
    tft.fillCircle(x + 32, y + 14, 1, SHADOW);
    tft.fillCircle(x + 36, y + 14, 1, SHADOW);
    tft.drawFastHLine(x + 32, y + 18, 5, SHADOW);
    tft.fillRect(x + 8, y + 24, 2, 13, WOOD);
    tft.fillRect(x + 58, y + 24, 2, 13, WOOD);
    tft.fillCircle(x + 9, y + 22, 3, TORCH);
    tft.fillCircle(x + 59, y + 22, 3, TORCH);
}

void drawLocationSprite(const TownLocationDefinition& location)
{
    switch (location.sprite)
    {
        case TOWN_SPRITE_FOREST: drawForest(location); break;
        case TOWN_SPRITE_DUNGEON: drawDungeon(location); break;
        case TOWN_SPRITE_SHOP: drawShop(location); break;
        case TOWN_SPRITE_BULLETIN_BOARD: drawBulletinBoard(location); break;
        case TOWN_SPRITE_HOME: drawHome(location); break;
        case TOWN_SPRITE_BARN: drawBarn(location); break;
        case TOWN_SPRITE_CASTLE: drawCastle(location); break;
        case TOWN_SPRITE_MYSTERIOUS_ALLEY: drawAlley(location); break;
    }
}

void drawLocationOutline(const TownLocationDefinition& location)
{
    const int16_t x = location.x;
    const int16_t y = location.y;
    switch (location.sprite)
    {
        case TOWN_SPRITE_FOREST:
            tft.drawCircle(x + 21, y + 16, 11, TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 19, y + 20, 20, TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 23, y + 20, 20, TOWN_SELECTION_COLOR);
            break;
        case TOWN_SPRITE_DUNGEON:
            tft.drawRoundRect(x + 15, y + 15, 38, 32, 16,
                              TOWN_SELECTION_COLOR);
            tft.drawFastHLine(x + 14, y + 44, 40, TOWN_SELECTION_COLOR);
            break;
        case TOWN_SPRITE_CASTLE:
            tft.drawRect(x + 2, y + 13, 18, 58, TOWN_SELECTION_COLOR);
            tft.drawRect(x + 53, y + 13, 18, 58, TOWN_SELECTION_COLOR);
            tft.drawFastHLine(x + 20, y + 23, 33, TOWN_SELECTION_COLOR);
            break;
        case TOWN_SPRITE_SHOP:
            tft.drawLine(x + 6, y + 15, x + 29, y + 4,
                         TOWN_SELECTION_COLOR);
            tft.drawLine(x + 29, y + 4, x + 54, y + 15,
                         TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 4, y + 15, 32,
                              TOWN_SELECTION_COLOR);
            tft.drawFastHLine(x + 4, y + 46, location.width - 8,
                              TOWN_SELECTION_COLOR);
            break;
        case TOWN_SPRITE_HOME:
            tft.drawLine(x + 5, y + 17, x + 25, y + 5,
                         TOWN_SELECTION_COLOR);
            tft.drawLine(x + 25, y + 5, x + 46, y + 17,
                         TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 5, y + 18, 29, TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 45, y + 18, 29, TOWN_SELECTION_COLOR);
            tft.drawFastHLine(x + 5, y + 46, 41, TOWN_SELECTION_COLOR);
            break;
        case TOWN_SPRITE_BARN:
            tft.drawLine(x + 5, y + 17, x + 21, y + 6,
                         TOWN_SELECTION_COLOR);
            tft.drawLine(x + 21, y + 6, x + 38, y + 17,
                         TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 4, y + 17, 25, TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 38, y + 17, 25, TOWN_SELECTION_COLOR);
            tft.drawFastHLine(x + 4, y + 41, 35, TOWN_SELECTION_COLOR);
            break;
        case TOWN_SPRITE_BULLETIN_BOARD:
            tft.drawRect(x + 2, y + 5, 23, 21, TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 5, y + 26, 9, TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 22, y + 26, 9, TOWN_SELECTION_COLOR);
            break;
        case TOWN_SPRITE_MYSTERIOUS_ALLEY:
            tft.drawFastVLine(x + 6, y + 9, 34, TOWN_SELECTION_COLOR);
            tft.drawFastVLine(x + 25, y + 8, 35, TOWN_SELECTION_COLOR);
            tft.drawFastHLine(x + 10, y + 5, 12, TOWN_SELECTION_COLOR);
            break;
    }
}

void drawLocation(const TownLocationDefinition& location, bool selected)
{
    drawLocationSprite(location);
    if (selected) drawLocationOutline(location);
}

void drawWrappedText(const char* text, int16_t x, int16_t y,
                     uint8_t maxColumns, uint8_t maxLines)
{
    if (text == nullptr) return;
    uint8_t column = 0;
    uint8_t line = 0;
    const char* cursor = text;
    tft.setCursor(x, y);
    while (*cursor != '\0' && line < maxLines)
    {
        const char* wordEnd = cursor;
        while (*wordEnd != '\0' && *wordEnd != ' ') ++wordEnd;
        const uint8_t wordLength = static_cast<uint8_t>(wordEnd - cursor);
        if (column != 0 && column + 1 + wordLength > maxColumns)
        {
            ++line;
            if (line >= maxLines) break;
            y += 9;
            tft.setCursor(x, y);
            column = 0;
        }
        if (column != 0)
        {
            tft.print(' ');
            ++column;
        }
        while (cursor < wordEnd)
        {
            tft.print(*cursor++);
            ++column;
        }
        while (*cursor == ' ') ++cursor;
    }
}

void drawTownInformationPanel()
{
    const TownLocationDefinition& location = getTownLocation(townSelection);
    tft.fillRect(0, TOWN_SCENE_HEIGHT, 240, 40, 0x0841);
    tft.drawFastHLine(0, TOWN_SCENE_HEIGHT, 240, STONE_LIGHT);
    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.setTextColor(location.available ? 0xFFFF : 0xFEA0);
    tft.setCursor(5, 203);
    tft.print(location.displayName);
    tft.setTextSize(1);
    tft.setTextColor(0xFFFF);
    drawWrappedText(location.description, 5, 221, 38, 2);
}
}

void drawTownScene()
{
    if (fullRedrawNeeded || redrawType == REDRAW_FULL)
    {
        drawTownBackground();
        for (uint8_t index = 0; index < TOWN_OPTION_COUNT; ++index)
        {
            const TownOption option = static_cast<TownOption>(index);
            drawLocation(getTownLocation(option), option == townSelection);
        }
    }
    else
        drawLocation(getTownLocation(townSelection), true);
    drawTownInformationPanel();
    fullRedrawNeeded = false;
}

void invalidateTownScene()
{
    fullRedrawNeeded = true;
}

void clearTownSceneSelectionHighlight(TownOption option)
{
    if (!fullRedrawNeeded && option < TOWN_OPTION_COUNT)
        drawLocation(getTownLocation(option), false);
}
