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
#include "data/entitytraits.h"
#include "dungeon/abilityresolver.h"
#include "dungeon/combat.h"
#include "map/activemap.h"
#include "dungeon/dungeon.h"
#include "dungeon/roomdraw.h"
#include "forest/forest.h"
#include "map/mapeffects.h"
#include "graphics/tiles.h"
#include "input/inventorymenu.h"
#include "input/menu.h"
#include "town/town.h"


Adafruit_ST7789 tft(
    TFT_CS,
    TFT_DC,
    TFT_RST);

namespace
{
constexpr uint16_t COLOR_GREASE = 0xB5A6;
constexpr uint16_t COLOR_WEB = 0xCE79;
constexpr uint16_t COLOR_AREA_TARGET = 0xC81F;

void getDamageFlashColors(DamageType type, uint16_t& primary, uint16_t& highlight)
{
    primary = ST77XX_WHITE;
    highlight = 0x7BEF;
    switch (type)
    {
        case DAMAGE_FIRE: primary = ST77XX_RED; highlight = ST77XX_YELLOW; break;
        case DAMAGE_COLD: primary = ST77XX_BLUE; highlight = ST77XX_WHITE; break;
        case DAMAGE_ELECTRIC: primary = ST77XX_YELLOW; highlight = ST77XX_WHITE; break;
        case DAMAGE_ACID: primary = ST77XX_GREEN; highlight = 0xAFE5; break;
        case DAMAGE_SONIC: primary = 0xC81F; highlight = ST77XX_WHITE; break;
        case DAMAGE_POSITIVE: primary = ST77XX_WHITE; highlight = ST77XX_YELLOW; break;
        case DAMAGE_NEGATIVE: primary = 0x8010; highlight = 0xC81F; break;
        default: break;
    }
}

void drawMapEffectOverlayAt(int tileX, int tileY)
{
    int screenX = tileX * TILE_SIZE;
    int screenY = tileY * TILE_SIZE;

    // Render every active overlay that covers this tile. Mechanical queries
    // already support overlaps; drawing them all keeps future effects from
    // being hidden merely because another effect occupied an earlier slot.
    for (uint8_t i = 0; i < MAX_MAP_EFFECTS; i++)
    {
        const MapEffect& effect = activeMapEffects[i];

        if (!mapEffectAffectsTile(effect, tileX, tileY))
            continue;

        switch (effect.type)
        {
            case MAP_EFFECT_GREASE:
                // Sparse highlights leave the original forest/dungeon tile
                // visible beneath the temporary overlay.
                tft.drawLine(screenX + 2, screenY + 12,
                             screenX + 6, screenY + 8, COLOR_GREASE);
                tft.drawLine(screenX + 7, screenY + 13,
                             screenX + 13, screenY + 7, COLOR_GREASE);
                tft.drawPixel(screenX + 4, screenY + 4, COLOR_GREASE);
                tft.drawPixel(screenX + 11, screenY + 3, COLOR_GREASE);
                break;

            case MAP_EFFECT_WEB:
                tft.drawLine(screenX + 1, screenY + 1,
                             screenX + 14, screenY + 14, COLOR_WEB);
                tft.drawLine(screenX + 14, screenY + 1,
                             screenX + 1, screenY + 14, COLOR_WEB);
                tft.drawLine(screenX + 1, screenY + 8,
                             screenX + 14, screenY + 8, COLOR_WEB);
                tft.drawLine(screenX + 8, screenY + 1,
                             screenX + 8, screenY + 14, COLOR_WEB);
                break;

            case MAP_EFFECT_WALL_OF_FIRE:
                tft.drawLine(screenX + 2, screenY + 14,
                             screenX + 5, screenY + 3, ST77XX_RED);
                tft.drawLine(screenX + 6, screenY + 14,
                             screenX + 9, screenY + 1, 0xFD20);
                tft.drawLine(screenX + 10, screenY + 14,
                             screenX + 13, screenY + 4, ST77XX_YELLOW);
                tft.drawPixel(screenX + 4, screenY + 7, ST77XX_YELLOW);
                tft.drawPixel(screenX + 11, screenY + 9, ST77XX_RED);
                break;

            case MAP_EFFECT_ACID_FOG:
                tft.fillCircle(screenX + 4, screenY + 5, 2, ST77XX_GREEN);
                tft.fillCircle(screenX + 10, screenY + 8, 3, 0xAFE5);
                tft.drawLine(screenX + 2, screenY + 12,
                             screenX + 13, screenY + 12, ST77XX_GREEN);
                tft.drawPixel(screenX + 13, screenY + 4, 0xAFE5);
                break;

            case MAP_EFFECT_BLADE_BARRIER:
                tft.drawLine(screenX + 2, screenY + 13,
                             screenX + 13, screenY + 2, ST77XX_WHITE);
                tft.drawLine(screenX + 2, screenY + 2,
                             screenX + 13, screenY + 13, 0xC618);
                tft.drawLine(screenX + 1, screenY + 8,
                             screenX + 14, screenY + 8, ST77XX_WHITE);
                break;

            case MAP_EFFECT_NONE:
                break;
        }
    }
}

void drawGroundAbilityCursorTile(int tileX, int tileY)
{
    int targetX = 0;
    int targetY = 0;
    const Ability* ability = getAbility(combat.selectedAbility);

    if (ability == nullptr ||
        !isInsideActiveMap(tileX, tileY) ||
        !getSelectedAbilityGroundTarget(targetX, targetY) ||
        abs(tileX - targetX) > ability->areaRadiusTiles ||
        abs(tileY - targetY) > ability->areaRadiusTiles)
    {
        return;
    }

    int minimumX = targetX - ability->areaRadiusTiles;
    int maximumX = targetX + ability->areaRadiusTiles;
    int minimumY = targetY - ability->areaRadiusTiles;
    int maximumY = targetY + ability->areaRadiusTiles;

    if (minimumX < 0)
        minimumX = 0;
    if (minimumY < 0)
        minimumY = 0;
    if (maximumX >= getActiveMapWidth())
        maximumX = getActiveMapWidth() - 1;
    if (maximumY >= getActiveMapHeight())
        maximumY = getActiveMapHeight() - 1;

    int screenX = tileX * TILE_SIZE;
    int screenY = tileY * TILE_SIZE;

    // Draw only the outside edge, producing one clear perimeter around the
    // complete affected area instead of a grid of individually boxed tiles.
    if (tileY == minimumY)
        tft.drawLine(screenX, screenY,
                     screenX + TILE_SIZE - 1, screenY,
                     COLOR_AREA_TARGET);
    if (tileY == maximumY)
        tft.drawLine(screenX, screenY + TILE_SIZE - 1,
                     screenX + TILE_SIZE - 1,
                     screenY + TILE_SIZE - 1,
                     COLOR_AREA_TARGET);
    if (tileX == minimumX)
        tft.drawLine(screenX, screenY,
                     screenX, screenY + TILE_SIZE - 1,
                     COLOR_AREA_TARGET);
    if (tileX == maximumX)
        tft.drawLine(screenX + TILE_SIZE - 1, screenY,
                     screenX + TILE_SIZE - 1,
                     screenY + TILE_SIZE - 1,
                     COLOR_AREA_TARGET);

    if (tileX != targetX || tileY != targetY)
        return;

    // The arrow is derived from the generic targeting direction and stays in
    // the center tile, showing where the next encoder click will move the AoE.
    const DirectionOffset& direction =
        directionOffsets[combat.selectedAbilityDirection];
    int centerX = screenX + TILE_SIZE / 2;
    int centerY = screenY + TILE_SIZE / 2;
    int endX = centerX + direction.dx * 5;
    int endY = centerY + direction.dy * 5;
    int baseX = endX - direction.dx * 3;
    int baseY = endY - direction.dy * 3;
    int perpendicularX = -direction.dy * 2;
    int perpendicularY = direction.dx * 2;

    tft.drawLine(centerX, centerY, endX, endY, COLOR_AREA_TARGET);
    tft.drawLine(endX, endY,
                 baseX + perpendicularX,
                 baseY + perpendicularY,
                 COLOR_AREA_TARGET);
    tft.drawLine(endX, endY,
                 baseX - perpendicularX,
                 baseY - perpendicularY,
                 COLOR_AREA_TARGET);
}

void drawDirectionalAbilityCursorTile(int tileX, int tileY)
{
    Entity* caster = getActiveMapPlayer();

    if (caster == nullptr || !isPlayerTargetingDirectionalAbility() ||
        !isTileInDirectionalAbilityArea(
            *caster,
            combat.selectedAbility,
            combat.selectedAbilityDirection,
            tileX,
            tileY))
    {
        return;
    }

    // Every effective cone tile receives the same purple outline. Because
    // this calls the resolver's geometry predicate, walls and bounds affect
    // the preview and the actual cast identically.
    tft.drawRect(
        tileX * TILE_SIZE + 1,
        tileY * TILE_SIZE + 1,
        TILE_SIZE - 2,
        TILE_SIZE - 2,
        COLOR_AREA_TARGET);
}
}

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
        tft.setCursor(20, 65);
        tft.print(getTownHomeSelection() == TOWN_HOME_REST ? "> " : "  ");
        tft.print("Rest");

        tft.setCursor(20, 95);
        tft.print(getTownHomeSelection() == TOWN_HOME_SAVE_GAME ? "> " : "  ");
        tft.print("Save Game");

        tft.setCursor(20, 125);
        tft.print(getTownHomeSelection() == TOWN_HOME_CHARACTER_SHEET
            ? "> " : "  ");
        tft.print("Character Sheet");

        tft.setCursor(20, 155);
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

            for (int y = 0; y < ROOM_SIZE; y++)
            {
                for (int x = 0; x < ROOM_SIZE; x++)
                    drawMapEffectOverlayAt(x, y);
            }

            break;

        default:

            break;
    }
}

