#ifndef PATHFINDERMINIEXTREME_025_SHOP_H
#define PATHFINDERMINIEXTREME_025_SHOP_H

#include <stdint.h>

#include "characters/items.h"

constexpr uint8_t MAX_SHOP_ITEMS = 12;

struct Shop
{
    ItemInstance stock[MAX_SHOP_ITEMS];
    uint8_t stockCount;
};

extern const Shop townShop;

void openTownShop();
void openShopBuyMenu();
void openShopSellMenu();

void buyShopItem(uint8_t menuIndex);
void sellShopItem(uint8_t menuIndex);

#endif // PATHFINDERMINIEXTREME_025_SHOP_H
