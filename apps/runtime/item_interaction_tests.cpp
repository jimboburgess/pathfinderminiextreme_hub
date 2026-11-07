#include "runtime.h"
#include "controller_slots.h"
#include "characters/items.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/actioneconomy.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "dungeon/entitypersistence.h"
#include "graphics/display.h"
#include "graphics/itemdropvisual.h"
#include "input/menu.h"
#include "map/activemap.h"
#include "map/worlditems.h"
#include "multiplayer/itemtrade.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

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

void tap(const char* action)
{
    check(controllerInput(0, action, true), "input down");
    wait(30);
    check(controllerInput(0, action, false), "input up");
    wait(310);
}

void enterTestDungeon()
{
    tap("a"); tap("a"); tap("a"); tap("a"); tap("a"); tap("encoder");
    check(gameState == GAME_TOWN, "town");
    tap("cw"); tap("cw"); tap("a");
    if (menuState.isOpen) tap("a");
    check(gameState == GAME_DUNGEON, "dungeon");
}

Entity* prepareActor()
{
    Entity* actor = getActiveMapPlayer();
    check(actor != nullptr, "actor");
    actor->x = 6; actor->y = 6;
    actor->character.team = TEAM_PLAYER;
    actor->character.state = STATE_ALIVE;
    actor->character.level = 3;
    actor->character.abilities.dexterity = 14;
    clearInventory(actor->character.inventory);
    for (int y = 0; y < ROOM_HEIGHT; ++y)
        for (int x = 0; x < ROOM_WIDTH; ++x)
            dungeon.rooms[dungeon.currentRoom].map.tiles[y][x] = TILE_FLOOR;
    for (uint8_t i = 0; i < dungeon.entityCount; ++i)
        if (&dungeon.entities[i] != actor)
            dungeon.entities[i] = Entity{};
    combat = Combat{};
    return actor;
}

Entity* addAlly(int x, int y)
{
    Entity* ally = spawnEntity(
        dungeon.entities, dungeon.entityCount, ENTITY_PLAYER,
        static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    check(ally != nullptr, "ally");
    ally->ownerPlayerID = 1;
    ally->character.team = TEAM_PLAYER;
    ally->character.state = STATE_ALIVE;
    ally->character.health.maxHP = 30;
    ally->character.health.currentHP = 10;
    setCharacterName(ally->character, "Mira");
    ally->sprite = getPlayerSprite(CLASS_CLERIC);
    return ally;
}

void prepareCombat(Entity& actor, Entity& ally)
{
    combat = Combat{};
    combat.active = true;
    combat.phase = COMBAT_TURN;
    combat.waitingForPlayer = true;
    combat.combatantCount = 2;
    combat.currentTurnIndex = 0;
    combat.initiativeOrder[0] = &actor;
    combat.initiativeOrder[1] = &ally;
    actor.turn = TurnState{};
    actor.turn.turnActive = true;
    actor.turn.movementRemaining = 6;
}

void testTradeBook()
{
    Character sender{};
    Character recipient{};
    addItem(sender, ITEM_POTION_CURE_LIGHT_WOUNDS, 2);
    TradeOfferBook book;
    TradeOfferLine line{makeItemInstance(ITEM_POTION_CURE_LIGHT_WOUNDS), 1};
    uint32_t offerID = 0;
    check(book.propose(0, 1, &line, 1, sender, offerID) == TradeResult::SUCCESS,
          "trade proposed");
    check(getItemQuantity(sender, ITEM_POTION_CURE_LIGHT_WOUNDS) == 2 &&
          getItemQuantity(recipient, ITEM_POTION_CURE_LIGHT_WOUNDS) == 0,
          "proposal moves nothing");
    check(book.pendingFor(0) != nullptr && book.pendingFor(1) != nullptr,
          "one pending offer binds both players");
    uint32_t blockedID = 0;
    check(book.propose(0, 2, &line, 1, sender, blockedID) ==
              TradeResult::PLAYER_BUSY,
          "one active offer per player");
    check(book.accept(offerID, 1, sender, recipient) == TradeResult::SUCCESS,
          "trade accepted");
    check(getItemQuantity(sender, ITEM_POTION_CURE_LIGHT_WOUNDS) == 1 &&
          getItemQuantity(recipient, ITEM_POTION_CURE_LIGHT_WOUNDS) == 1,
          "accepted transfer happens exactly once");
    check(book.accept(offerID, 1, sender, recipient) == TradeResult::NOT_FOUND,
          "accepted offer cannot replay");

    check(book.propose(0, 1, &line, 1, sender, offerID) == TradeResult::SUCCESS,
          "second trade proposed");
    check(book.decline(offerID, 1) == TradeResult::SUCCESS,
          "trade declined");
    check(getItemQuantity(sender, ITEM_POTION_CURE_LIGHT_WOUNDS) == 1,
          "decline preserves sender item");

    clearInventory(recipient.inventory);
    check(addItem(recipient, ITEM_LONGSWORD, MAX_INVENTORY),
          "full inventory fixture");
    check(book.propose(0, 1, &line, 1, sender, offerID) == TradeResult::SUCCESS,
          "full inventory offer pending");
    check(book.accept(offerID, 1, sender, recipient) ==
              TradeResult::INVENTORY_FULL,
          "full inventory rejects acceptance");
    check(getItemQuantity(sender, ITEM_POTION_CURE_LIGHT_WOUNDS) == 1,
          "failed acceptance preserves sender item");

    clearInventory(recipient.inventory);
    check(book.propose(0, 1, &line, 1, sender, offerID) == TradeResult::SUCCESS,
          "disconnect fixture offer");
    book.cancelForPlayer(1);
    check(book.pendingFor(0) == nullptr &&
          getItemQuantity(sender, ITEM_POTION_CURE_LIGHT_WOUNDS) == 1,
          "disconnect cancellation moves nothing");
}
}

