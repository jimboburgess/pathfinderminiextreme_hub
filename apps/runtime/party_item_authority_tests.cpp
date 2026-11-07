#include "runtime.h"
#include "controller_slots.h"
#include "native_party.h"
#include "native_rooms.h"
#include "characters/sheet.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "graphics/charcreationscreen.h"
#include "input/menu.h"
#include "town/town.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
using namespace pfme::runtime;

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

void tap(PlayerID id, const char* action)
{
    check(controllerInput(id, action, true), "input down");
    wait(30);
    check(controllerInput(id, action, false), "input up");
    wait(310);
}

void choose(PlayerID id, const char* title)
{
    check(selectController(id), "select controller");
    check(menuState.isOpen && menuState.depth != 0, "menu open");
    const Menu* menu = menuState.menuStack[menuState.depth - 1];
    int wanted = -1;
    int visible = 0;
    for (uint8_t i = 0; i < menu->itemCount; ++i)
    {
        if (!isMenuItemVisible(menu->items[i].action)) continue;
        if (std::string(menu->items[i].title) == title) wanted = visible;
        ++visible;
    }
    check(wanted >= 0, "menu choice present");
    for (unsigned i = 0; menuState.cursorIndex != wanted && i < 40; ++i)
        tap(id, menuState.cursorIndex < wanted ? "cw" : "ccw");
    check(menuState.cursorIndex == wanted, "menu choice selected");
    tap(id, "a");
}

void createCharacter(PlayerID id, const char* name)
{
    nativePartyCharacterName(id, name);
    tap(id, "a");
    tap(id, "a");
    if (getCharacterCreationState() == CCS_WEAPON_STYLE_SELECT) tap(id, "a");
    tap(id, "a"); tap(id, "a"); tap(id, "encoder");
    check(gameState == GAME_TOWN, "character reached town");
}

void openBoard(PlayerID id)
{
    selectController(id);
    if (menuState.isOpen) tap(id, "b");
    for (unsigned i = 0; townSelection != TOWN_BULLETIN_BOARD && i < 6; ++i)
        tap(id, "cw");
    tap(id, "a");
}

void joinParty(PlayerID id, const char* code)
{
    openBoard(id);
    choose(id, "Join Party");
    for (uint8_t i = 0; i < pfme::PARTY_JOIN_CODE_LENGTH; ++i)
    {
        char letter[2] = {code[i], '\0'};
        choose(id, letter);
    }
    tap(id, "b"); tap(id, "b");
}

TradeOfferLine healingPotionLine()
{
    TradeOfferLine line;
    line.item = makeItemInstance(ITEM_POTION_CURE_LIGHT_WOUNDS);
    line.quantity = 1;
    return line;
}
}