void drawMapEntities()
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities == nullptr)
        return;

    for (uint8_t i = 0; i < entityCount; i++)
        drawEntity(entities[i]);
}

static void drawMoveCursor()
{
    Entity* player = getActiveMapPlayer();

    if (player == nullptr)
        return;

    const int cursorX =
        player->x + directionOffsets[moveDirection].dx;
    const int cursorY =
        player->y + directionOffsets[moveDirection].dy;

    tft.drawRect(
        cursorX * TILE_SIZE,
        cursorY * TILE_SIZE,
        TILE_SIZE,
        TILE_SIZE,
        ST77XX_WHITE);
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
            Entity* player = getActiveMapPlayer();

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

    if (isPlayerTargetingAbility())
    {
        if (isPlayerTargetingDirectionalAbility())
        {
            for (int y = 0; y < getActiveMapHeight(); y++)
            {
                for (int x = 0; x < getActiveMapWidth(); x++)
                    drawDirectionalAbilityCursorTile(x, y);
            }

            return;
        }

        if (isPlayerTargetingGroundAbility())
        {
            int targetX = 0;
            int targetY = 0;
            const Ability* ability = getAbility(combat.selectedAbility);

            if (ability != nullptr &&
                getSelectedAbilityGroundTarget(targetX, targetY))
            {
                for (int y = targetY - ability->areaRadiusTiles;
                     y <= targetY + ability->areaRadiusTiles;
                     y++)
                {
                    for (int x = targetX - ability->areaRadiusTiles;
                         x <= targetX + ability->areaRadiusTiles;
                         x++)
                    {
                        drawGroundAbilityCursorTile(x, y);
                    }
                }
            }

            return;
        }

        Entity* target = getSelectedAbilityTarget();

        if (target != nullptr)
        {
            tft.drawRect(
                target->x * TILE_SIZE,
                target->y * TILE_SIZE,
                target->spriteWidth,
                target->spriteHeight,
                ST77XX_YELLOW);
        }

        return;
    }

    drawMoveCursor();
}

