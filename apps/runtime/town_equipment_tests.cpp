#include "runtime.h"
#include "controller_slots.h"
#include "characters/charactercreation.h"
#include "characters/sheet.h"
#include "data/game.h"
#include "data/savegame.h"
#include "input/menu.h"
#include "town/shop.h"
#include "town/town.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace pfme::runtime;
namespace fs = std::filesystem;

namespace
{
void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

void wait(unsigned milliseconds)
{
    while (milliseconds != 0)
    {
        const unsigned step = std::min(milliseconds, 10u);
        advance(step);
        milliseconds -= step;
    }
}

void tap(uint8_t controller, const char* action)
{
    check(controllerInput(controller, action, true), "input down");
    wait(30);
    check(controllerInput(controller, action, false), "input up");
    wait(310);
}

void setEnvironment(const char* key, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}

void resetEquipment(Character& character)
{
    for (uint8_t slot = 0; slot < NUM_EQUIPMENT_SLOTS; ++slot)
        character.equipment.equipped[slot] = makeItemInstance(ITEM_NONE);
}

void prepareTownCharacter(const char* name)
{
    createCharacter(player, CLASS_FIGHTER, WEAPON_GROUP_NONE);
    setCharacterName(player, name);
    clearInventory(player.inventory);
    resetEquipment(player);
    gameState = GAME_TOWN;
    closeMenu();
    closeTownHome();
    closeCharacterSheet();
    townSelection = TOWN_STAY_HOME;
    releaseInputs();
    wait(350);
}

void chooseMenuIndex(uint8_t controller, uint8_t index)
{
    check(menuState.isOpen && menuState.depth != 0, "menu open");
    while (menuState.cursorIndex < index) tap(controller, "cw");
    while (menuState.cursorIndex > index) tap(controller, "ccw");
    tap(controller, "a");
}

void openHomeEquipment(uint8_t controller)
{
    townSelection = TOWN_STAY_HOME;
    if (!isTownHomeOpen()) tap(controller, "a");
    check(isTownHomeOpen(), "Stay Home opens");
    while (getTownHomeSelection() != TOWN_HOME_EQUIPMENT)
        tap(controller, "cw");
    tap(controller, "a");
    check(isCharacterSheetVisible(), "Equipment opens from Stay Home");
}

void equipCurrentSlot(uint8_t controller)
{
    tap(controller, "a");
    tap(controller, "a");
}

void testHomeEquipment(uint8_t controller)
{
    check(addItem(player, ITEM_LONGSWORD), "add melee");
    check(addItem(player, ITEM_SHORTBOW), "add ranged");
    check(addItem(player, ITEM_CHAINMAIL), "add armor");
    check(addItem(player, ITEM_HEAVY_WOODEN_SHIELD), "add shield");

    const int armorBefore = getArmorClass(player);
    openHomeEquipment(controller);
    equipCurrentSlot(controller);
    check(getEquippedItem(player, SLOT_MELEE_WEAPON) == ITEM_LONGSWORD,
          "Town equips melee weapon");
    tap(controller, "cw");
    equipCurrentSlot(controller);
    check(getEquippedItem(player, SLOT_RANGED_WEAPON) == ITEM_SHORTBOW,
          "Town equips ranged weapon");
    tap(controller, "cw");
    equipCurrentSlot(controller);
    check(getEquippedItem(player, SLOT_ARMOR) == ITEM_CHAINMAIL,
          "Town equips armor");
    check(getArmorClass(player) > armorBefore,
          "armor class refreshes immediately");
    tap(controller, "cw");
    equipCurrentSlot(controller);
    check(getEquippedItem(player, SLOT_SHIELD) == ITEM_HEAVY_WOODEN_SHIELD,
          "Town equips shield");

    tap(controller, "b");
    check(!isCharacterSheetVisible(), "Equipment returns to Home");

    check(addItem(player, ITEM_LEATHER_ARMOR), "add replacement armor");
    openHomeEquipment(controller);
    tap(controller, "cw");
    tap(controller, "cw");
    equipCurrentSlot(controller);
    check(getEquippedItem(player, SLOT_ARMOR) == ITEM_LEATHER_ARMOR &&
              hasItem(player, ITEM_CHAINMAIL),
          "Town atomically replaces armor");
    tap(controller, "b");

    ItemInstance enhanced = makeItemInstance(ITEM_LONGSWORD);
    enhanced.enhancementBonus = 2;
    enhanced.weaponProperties = WEAPON_PROPERTY_FLAMING;
    check(addItem(player, enhanced), "add enhanced weapon");
    const int attackBefore = getMeleeAttackBonus(player);
    openHomeEquipment(controller);
    equipCurrentSlot(controller);
    check(player.equipment.equipped[SLOT_MELEE_WEAPON] == enhanced,
          "replacement preserves exact enhanced instance");
    check(hasItem(player, ITEM_LONGSWORD),
          "replaced weapon safely returns to inventory");
    check(getMeleeAttackBonus(player) > attackBefore,
          "weapon bonus refreshes immediately");

    check(activateCharacterSheetSelection(), "open melee item list");
    uint8_t compatibleMelee = 0;
    for (uint8_t index = 0; index < player.inventory.itemCount; ++index)
        if (isItemCompatibleWithEquipmentSlot(
                player.inventory.slots[index].item, SLOT_MELEE_WEAPON))
            ++compatibleMelee;
    for (uint8_t index = 0; index < compatibleMelee; ++index)
        scrollCharacterSheetDown();
    check(activateCharacterSheetSelection(), "activate melee unequip");
    check(getEquippedItem(player, SLOT_MELEE_WEAPON) == ITEM_NONE,
          "Town unequips weapon");
    check(hasItem(player, enhanced), "unequipped exact instance returns");

    check(equipItem(player, ITEM_HEAVY_WOODEN_SHIELD) == false,
          "already equipped shield is not in inventory");
    ItemInstance greatsword = makeItemInstance(ITEM_GREATSWORD);
    check(addItem(player, greatsword), "add two-handed weapon");
    const InventoryData inventoryBefore = player.inventory;
    const ItemInstance shieldBefore =
        player.equipment.equipped[SLOT_SHIELD];
    check(equipItemWithResult(player, greatsword) ==
              EQUIP_TWO_HANDED_CONFLICT,
          "two-handed conflict rejected");
    check(player.equipment.equipped[SLOT_SHIELD] == shieldBefore &&
              player.inventory.itemCount == inventoryBefore.itemCount &&
              hasItem(player, greatsword),
          "rejected operation changes nothing");
    tap(controller, "b");
}

void testShop(uint8_t controller)
{
    clearInventory(player.inventory);
    resetEquipment(player);
    player.inventory.gold = 10000;
    const uint32_t startingGold = player.inventory.gold;
    const uint16_t daggerPrice = getItem(ITEM_DAGGER)->value;

    openTownShop();
    wait(350);
    chooseMenuIndex(controller, 0);
    chooseMenuIndex(controller, 0);
    check(std::string(menuState.menuStack[menuState.depth - 1]->title) ==
              "Equip Now?",
          "equippable purchase prompts");
    check(player.inventory.gold == startingGold - daggerPrice &&
              getItemQuantity(player, ITEM_DAGGER) == 1,
          "purchase charges and creates exactly once");
    chooseMenuIndex(controller, 0);
    check(getEquippedItem(player, SLOT_MELEE_WEAPON) == ITEM_DAGGER &&
              getItemQuantity(player, ITEM_DAGGER) == 0,
          "Equip Now equips purchased instance");
    check(std::string(menuState.menuStack[0]->title) == "Shop" &&
              menuState.depth == 1,
          "successful equip returns to shop");

    chooseMenuIndex(controller, 0);
    chooseMenuIndex(controller, 0);
    check(player.inventory.gold == startingGold - 2 * daggerPrice,
          "second purchase charges once");
    chooseMenuIndex(controller, 1);
    check(getEquippedItem(player, SLOT_MELEE_WEAPON) == ITEM_DAGGER &&
              getItemQuantity(player, ITEM_DAGGER) == 1,
          "declining keeps item in inventory");

    chooseMenuIndex(controller, 0);
    const uint16_t potionBefore = getItemQuantity(
        player, ITEM_POTION_CURE_LIGHT_WOUNDS);
    chooseMenuIndex(controller, 8);
    check(std::string(menuState.menuStack[menuState.depth - 1]->title) ==
              "Buy",
          "potion purchase has no equipment prompt");
    check(getItemQuantity(player, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
              potionBefore + 1,
          "potion purchase succeeds once");

    tap(controller, "b");
    ItemInstance greatsword = makeItemInstance(ITEM_GREATSWORD);
    check(addItem(player, greatsword) && equipItem(player, greatsword),
          "shop conflict fixture");
    const uint16_t shieldBefore = getItemQuantity(
        player, ITEM_HEAVY_WOODEN_SHIELD);
    chooseMenuIndex(controller, 0);
    chooseMenuIndex(controller, 7);
    chooseMenuIndex(controller, 0);
    check(getEquippedItem(player, SLOT_MELEE_WEAPON) == ITEM_GREATSWORD &&
              getItemQuantity(player, ITEM_HEAVY_WOODEN_SHIELD) ==
                  shieldBefore + 1 &&
              std::string(getShopStatusMessage()).find("Remove") !=
                  std::string::npos,
          "failed Equip Now explains conflict and preserves purchased item");

    Character enhancedBuyer{};
    createCharacter(enhancedBuyer, CLASS_FIGHTER, WEAPON_GROUP_NONE);
    clearInventory(enhancedBuyer.inventory);
    resetEquipment(enhancedBuyer);
    enhancedBuyer.inventory.gold = 500;
    ItemInstance enhanced = makeItemInstance(ITEM_LONGSWORD);
    enhanced.enhancementBonus = 2;
    enhanced.weaponProperties = WEAPON_PROPERTY_FLAMING;
    check(purchaseShopItem(enhancedBuyer, enhanced, 250) ==
              SHOP_PURCHASE_SUCCESS,
          "generic purchase accepts exact enhanced instance");
    check(enhancedBuyer.inventory.gold == 250 &&
              hasItem(enhancedBuyer, enhanced) &&
              equipItemWithResult(enhancedBuyer, enhanced) == EQUIP_SUCCESS &&
              enhancedBuyer.equipment.equipped[SLOT_MELEE_WEAPON] == enhanced,
          "enhanced purchase and equipment retain properties");

    Character fullBuyer{};
    createCharacter(fullBuyer, CLASS_FIGHTER, WEAPON_GROUP_NONE);
    clearInventory(fullBuyer.inventory);
    resetEquipment(fullBuyer);
    fullBuyer.inventory.gold = 500;
    check(addItem(fullBuyer, ITEM_LONGSWORD, MAX_INVENTORY),
          "full inventory fixture");
    const uint32_t goldBefore = fullBuyer.inventory.gold;
    check(purchaseShopItem(fullBuyer, makeItemInstance(ITEM_CHAINMAIL), 100) ==
              SHOP_PURCHASE_INVENTORY_FULL &&
              fullBuyer.inventory.gold == goldBefore &&
              !hasItem(fullBuyer, ITEM_CHAINMAIL),
          "failed purchase preserves gold and inventory");
}

void testPersistenceAndBrowser()
{
    ItemInstance enhanced = makeItemInstance(ITEM_LONGSWORD);
    enhanced.enhancementBonus = 2;
    enhanced.weaponProperties = WEAPON_PROPERTY_FLAMING;
    clearInventory(player.inventory);
    resetEquipment(player);
    check(addItem(player, enhanced) && equipItem(player, enhanced),
          "persistence equipment fixture");
    check(addItem(player, ITEM_CHAINMAIL) && equipItem(player, ITEM_CHAINMAIL),
          "persistence armor fixture");
    check(saveGameToSlot(player, 0), "save equipped character");
    Character loaded{};
    check(loadGameFromSlot(loaded, 0), "load equipped character");
    check(loaded.equipment.equipped[SLOT_MELEE_WEAPON] == enhanced &&
              getEquippedItem(loaded, SLOT_ARMOR) == ITEM_CHAINMAIL &&
              getArmorClass(loaded) == getArmorClass(player),
          "save/load preserves exact equipment and derived stats");

    BrowserPresentation presentation;
    check(browserPresentation(0, 0, presentation),
          "browser presentation available");
    check(presentation.character.meleeWeapon.find("Longsword") !=
              std::string::npos &&
              presentation.character.armor == "Chainmail" &&
              presentation.character.armorClass == getArmorClass(player),
          "browser panel uses authoritative equipment and stats");
}
}

int main()
{
    fs::path saveRoot;
    try
    {
        saveRoot = fs::temp_directory_path() /
            ("pfme-town-equipment-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(saveRoot);
        fs::permissions(saveRoot, fs::perms::owner_all,
                        fs::perm_options::replace);
        setEnvironment("PFME_SAVE_DIR", saveRoot.string());

        initializeControllers(37, true);
        ControllerState first;
        ControllerState second;
        check(bindController(0, first), "bind first controller");
        prepareTownCharacter("Aric");
        testHomeEquipment(0);
        testShop(0);

        check(bindController(1, second), "bind second controller");
        prepareTownCharacter("Mira");
        player.inventory.gold = 777;
        check(selectController(0), "restore first controller");
        testPersistenceAndBrowser();
        check(selectController(1), "select second controller");
        check(std::string(getCharacterName(player)) == "Mira" &&
                  player.inventory.gold == 777 &&
                  getEquippedItem(player, SLOT_MELEE_WEAPON) !=
                      ITEM_LONGSWORD,
              "Town equipment remains isolated per player");

        fs::remove_all(saveRoot);
        std::cout << "PASS Town Equipment, Shop Equip Now, exact instances, "
                     "persistence, browser state, and player isolation\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << " " << diagnostic() << '\n';
        if (!saveRoot.empty()) fs::remove_all(saveRoot);
        return 1;
    }
}