int main()
{
    try
    {
        initializeControllers(17, true);
        ControllerState states[2];
        check(bindController(0, states[0]), "bind Aric");
        createCharacter(0, "Aric");
        check(bindController(1, states[1]), "bind Mira");
        createCharacter(1, "Mira");

        openBoard(0);
        choose(0, "Create Party");
        pfme::PartySnapshot party;
        check(nativePartySnapshot(0, party), "party created");
        tap(0, "b"); tap(0, "b");
        joinParty(1, party.joinCode);
        check(nativePartySnapshot(0, party) && party.memberCount == 2,
              "party joined");

        selectController(0);
        addItem(player, ITEM_POTION_CURE_LIGHT_WOUNDS, 3);
        check(canOpenNativeTradeMenu() && isMenuItemVisible(MENU_TRADE_ITEMS),
              "Trade Items is visible for party member");
        openNativeTradeMenu();
        check(menuState.isOpen &&
              std::string(menuState.menuStack[0]->title) == "Trade Items",
              "trade player selection uses framebuffer menu");
        closeMenu();

        TradeOfferLine line = healingPotionLine();
        uint32_t offerID = 0;
        const uint16_t senderBefore = getItemQuantity(
            player, ITEM_POTION_CURE_LIGHT_WOUNDS);
        check(proposeNativeTrade(1, &line, 1, offerID) == TradeResult::SUCCESS,
              "authoritative offer accepted");
        check(getItemQuantity(player, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
                  senderBefore,
              "offer does not transfer immediately");
        selectController(1);
        check(pendingNativeTrade(1) != nullptr && canOpenNativeTradeMenu(),
              "recipient can review pending offer");
        check(respondToNativeTrade(true) == TradeResult::SUCCESS,
              "recipient accepts");
        check(getItemQuantity(player, ITEM_POTION_CURE_LIGHT_WOUNDS) == 1,
              "recipient receives one unit");
        selectController(0);
        check(getItemQuantity(player, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
                  senderBefore - 1,
              "sender loses one unit exactly once");

        check(proposeNativeTrade(1, &line, 1, offerID) == TradeResult::SUCCESS,
              "decline offer proposed");
        selectController(1);
        check(respondToNativeTrade(false) == TradeResult::SUCCESS,
              "recipient declines");
        selectController(0);
        check(getItemQuantity(player, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
                  senderBefore - 1,
              "decline preserves ownership");

        selectController(1);
        clearInventory(player.inventory);
        check(addItem(player, ITEM_LONGSWORD, MAX_INVENTORY),
              "recipient full fixture");
        selectController(0);
        check(proposeNativeTrade(1, &line, 1, offerID) == TradeResult::SUCCESS,
              "full recipient offer proposed");
        selectController(1);
        check(respondToNativeTrade(true) == TradeResult::INVENTORY_FULL,
              "full recipient rejected");
        selectController(0);
        check(getItemQuantity(player, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
                  senderBefore - 1,
              "full recipient cannot lose sender item");

        check(proposeNativeTrade(1, &line, 1, offerID) == TradeResult::SUCCESS,
              "disconnect offer proposed");
        setControllerConnected(1, false);
        check(pendingNativeTrade(0) == nullptr,
              "disconnect cancels pending offer");
        selectController(0);
        check(getItemQuantity(player, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
                  senderBefore - 1,
              "disconnect cancellation preserves item");
        setControllerConnected(1, true);

        selectController(1);
        clearInventory(player.inventory);

        selectController(0);
        for (unsigned i = 0; townSelection != TOWN_DUNGEON && i < 6; ++i)
            tap(0, "cw");
        tap(0, "a");
        check(gameState == GAME_DUNGEON && nativeActor(0) && nativeActor(1),
              "party entered dungeon");
        selectController(1);
        dungeon.currentRoom = 1;
        check(travelNativeRoom(dungeon, ENTRY_WEST),
              "recipient moved to another room");
        check(nativeActorRoom(0) != nativeActorRoom(1),
              "different-room trade fixture");

        selectController(0);
        check(proposeNativeTrade(1, &line, 1, offerID) == TradeResult::SUCCESS,
              "trade ignores dungeon room distance");
        selectController(1);
        check(respondToNativeTrade(true) == TradeResult::SUCCESS,
              "different-room recipient accepts");

        Entity* actor0 = nativeActor(0);
        Entity* actor1 = nativeActor(1);
        combat = Combat{};
        combat.active = true;
        combat.phase = COMBAT_TURN;
        combat.waitingForPlayer = true;
        combat.combatantCount = 2;
        combat.initiativeOrder[0] = actor1;
        combat.initiativeOrder[1] = actor0;
        combat.currentTurnIndex = 0;
        actor0->turn = TurnState{};
        actor1->turn = TurnState{};
        selectController(0);
        check(proposeNativeTrade(1, &line, 1, offerID) ==
                  TradeResult::PLAYER_BUSY,
              "non-acting player cannot initiate combat trade");
        combat.currentTurnIndex = 1;
        check(proposeNativeTrade(1, &line, 1, offerID) == TradeResult::SUCCESS,
              "acting player can initiate combat trade");
        check(actor0->turn.standardActionUsed,
              "combat trade commitment spends standard action");
        selectController(1);
        check(canOpenNativeTradeMenu(),
              "recipient may administratively review out of turn");
        check(respondToNativeTrade(false) == TradeResult::SUCCESS,
              "recipient response costs no combat action");
        check(!actor1->turn.standardActionUsed,
              "recipient response leaves actions untouched");

        std::cout << "PASS authoritative party trade menus, acceptance, "
                     "distance independence, disconnects, and combat timing\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << "; "
                  << diagnostic() << '\n';
        return 1;
    }
}