void redrawMapMessage()
{
    if (gameState != GAME_FOREST && gameState != GAME_DUNGEON)
        return;

    tft.fillRect(0, 224, 240, 16, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(2, 228);
    tft.print(getGameMessage());
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

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    drawRoomTile(room, x, y);
    drawMapEffectOverlayAt(x, y);

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

    if (isPlayerTargetingAbility())
    {
        if (isPlayerTargetingDirectionalAbility())
        {
            drawDirectionalAbilityCursorTile(x, y);
            return;
        }

        if (isPlayerTargetingGroundAbility())
        {
            drawGroundAbilityCursorTile(x, y);
            return;
        }

        Entity* target = getSelectedAbilityTarget();

        if (target != nullptr && entityOccupiesTile(*target, x, y))
        {
            tft.drawRect(target->x * TILE_SIZE, target->y * TILE_SIZE,
                         target->spriteWidth, target->spriteHeight,
                         ST77XX_YELLOW);
        }

        return;
    }

    if (isPlayerTargetingAttack())
    {
        Entity* target = getSelectedAttackTarget();

        if (combat.attackType == COMBAT_ATTACK_MELEE)
        {
            Entity* player = getActiveMapPlayer();

            if (player != nullptr &&
                player->x + directionOffsets[moveDirection].dx == x &&
                player->y + directionOffsets[moveDirection].dy == y)
            {
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

    Entity* player = getActiveMapPlayer();

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
    redrawMapMessage();
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
    redrawMapMessage();
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

    drawMapEffectOverlayAt(x, y);
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

    if (isPlayerTargetingAbility())
    {
        if (isPlayerTargetingDirectionalAbility())
        {
            drawDirectionalAbilityCursorTile(x, y);
            return;
        }

        if (isPlayerTargetingGroundAbility())
        {
            drawGroundAbilityCursorTile(x, y);
            return;
        }

        Entity* target = getSelectedAbilityTarget();

        if (target != nullptr && entityOccupiesTile(*target, x, y))
        {
            tft.drawRect(target->x * TILE_SIZE, target->y * TILE_SIZE,
                         target->spriteWidth, target->spriteHeight,
                         ST77XX_YELLOW);
        }

        return;
    }

    if (isPlayerTargetingAttack())
    {
        Entity* target = getSelectedAttackTarget();

        if (combat.attackType == COMBAT_ATTACK_MELEE)
        {
            Entity* player = getActiveMapPlayer();

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

    Entity* player = getActiveMapPlayer();

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

    if (entity.type == ENTITY_PUZZLE_KEY)
    {
        const int x = entity.x * TILE_SIZE;
        const int y = entity.y * TILE_SIZE;
        tft.drawCircle(x + 5, y + 6, 3, ST77XX_YELLOW);
        tft.drawLine(x + 8, y + 8, x + 13, y + 13, ST77XX_YELLOW);
        tft.drawLine(x + 11, y + 11, x + 13, y + 9, ST77XX_YELLOW);
        return;
    }

    if (entity.type == ENTITY_MONSTER && entity.character.state == STATE_ALIVE)
    {
        if (entity.sprite == nullptr)
            return;

        Entity* player = getActiveMapPlayer();
        const bool hasCurrentLineOfSight = player != nullptr &&
            canSee(*player) && hasLineOfSightBetweenFootprintsAt(
                *player, player->x, player->y, entity);
        if (!hasCurrentLineOfSight)
        {
            drawSpriteGrayscaleTransparent(
                entity.x * TILE_SIZE,
                entity.y * TILE_SIZE,
                entity.sprite,
                entity.spriteWidth,
                entity.spriteHeight);
            return;
        }
    }

    if (entity.sprite == nullptr)
        return;

    drawSpriteTransparent(
        entity.x * TILE_SIZE,
        entity.y * TILE_SIZE,
        entity.sprite,
        entity.spriteWidth,
        entity.spriteHeight);

    if (entity.character.state == STATE_DEAD ||
        entity.character.state == STATE_TURNED ||
        (entity.type == ENTITY_PLAYER &&
         entity.character.state == STATE_UNCONSCIOUS))
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

    redrawMapMessage();
}

  void playAreaDamageFlash(
    DamageType damageType,
    const AreaFlashTile* tiles,
    uint8_t tileCount)
{
    if (tiles == nullptr || tileCount == 0)
        return;

    uint16_t primary = ST77XX_WHITE;
    uint16_t highlight = ST77XX_WHITE;
    getDamageFlashColors(damageType, primary, highlight);
    const uint16_t colors[] = { primary, highlight, primary };
    for (uint8_t phase = 0; phase < 3; phase++)
    {
        for (uint8_t i = 0; i < tileCount; i++)
            tft.fillRect(tiles[i].x * TILE_SIZE, tiles[i].y * TILE_SIZE,
                         TILE_SIZE, TILE_SIZE, colors[phase]);
        delay(40);
    }
    // Restore every flashed tile immediately. Large cones can exceed the
    // bounded dirty-tile queue, so using that queue here could leave the last
    // few tiles painted with the flash color until a later full redraw.
    for (uint8_t i = 0; i < tileCount; i++)
    {
        if (gameState == GAME_DUNGEON)
            redrawDungeonTile(tiles[i].x, tiles[i].y);
        else if (gameState == GAME_FOREST)
            redrawForestTile(tiles[i].x, tiles[i].y);
    }
    redrawMapMessage();
  }

  void playAbilityImpactFlash(AbilityImpactVisual visual,
                              DamageType damageType,
                              int tileX, int tileY)
  {
      uint16_t primary = 0xC81F;
      uint16_t highlight = ST77XX_WHITE;
      if (visual == IMPACT_DAMAGE)
          getDamageFlashColors(damageType, primary, highlight);
      else if (visual == IMPACT_HEAL)
      {
          primary = damageType == DAMAGE_NEGATIVE ? 0x8010 : ST77XX_GREEN;
          highlight = ST77XX_WHITE;
      }
      else if (visual == IMPACT_BUFF)
      {
          primary = ST77XX_BLUE;
          highlight = ST77XX_WHITE;
      }
      const AreaFlashTile tile = { static_cast<int8_t>(tileX),
                                   static_cast<int8_t>(tileY) };
      const uint16_t colors[] = { primary, highlight, primary };
      for (uint8_t phase = 0; phase < 3; phase++)
      {
          tft.fillRect(tile.x * TILE_SIZE, tile.y * TILE_SIZE,
                       TILE_SIZE, TILE_SIZE, colors[phase]);
          delay(40);
      }
      markTileDirty(tile.x, tile.y);
      redrawDirtyTiles();
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

void drawSpriteGrayscaleTransparent(
    int x, int y, const uint16_t* sprite, uint8_t width, uint8_t height)
{
    if (sprite == nullptr)
        return;

    tft.startWrite();
    for (uint8_t row = 0; row < height; row++)
    {
        for (uint8_t col = 0; col < width; col++)
        {
            const uint16_t color = pgm_read_word(&sprite[row * width + col]);
            if (color == 0xF81F)
                continue;

            const uint8_t red = ((color >> 11) & 0x1F) * 255 / 31;
            const uint8_t green = ((color >> 5) & 0x3F) * 255 / 63;
            const uint8_t blue = (color & 0x1F) * 255 / 31;
            const uint8_t gray =
                (static_cast<uint16_t>(red) * 30 +
                 static_cast<uint16_t>(green) * 59 +
                 static_cast<uint16_t>(blue) * 11) / 100;
            tft.writePixel(x + col, y + row,
                ((gray >> 3) << 11) |
                ((gray >> 2) << 5) |
                (gray >> 3));
        }
    }
    tft.endWrite();
}