int main()
{
    try
    {
        check(calculateCureLightWoundsPotionHealing(1) == 0,
              "healing roll one is zero");
        check(calculateCureLightWoundsPotionHealing(8) == 7,
              "healing roll eight is seven");
        check(!squareTargetRangedAttackHits(1, 20),
              "natural one misses square");
        check(squareTargetRangedAttackHits(2, 3),
              "square attack uses AC five rather than ally AC");
        testTradeBook();

        initializeControllers(11);
        ControllerState state;
        check(bindController(0, state), "bind");
        enterTestDungeon();
        Entity* actor = prepareActor();
        Entity* ally = addAlly(9, 6);

        addItem(actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS, 2);
        const uint16_t beforeSuccess = getItemQuantity(
            actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS);
        ThrownPotionResolution success = resolveThrownHealingPotion(
            *actor, ally->x, ally->y, 20, 8);
        check(success.result == WorldItemResult::SUCCESS && success.hit,
              "successful potion throw");
        check(success.healing == 7 && ally->character.health.currentHP == 17,
              "successful throw heals one d8 minus one");
        check(getItemQuantity(actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
                  beforeSuccess - 1,
              "successful throw consumes once");
        check(!combat.active, "exploration throw starts no combat");

        const int hpBeforeMiss = ally->character.health.currentHP;
        ThrownPotionResolution miss = resolveThrownHealingPotion(
            *actor, ally->x, ally->y, 1, 8);
        check(miss.result == WorldItemResult::SUCCESS && !miss.hit &&
              miss.consumed, "miss consumes potion");
        check(ally->character.health.currentHP == hpBeforeMiss,
              "miss heals nothing");

        addItem(actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS, 1);
        const uint16_t beforeInvalid = getItemQuantity(
            actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS);
        check(resolveThrownHealingPotion(*actor, 8, 7, 20, 8).result ==
                  WorldItemResult::INVALID_TARGET,
              "empty square rejected");
        Entity* enemy = spawnEntity(
            dungeon.entities, dungeon.entityCount, ENTITY_MONSTER, 8, 8);
        check(enemy != nullptr, "enemy target fixture");
        enemy->character.team = TEAM_MONSTER;
        enemy->character.state = STATE_ALIVE;
        check(resolveThrownHealingPotion(*actor, 8, 8, 20, 8).result ==
                  WorldItemResult::INVALID_TARGET,
              "enemy square rejected");
        ally->active = false;
        check(resolveThrownHealingPotion(*actor, 9, 6, 20, 8).result ==
                  WorldItemResult::INVALID_TARGET,
              "ally outside active room rejected");
        ally->active = true;
        check(resolveThrownHealingPotion(*actor, 14, 6, 20, 8).result ==
                  WorldItemResult::OUT_OF_RANGE,
              "out of range rejected");
        check(getItemQuantity(actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
                  beforeInvalid, "invalid throws consume nothing");

        beginThrownHealingPotion();
        check(isPlayerTargetingGroundAbility(), "throw uses ground cursor");
        int targetX = 0;
        int targetY = 0;
        check(getSelectedAbilityGroundTarget(targetX, targetY) &&
              targetX == ally->x && targetY == ally->y,
              "throw selects allied square");
        tft.fillScreen(ST77XX_BLACK);
        drawMapCursor();
        check(pixels()[(targetY * TILE_SIZE + 1) * 240 +
                       targetX * TILE_SIZE + 1] == 0xC81F,
              "throw target is visible in framebuffer");
        cancelPlayerAbility();
        check(getItemQuantity(actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS) ==
                  beforeInvalid, "cancel consumes nothing");

        prepareCombat(*actor, *ally);
        actor->turn.moveActionUsed = true;
        addItem(actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS, 1);
        ThrownPotionResolution movedThrow = resolveThrownHealingPotion(
            *actor, ally->x, ally->y, 20, 8);
        check(movedThrow.result == WorldItemResult::SUCCESS &&
              actor->turn.standardActionUsed,
              "move then throw spends standard action");
        check(!canUseDoubleMove(actor->turn),
              "throw then double move is illegal");

        prepareCombat(*actor, *ally);
        addItem(actor->character, ITEM_POTION_CURE_LIGHT_WOUNDS, 1);
        check(resolveThrownHealingPotion(
                  *actor, ally->x, ally->y, 20, 8).result ==
                  WorldItemResult::SUCCESS,
              "throw before moving is legal");
        check(canUseMoveAction(actor->turn), "move remains after throw");

        combat = Combat{};
        clearInventory(actor->character.inventory);
        addItem(actor->character, ITEM_MANA_POTION, 2);
        Entity* drop = nullptr;
        check(dropWorldItem(
                  *actor, makeItemInstance(ITEM_MANA_POTION), &drop) ==
                  WorldItemResult::SUCCESS,
              "drop creates world item");
        check(drop != nullptr && isDroppedWorldItem(*drop) &&
              getItemQuantity(actor->character, ITEM_MANA_POTION) == 1,
              "drop removes one stack unit");

        PersistentEntity packed{};
        check(packPersistentEntity(*drop, packed), "drop packs");
        Entity inflated{};
        check(inflatePersistentEntity(packed, inflated) &&
              getDroppedWorldItem(inflated)->item.itemID == ITEM_MANA_POTION,
              "drop persists across room inflation");

        actor->x = drop->x;
        actor->y = static_cast<uint8_t>(drop->y + 1);
        check(pickupWorldItem(*actor, *drop) == WorldItemResult::SUCCESS,
              "another player can pick up drop");
        check(pickupWorldItem(*actor, *drop) == WorldItemResult::UNAVAILABLE,
              "pickup race has one winner");
        check(getItemQuantity(actor->character, ITEM_MANA_POTION) == 2,
              "pickup restores exact instance once");

        clearInventory(actor->character.inventory);
        check(addItem(actor->character, ITEM_LONGSWORD, MAX_INVENTORY),
              "pickup full inventory fixture");
        Entity fullDrop{};
        fullDrop.active = true; fullDrop.type = ENTITY_LOOT;
        fullDrop.x = static_cast<uint8_t>(actor->x + 1); fullDrop.y = actor->y;
        fullDrop.loot.generated = true; fullDrop.loot.itemCount = 1;
        fullDrop.loot.slots[0] = {makeItemInstance(ITEM_MANA_POTION), 1};
        check(pickupWorldItem(*actor, fullDrop) ==
                  WorldItemResult::INVENTORY_FULL &&
              isDroppedWorldItem(fullDrop),
              "full inventory leaves world drop intact");

        clearInventory(actor->character.inventory);
        addItem(actor->character, ITEM_MANA_POTION, 2);
        prepareCombat(*actor, *ally);
        Entity* combatDrop = nullptr;
        check(dropWorldItem(
                  *actor, makeItemInstance(ITEM_MANA_POTION), &combatDrop) ==
                  WorldItemResult::SUCCESS && actor->turn.moveActionUsed,
              "combat drop spends move action");
        check(dropWorldItem(
                  *actor, makeItemInstance(ITEM_MANA_POTION), nullptr) ==
                  WorldItemResult::ACTION_SPENT,
              "combat drop cannot repeat after move action");
        combat = Combat{};

        Entity healingDrop{};
        healingDrop.active = true; healingDrop.type = ENTITY_LOOT;
        healingDrop.x = 2; healingDrop.y = 2;
        healingDrop.loot.generated = true; healingDrop.loot.itemCount = 1;
        healingDrop.loot.slots[0] = {
            makeItemInstance(ITEM_POTION_CURE_LIGHT_WOUNDS), 1};
        Entity manaDrop = healingDrop;
        manaDrop.x = 3;
        manaDrop.loot.slots[0].item = makeItemInstance(ITEM_MANA_POTION);
        tft.fillScreen(ST77XX_BLACK);
        drawEntity(healingDrop);
        drawEntity(manaDrop);
        const uint16_t* frame = pixels();
        check(frame[(2 * TILE_SIZE + 7) * 240 + 2 * TILE_SIZE + 6] ==
                  HEALING_POTION_DROP_COLOR,
              "healing potion sprite is light blue");
        check(frame[(2 * TILE_SIZE + 7) * 240 + 3 * TILE_SIZE + 6] ==
                  MANA_POTION_DROP_COLOR,
              "mana potion sprite is deep blue");
        check(HEALING_POTION_DROP_COLOR != MANA_POTION_DROP_COLOR,
              "potion colors are distinct");
        manaDrop.x = 4;
        manaDrop.loot.slots[0].item = makeItemInstance(ITEM_LONGSWORD);
        drawEntity(manaDrop);
        bool nonBlack = false;
        for (int y = 4; y < 14; ++y)
            for (int x = 3; x < 14; ++x)
                nonBlack = nonBlack || frame[(2 * TILE_SIZE + y) * 240 +
                                             4 * TILE_SIZE + x] != ST77XX_BLACK;
        check(nonBlack, "non-potion category fallback renders");

        std::cout << "PASS potion formula, square throws, trade state, "
                     "world drops, persistence, races, and sprites\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << "; "
                  << diagnostic() << '\n';
        return 1;
    }
}
