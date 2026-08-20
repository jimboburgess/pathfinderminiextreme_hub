#include "shop.h"

#include <cstdio>

#include "audio/audio.h"
#include "characters/characters.h"
#include "data/game.h"
#include "graphics/display.h"
#include "input/menu.h"
#include "map/skillactions.h"

const Shop townShop =
{
    {
        { ITEM_DAGGER, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_MACE, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_LONGSWORD, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_SHORTBOW, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_LIGHT_CROSSBOW, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_LEATHER_ARMOR, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_CHAINMAIL, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_HEAVY_WOODEN_SHIELD, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_POTION_CURE_LIGHT_WOUNDS, 0, WEAPON_ENHANCEMENT_NONE },
        { ITEM_MANA_POTION, 0, WEAPON_ENHANCEMENT_NONE }
    },
    MAX_SHOP_ITEMS
};

namespace
{
constexpr uint8_t SHOP_LABEL_LENGTH = 32;
constexpr uint8_t SHOP_STATUS_LENGTH = 64;

char shopGoldText[24] = "";
char shopStatus[SHOP_STATUS_LENGTH] = "";

char buyLabels[MAX_SHOP_ITEMS][SHOP_LABEL_LENGTH];
MenuItem buyMenuItems[MAX_SHOP_ITEMS];
uint8_t buyStockIndices[MAX_SHOP_ITEMS];

char sellLabels[MAX_INVENTORY][SHOP_LABEL_LENGTH];
MenuItem sellMenuItems[MAX_INVENTORY];
uint8_t sellInventoryIndices[MAX_INVENTORY];

const MenuItem shopMenuItems[] =
{
    {
        "Buy",
        "Browse the shop's fixed stock.",
        MENU_SHOP_BUY,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Sell",
        "Sell carried items for half value.",
        MENU_SHOP_SELL,
        nullptr,
        MENU_CLASS_ALL
    },
    {
        "Leave",
        "Return to town.",
        MENU_SHOP_LEAVE,
        nullptr,
        MENU_CLASS_ALL
    }
};

Menu shopMenu =
{
    "Shop",
    shopMenuItems,
    sizeof(shopMenuItems) / sizeof(shopMenuItems[0]),
    shopGoldText,
    shopStatus
};

Menu buyMenu =
{
    "Buy",
    buyMenuItems,
    0,
    shopGoldText,
    shopStatus
};

Menu sellMenu =
{
    "Sell",
    sellMenuItems,
    0,
    shopGoldText,
    shopStatus
};

void setShopStatus(const char* message)
{
    snprintf(shopStatus, sizeof(shopStatus), "%s",
             message != nullptr ? message : "");
}

void updateShopGoldText()
{
    snprintf(shopGoldText, sizeof(shopGoldText), "Gold: %lu",
             static_cast<unsigned long>(player.inventory.gold));
}

bool isSellable(const Item* item)
{
    return item != nullptr && item->value > 0 && item->value / 2 > 0 &&
           item->type != ITEMTYPE_NONE && item->type != ITEMTYPE_QUEST;
}

void buildBuyMenu()
{
    buyMenu.itemCount = 0;

    for (uint8_t i = 0;
         i < townShop.stockCount && i < MAX_SHOP_ITEMS;
         i++)
    {
        const Item* item = getItem(townShop.stock[i].itemID);

        if (item == nullptr || item->value == 0)
            continue;

        uint8_t menuIndex = buyMenu.itemCount++;
        buyStockIndices[menuIndex] = i;
        snprintf(buyLabels[menuIndex], sizeof(buyLabels[menuIndex]),
                 "%-20.20s %u gp", item->name,
                 static_cast<unsigned>(item->value));

        buyMenuItems[menuIndex] =
        {
            buyLabels[menuIndex],
            item->description,
            MENU_SHOP_BUY_ITEM,
            nullptr,
            MENU_CLASS_ALL
        };
    }
}

void buildSellMenu()
{
    sellMenu.itemCount = 0;

    for (uint8_t inventoryIndex = 0;
         inventoryIndex < player.inventory.itemCount;
         inventoryIndex++)
    {
        const InventorySlot& slot =
            player.inventory.slots[inventoryIndex];
        const Item* item = getItem(slot.item.itemID);

        if (!isSellable(item))
            continue;

        uint8_t menuIndex = sellMenu.itemCount++;
        sellInventoryIndices[menuIndex] = inventoryIndex;
        uint16_t sellPrice = item->value / 2;

        if (slot.quantity > 1)
        {
            snprintf(sellLabels[menuIndex], sizeof(sellLabels[menuIndex]),
                     "%.16s x%u %u gp", item->name,
                     static_cast<unsigned>(slot.quantity),
                     static_cast<unsigned>(sellPrice));
        }
        else
        {
            snprintf(sellLabels[menuIndex], sizeof(sellLabels[menuIndex]),
                     "%-20.20s %u gp", item->name,
                     static_cast<unsigned>(sellPrice));
        }

        sellMenuItems[menuIndex] =
        {
            sellLabels[menuIndex],
            item->description,
            MENU_SHOP_SELL_ITEM,
            nullptr,
            MENU_CLASS_ALL
        };
    }

    if (sellMenu.itemCount == 0)
    {
        snprintf(sellLabels[0], sizeof(sellLabels[0]), "Nothing to sell");
        sellInventoryIndices[0] = MAX_INVENTORY;
        sellMenuItems[0] =
        {
            sellLabels[0],
            "No eligible carried items.",
            MENU_NONE,
            nullptr,
            MENU_CLASS_ALL
        };
        sellMenu.itemCount = 1;
    }
}

void requestShopRedraw()
{
    menuState.previousCursorIndex = menuState.cursorIndex;
    menuState.redrawType = MENU_REDRAW_FULL;
    needsRedraw = true;
}

void clampSellMenuSelection()
{
    if (menuState.cursorIndex >= sellMenu.itemCount)
        menuState.cursorIndex = sellMenu.itemCount - 1;

    if (menuState.firstVisibleIndex > menuState.cursorIndex)
        menuState.firstVisibleIndex = menuState.cursorIndex;

    if (menuState.cursorIndex >=
        menuState.firstVisibleIndex + MENU_VISIBLE_ITEMS)
    {
        menuState.firstVisibleIndex =
            menuState.cursorIndex - MENU_VISIBLE_ITEMS + 1;
    }
}
}

