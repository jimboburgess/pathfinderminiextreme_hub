#include "graphics/itemdropvisual.h"

#include <Adafruit_ST7789.h>

#include "config.h"
#include "data/entities.h"
#include "dungeon/dungeon.h"
#include "graphics/display.h"
#include "graphics/tiles.h"
#include "map/worlditems.h"

uint16_t getDroppedPotionColor(ItemID itemID)
{
    if (itemID == ITEM_POTION_CURE_LIGHT_WOUNDS)
        return HEALING_POTION_DROP_COLOR;
    if (itemID == ITEM_MANA_POTION)
        return MANA_POTION_DROP_COLOR;
    return 0xF81F;
}

namespace
{
void drawPotion(int x, int y, uint16_t color)
{
    tft.fillRect(x + 6, y + 3, 4, 3, 0xA145);
    tft.drawRect(x + 5, y + 5, 6, 8, ST77XX_WHITE);
    tft.fillRect(x + 6, y + 7, 4, 5, color);
    tft.drawPixel(x + 7, y + 8, ST77XX_WHITE);
}

void drawWeapon(int x, int y)
{
    tft.drawLine(x + 4, y + 12, x + 11, y + 5, 0xC618);
    tft.drawLine(x + 5, y + 13, x + 12, y + 6, ST77XX_WHITE);
    tft.drawLine(x + 3, y + 10, x + 7, y + 14, 0xA145);
}

void drawArmor(int x, int y)
{
    tft.fillTriangle(x + 8, y + 3, x + 3, y + 6,
                     x + 13, y + 6, 0x7BEF);
    tft.fillTriangle(x + 3, y + 6, x + 13, y + 6,
                     x + 8, y + 13, 0xC618);
    tft.drawLine(x + 8, y + 5, x + 8, y + 11, ST77XX_WHITE);
}

void drawScroll(int x, int y)
{
    tft.fillRect(x + 4, y + 4, 8, 9, 0xFFE0);
    tft.drawLine(x + 6, y + 7, x + 10, y + 7, 0xA145);
    tft.drawLine(x + 6, y + 9, x + 10, y + 9, 0xA145);
    tft.drawCircle(x + 4, y + 5, 2, 0xFD20);
    tft.drawCircle(x + 12, y + 12, 2, 0xFD20);
}

void drawBag(int x, int y)
{
    tft.fillTriangle(x + 8, y + 4, x + 4, y + 12,
                     x + 12, y + 12, 0xA145);
    tft.drawLine(x + 6, y + 5, x + 10, y + 5, 0xFFE0);
    tft.drawRect(x + 4, y + 8, 9, 5, 0xFD20);
}
}

void drawDroppedItem(const Entity& entity)
{
    const InventorySlot* slot = getDroppedWorldItem(entity);
    const Item* item = slot != nullptr ? getItem(slot->item.itemID) : nullptr;
    if (item == nullptr) return;
    const int x = entity.x * TILE_SIZE;
    const int y = entity.y * TILE_SIZE;
    switch (item->type)
    {
        case ITEMTYPE_POTION:
            drawPotion(x, y, getDroppedPotionColor(slot->item.itemID));
            break;
        case ITEMTYPE_WEAPON:
            drawWeapon(x, y);
            break;
        case ITEMTYPE_ARMOR:
        case ITEMTYPE_SHIELD:
            drawArmor(x, y);
            break;
        case ITEMTYPE_SCROLL:
            drawScroll(x, y);
            break;
        default:
            drawBag(x, y);
            break;
    }
}
