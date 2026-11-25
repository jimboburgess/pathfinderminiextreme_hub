#ifndef PATHFINDERMINIEXTREME_025_SHOP_H
#define PATHFINDERMINIEXTREME_025_SHOP_H

#include <stdint.h>

#include "characters/items.h"

struct Character;
struct Menu;

constexpr uint8_t MAX_SHOP_ITEMS = 12;

struct Shop
{
    ItemInstance stock[MAX_SHOP_ITEMS];
    uint8_t stockCount;
};

extern const Shop townShop;

enum ShopPurchaseResult
{
    SHOP_PURCHASE_SUCCESS,
    SHOP_PURCHASE_INVALID_ITEM,
    SHOP_PURCHASE_NOT_ENOUGH_GOLD,
    SHOP_PURCHASE_INVENTORY_FULL
};

ShopPurchaseResult purchaseShopItem(Character& character,
                                    const ItemInstance& instance,
                                    uint16_t price);

void openTownShop();
void openShopBuyMenu();
void openShopSellMenu();

void buyShopItem(uint8_t menuIndex);
void sellShopItem(uint8_t menuIndex);
void completeShopEquipPrompt(bool equipNow);
bool cancelShopEquipPrompt(const Menu* menu);
const char* getShopStatusMessage();

#endif // PATHFINDERMINIEXTREME_025_SHOP_H