void openTownShop()
{
    updateShopGoldText();
    buildBuyMenu();
    buildSellMenu();
    setShopStatus(getShopDiplomacyMessage(
        resolveAutomaticSocialCheck(player, SKILL_DIPLOMACY, 15)));
    openMenu(&shopMenu);
    menuState.redrawType = MENU_REDRAW_FULL;
}

void openShopBuyMenu()
{
    updateShopGoldText();
    buildBuyMenu();
    setShopStatus("Select an item to buy.");
    pushMenu(&buyMenu);
}

void openShopSellMenu()
{
    updateShopGoldText();
    buildSellMenu();
    setShopStatus("Select an item to sell.");
    pushMenu(&sellMenu);
}

void buyShopItem(uint8_t menuIndex)
{
    if (menuIndex >= buyMenu.itemCount)
        return;

    uint8_t stockIndex = buyStockIndices[menuIndex];

    if (stockIndex >= townShop.stockCount)
        return;

    const ItemInstance& instance = townShop.stock[stockIndex];
    const Item* item = getItem(instance.itemID);

    if (item == nullptr || item->value == 0)
    {
        setShopStatus("That item is not for sale.");
        playSound(SoundEffect::ERROR);
        requestShopRedraw();
        return;
    }

    if (player.inventory.gold < item->value)
    {
        setShopStatus("Not enough gold.");
        playSound(SoundEffect::ERROR);
        requestShopRedraw();
        return;
    }

    if (!addItem(player, instance))
    {
        setShopStatus("Inventory full.");
        playSound(SoundEffect::ERROR);
        requestShopRedraw();
        return;
    }

    player.inventory.gold -= item->value;
    updateShopGoldText();
    buildSellMenu();
    snprintf(shopStatus, sizeof(shopStatus), "Bought %s.", item->name);
    playSound(SoundEffect::ITEM_PICKUP);
    requestShopRedraw();
}

void sellShopItem(uint8_t menuIndex)
{
    if (menuIndex >= sellMenu.itemCount)
        return;

    uint8_t inventoryIndex = sellInventoryIndices[menuIndex];

    if (inventoryIndex >= player.inventory.itemCount)
        return;

    ItemInstance instance = player.inventory.slots[inventoryIndex].item;
    const Item* item = getItem(instance.itemID);

    if (!isSellable(item))
    {
        setShopStatus("That item cannot be sold.");
        playSound(SoundEffect::ERROR);
        requestShopRedraw();
        return;
    }

    uint16_t sellPrice = item->value / 2;

    if (!removeItem(player, instance))
    {
        setShopStatus("Unable to sell that item.");
        playSound(SoundEffect::ERROR);
        requestShopRedraw();
        return;
    }

    if (sellPrice > UINT32_MAX - player.inventory.gold)
        player.inventory.gold = UINT32_MAX;
    else
        player.inventory.gold += sellPrice;

    updateShopGoldText();
    snprintf(shopStatus, sizeof(shopStatus), "Sold %s for %u gp.",
             item->name, static_cast<unsigned>(sellPrice));
    buildSellMenu();
    clampSellMenuSelection();
    playSound(SoundEffect::ITEM_PICKUP);
    requestShopRedraw();
}
