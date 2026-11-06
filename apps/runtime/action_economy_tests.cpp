#include "runtime.h"
#include "controller_slots.h"
#include "characters/conditions.h"
#include "characters/sheet.h"
#include "data/game.h"
#include "data/entityspawn.h"
#include "dungeon/actioneconomy.h"
#include "dungeon/combat.h"
#include "dungeon/combatpolicy.h"
#include "dungeon/dungeon.h"
#include "dungeon/loot.h"
#include "dungeon/monsterpresentation.h"
#include "dungeon/monsterscripts.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "input/inventorymenu.h"
#include "input/menu.h"
#include "map/activemap.h"
#include "map/interaction.h"
#include "map/mapeffects.h"
#include "map/movement.h"
#include "map/skillactions.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
using namespace pfme::runtime;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
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
    tap("a");
    tap("a");
    tap("a");
    tap("a");
    tap("a");
    tap("encoder");
    check(gameState == GAME_TOWN, "town");
    tap("cw");
    tap("cw");
    tap("a");
    if (menuState.isOpen) tap("a");
    check(gameState == GAME_DUNGEON && dungeon.runActive, "dungeon");
}

Direction openDirection(const Entity& actor)
{
    for (Direction direction : {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST})
    {
        const int x = actor.x + directionOffsets[direction].dx;
        const int y = actor.y + directionOffsets[direction].dy;
        if (x >= 0 && y >= 0 && x < ROOM_WIDTH && y < ROOM_HEIGHT &&
            dungeon.rooms[dungeon.currentRoom].map.tiles[y][x] == TILE_FLOOR &&
            getEntityAt(dungeon.entities, dungeon.entityCount, x, y) == nullptr)
            return direction;
    }
    throw std::runtime_error("open movement tile");
}

void placeAdjacent(Entity& entity, const Entity& actor, Direction direction)
{
    entity.x = static_cast<uint8_t>(
        actor.x + directionOffsets[direction].dx);
    entity.y = static_cast<uint8_t>(
        actor.y + directionOffsets[direction].dy);
}

bool hasOutline(const Entity& entity, uint16_t color)
{
    const uint16_t* frame = pixels();
    const int left = entity.x * TILE_SIZE;
    const int top = entity.y * TILE_SIZE;
    const int right = left + getEntityTileWidth(entity) * TILE_SIZE - 1;
    const int bottom = top + getEntityTileHeight(entity) * TILE_SIZE - 1;
    for (int x = left; x <= right; ++x)
        if (frame[top * 240 + x] != color ||
            frame[bottom * 240 + x] != color) return false;
    for (int y = top; y <= bottom; ++y)
        if (frame[y * 240 + left] != color ||
            frame[y * 240 + right] != color) return false;
    return true;
}

void checkAttackMath()
{
    Character character{};
    character.characterClass = CLASS_WIZARD;
    character.level = 2;
    character.abilities.strength = 14;
    character.abilities.dexterity = 14;
    character.equipment.equipped[SLOT_MELEE_WEAPON] =
        makeItemInstance(ITEM_LONGSWORD);
    character.equipment.equipped[SLOT_RANGED_WEAPON] =
        makeItemInstance(ITEM_SHORTBOW);
    check(getMeleeAttackBonus(character) == 3,
          "melee uses BAB plus Strength");
    check(getRangedAttackBonus(character) == 3,
          "ranged uses BAB plus Dexterity");

    character.abilities.strength = 18;
    check(getMeleeAttackBonus(character) == 5,
          "Strength 18 contributes plus four");
    character.equipment.equipped[SLOT_MELEE_WEAPON].enhancementBonus = 1;
    character.equipment.equipped[SLOT_RANGED_WEAPON].enhancementBonus = 1;
    check(getMeleeAttackBonus(character) == 6,
          "magic melee weapon adds one attack");
    check(getRangedAttackBonus(character) == 4,
          "magic ranged weapon adds one attack");

    character = Character{};
    character.characterClass = CLASS_FIGHTER;
    character.level = 2;
    character.abilities.strength = 10;
    character.abilities.dexterity = 10;
    setFighterTrainedWeaponGroup(character, WEAPON_GROUP_BLADES);
    character.equipment.equipped[SLOT_MELEE_WEAPON] =
        makeItemInstance(ITEM_LONGSWORD);
    check(getMeleeAttackBonus(character) == 3,
          "matching Weapon Focus applies only to trained group");
    character.equipment.equipped[SLOT_MELEE_WEAPON] =
        makeItemInstance(ITEM_BATTLEAXE);
    check(getMeleeAttackBonus(character) == 2,
          "nonmatching weapon group receives no focus bonus");
    check(getPowerAttackPenalty(character) == -1,
          "Power Attack retains PF1e BAB scaling");
}
}

int main()
{
    try
    {
        initializeControllers(2);
        ControllerState state;
        check(bindController(0, state), "bind controller");
        enterTestDungeon();

        Entity* actor = getActiveMapPlayer();
        check(actor != nullptr, "active actor");
        combat = Combat{};
        combat.active = true;
        combat.phase = COMBAT_TURN;
        combat.waitingForPlayer = true;
        combat.combatantCount = 1;
        combat.currentTurnIndex = 0;
        combat.initiativeOrder[0] = actor;
        actor->turn = TurnState{};
        actor->turn.turnActive = true;
        actor->turn.movementRemaining = getEffectiveSpeed(actor->character);

        check(!isMenuItemVisible(MENU_DOUBLE_MOVE),
              "Double Move hidden before movement");
        check(isMenuItemVisible(MENU_ATTACK), "attack visible on fresh turn");

        const Direction direction = openDirection(*actor);
        while (moveDirection != direction) tap("cw");
        const int oldX = actor->x;
        const int oldY = actor->y;
        tap("encoder");
        check(actor->x != oldX || actor->y != oldY, "production movement");
        check(actor->turn.moveActionUsed, "first step commits move action");
        check(isMenuItemVisible(MENU_DOUBLE_MOVE),
              "Double Move visible after movement");
        check(isMenuItemVisible(MENU_ATTACK),
              "attack remains visible after movement");
        check(!canMakeFullAttack(*actor),
              "partial movement prevents a full attack");

        const uint8_t effectiveSpeed = getEffectiveSpeed(actor->character);
        check(effectiveSpeed == 6, "test character speed is six");
        actor->turn.movementRemaining = 4;
        beginDoubleMove();
        check(actor->turn.standardActionUsed,
              "Double Move consumes standard action");
        check(actor->turn.movementRemaining == 10,
              "moving two of six then Double Move leaves ten");
        check(!isMenuItemVisible(MENU_DOUBLE_MOVE),
              "Double Move hidden after use");
        check(!isMenuItemVisible(MENU_ATTACK),
              "attack hidden after Double Move");
        check(!canMakeFullAttack(*actor),
              "Double Move cannot be followed by a full attack");
        check(getTerrainMovementCost(TILE_RUBBLE) == 2,
              "rubble retains difficult-terrain cost");
        spendMovementCost(*actor, getTerrainMovementCost(TILE_RUBBLE));
        check(actor->turn.movementRemaining == 8,
              "second movement allowance pays normal terrain cost");

        ConditionModifiers hasteModifiers{};
        hasteModifiers.speedBonus = 6;
        hasteModifiers.bonusAttacks = 1;
        check(addCondition(actor->character, CONDITION_HASTED,
                           hasteModifiers, 5),
              "Haste fixture applies");
        TurnState hastedTurn{};
        hastedTurn.moveActionUsed = true;
        hastedTurn.movementRemaining = 2;
        check(tryBeginDoubleMove(
                  hastedTurn,
                  static_cast<uint8_t>(getEffectiveSpeed(actor->character))) &&
              hastedTurn.movementRemaining == 14,
              "Double Move adds Haste-adjusted effective speed");
        check(removeCondition(actor->character, CONDITION_HASTED),
              "Haste fixture clears");

        const uint8_t remaining = actor->turn.movementRemaining;
        beginDoubleMove();
        check(actor->turn.movementRemaining == remaining,
              "stale Double Move is rejected without mutation");

        actor->turn = TurnState{};
        actor->turn.turnActive = true;
        actor->turn.standardActionUsed = true;
        actor->turn.movementRemaining = effectiveSpeed;
        check(!isMenuItemVisible(MENU_DOUBLE_MOVE),
              "Double Move hidden after attack");
        check(!isMenuItemVisible(MENU_ATTACK),
              "second standard attack hidden");

        checkAttackMath();

        char mpText[20];
        actor->character.magic.currentMP = 10;
        actor->character.magic.maxMP = 15;
        formatCharacterMP(actor->character, mpText, sizeof(mpText));
        check(std::string(mpText) == "10/15", "sheet formats current/max MP");
        actor->character.magic.currentMP = 0;
        formatCharacterMP(actor->character, mpText, sizeof(mpText));
        check(std::string(mpText) == "0/15", "sheet formats empty MP");
        actor->character.magic.currentMP = 15;
        formatCharacterMP(actor->character, mpText, sizeof(mpText));
        check(std::string(mpText) == "15/15", "sheet formats full MP");

        setCharacterName(actor->character, "Aric");
        char turnMessage[64];
        formatCombatTurnMessage(*actor, turnMessage, sizeof(turnMessage));
        check(std::string(turnMessage) == "Aric's turn.",
              "player turn message uses character name");
        Entity mira{};
        mira.type = ENTITY_PLAYER;
        setCharacterName(mira.character, "Mira");
        formatCombatTurnMessage(mira, turnMessage, sizeof(turnMessage));
        check(std::string(turnMessage) == "Mira's turn.",
              "other player turn message uses their name");

        ConditionTurnResult poisonResult{};
        poisonResult.damage = 3;
        poisonResult.poisonDamage = 3;
        char poisonMessage[64];
        formatConditionDamageMessage(
            *actor, poisonResult, false,
            poisonMessage, sizeof(poisonMessage));
        check(std::string(poisonMessage) ==
              "Aric takes 3 poison damage.",
              "poison damage message uses the character name");

        combat.active = false;
        removeCondition(actor->character, CONDITION_POISONED);
        tft.fillScreen(ST77XX_BLACK);
        drawEntity(*actor);
        const uint16_t* normalFrame = pixels();
        int poisonSampleX = -1;
        int poisonSampleY = -1;
        uint16_t normalColor = ST77XX_BLACK;
        const int actorLeft = actor->x * TILE_SIZE;
        const int actorTop = actor->y * TILE_SIZE;
        for (int y = actorTop + 1;
             y < actorTop + actor->spriteHeight - 1 && poisonSampleX < 0; y++)
        {
            for (int x = actorLeft + 1;
                 x < actorLeft + actor->spriteWidth - 1; x++)
            {
                const uint16_t color = normalFrame[y * 240 + x];
                if (color != ST77XX_BLACK)
                {
                    poisonSampleX = x;
                    poisonSampleY = y;
                    normalColor = color;
                    break;
                }
            }
        }
        check(poisonSampleX >= 0, "poison render sample exists");
        check(addCondition(actor->character, CONDITION_POISONED, 0, 3),
              "poison render condition applies");
        tft.fillScreen(ST77XX_BLACK);
        drawEntity(*actor);
        check(pixels()[poisonSampleY * 240 + poisonSampleX] ==
                  getPoisonTintColor(normalColor),
              "poisoned entity uses green render-time tint");
        combat.active = true;
        tft.fillScreen(ST77XX_BLACK);
        drawEntity(*actor);
        check(hasOutline(*actor, ST77XX_CYAN),
              "poison tint coexists with active-turn outline");
        check(removeCondition(actor->character, CONDITION_POISONED),
              "poison render condition clears");
        combat.active = false;
        tft.fillScreen(ST77XX_BLACK);
        drawEntity(*actor);
        check(pixels()[poisonSampleY * 240 + poisonSampleX] == normalColor,
              "poison tint disappears immediately after cure");

        TurnState spiderA{};
        TurnState spiderB{};
        commitMonsterWebCooldown(spiderA);
        check(!canMonsterUseWeb(spiderA) && canMonsterUseWeb(spiderB),
              "Web cooldown is independent per spider");
        advanceMonsterWebCooldown(spiderA);
        check(getMonsterWebCooldownTurns(spiderA) == 3,
              "Web cast turn does not consume first cooldown turn");
        for (int turn = 0; turn < 3; turn++)
        {
            check(!canMonsterUseWeb(spiderA),
                  "Web unavailable during each cooldown turn");
            advanceMonsterWebCooldown(spiderA);
        }
        check(canMonsterUseWeb(spiderA),
              "Web is available after three skipped turns");

        check(getMonsterThinkingChance(3) == 35 &&
              getMonsterThinkingChance(7) == 25 &&
              getMonsterThinkingChance(11) == 15 &&
              getMonsterThinkingChance(12) == 8,
              "thinking chance follows Intelligence thresholds");
        check(getMonsterThinkingChance(3) > getMonsterThinkingChance(12),
              "low Intelligence thinks more often");
        const uint32_t cosmeticValue = getMonsterPresentationValue(
            MONSTER_GIANT_SPIDER, 4, 5, 2, 1);
        check(cosmeticValue == getMonsterPresentationValue(
                  MONSTER_GIANT_SPIDER, 4, 5, 2, 1),
              "cosmetic thinking decision is deterministic");
        const unsigned long thinkingDuration =
            getMonsterThinkingDuration(
                static_cast<uint16_t>(cosmeticValue >> 8));
        check(thinkingDuration >= MONSTER_THINKING_MIN_MS &&
              thinkingDuration <= MONSTER_THINKING_MAX_MS,
              "thinking duration stays within presentation bounds");
        check(MONSTER_STEP_PRESENTATION_MS >= 150 &&
              MONSTER_STEP_PRESENTATION_MS <= 250,
              "monster tile pacing is near 200 milliseconds");

        clearMapEffects();
        MapEffect web{};
        web.active = true;
        web.type = MAP_EFFECT_WEB;
        web.x = actor->x;
        web.y = actor->y;
        web.saveDC = 16;
        web.expiresWithCombat = true;
        check(addMapEffect(web) != nullptr, "Web escape fixture applies");
        actor->turn = TurnState{};
        actor->turn.turnActive = true;
        actor->turn.movementRemaining = 0;
        check(addCondition(actor->character, CONDITION_GRAPPLED, 0, 0),
              "Web restraint applies");
        check(!attemptEscapeWeb(*actor, 15) &&
              actor->turn.movementRemaining == 0,
              "failed Web escape preserves restraint");
        check(attemptEscapeWeb(*actor, 16) &&
              actor->turn.movementRemaining == effectiveSpeed,
              "successful Web escape immediately restores unused movement");
        clearMapEffects();

        actor->character.equipment.equipped[SLOT_MELEE_WEAPON] =
            makeItemInstance(ITEM_LONGSWORD);
        actor->turn = TurnState{};
        actor->turn.turnActive = true;
        actor->turn.movementRemaining = 0;
        check(addCondition(actor->character, CONDITION_GRAPPLED, 0, 0),
              "cut-free restraint applies");
        combat = Combat{};
        combat.active = true;
        combat.phase = COMBAT_TURN;
        combat.waitingForPlayer = true;
        combat.combatantCount = 1;
        combat.initiativeOrder[0] = actor;
        check(cutFreeFromWeb(), "cut-free action succeeds");
        check(!hasCondition(actor->character, CONDITION_GRAPPLED) &&
              actor->turn.movementRemaining == effectiveSpeed &&
              actor->turn.standardActionUsed &&
              !actor->turn.moveActionUsed,
              "cut-free spends standard action and restores move action");

        actor->turn = TurnState{};
        actor->turn.turnActive = true;
        actor->turn.movementRemaining = effectiveSpeed;
        combat = Combat{};
        combat.active = true;
        combat.phase = COMBAT_TURN;
        combat.waitingForPlayer = true;
        combat.combatantCount = 1;
        combat.initiativeOrder[0] = actor;

        Direction corpseDirection = openDirection(*actor);
        Entity* corpse = spawnMonster(
            dungeon.entities, dungeon.entityCount,
            MONSTER_GOBLIN_SCIMITAR, 0, 0);
        check(corpse != nullptr, "corpse fixture");
        placeAdjacent(*corpse, *actor, corpseDirection);
        corpse->character.state = STATE_DEAD;
        corpse->character.health.currentHP = 0;
        corpse->loot.generated = true;
        corpse->loot.itemCount = 1;
        corpse->loot.slots[0].item = makeItemInstance(ITEM_RATIONS);
        corpse->loot.slots[0].quantity = 1;
        moveDirection = corpseDirection;

        check(tryInteractWithFacingEntity(),
              "corpse interaction allowed during player combat turn");
        check(isInventoryMenuOpen(), "existing corpse loot UI opens");
        check(actor->turn.standardActionUsed &&
              !actor->turn.moveActionUsed,
              "combat looting consumes only the standard action");
        wait(320);
        tap("a");
        check(!isInventoryMenuOpen() && hasItem(actor->character, ITEM_RATIONS),
              "corpse item transfers during combat");
        check(combat.active && canUseMoveAction(actor->turn),
              "loot-first flow retains normal movement");
        Character secondRecipient{};
        check(!takeCorpseLootItem(*corpse, 0, secondRecipient),
              "removed corpse item cannot be duplicated");

        Entity* nextActor = spawnMonster(
            dungeon.entities, dungeon.entityCount,
            MONSTER_GOBLIN_SCIMITAR, 0, 0);
        check(nextActor != nullptr, "turn monster fixture");
        Direction nextDirection = openDirection(*actor);
        placeAdjacent(*nextActor, *actor, nextDirection);
        nextActor->visibleToPlayer = true;
        nextActor->playerHasLineOfSight = true;

        Entity* secondCorpse = spawnMonster(
            dungeon.entities, dungeon.entityCount,
            MONSTER_GOBLIN_SCIMITAR, 0, 0);
        check(secondCorpse != nullptr, "second corpse fixture");
        Direction secondCorpseDirection = openDirection(*actor);
        placeAdjacent(*secondCorpse, *actor, secondCorpseDirection);
        secondCorpse->character.state = STATE_DEAD;
        secondCorpse->character.health.currentHP = 0;
        secondCorpse->loot.generated = true;
        secondCorpse->loot.itemCount = 1;
        secondCorpse->loot.slots[0].item = makeItemInstance(ITEM_RATIONS);
        secondCorpse->loot.slots[0].quantity = 1;

        combat.combatantCount = 2;
        combat.currentTurnIndex = 0;
        combat.initiativeOrder[0] = actor;
        combat.initiativeOrder[1] = nextActor;
        combat.waitingForPlayer = true;
        actor->turn = TurnState{};
        actor->turn.turnActive = true;
        actor->turn.moveActionUsed = true;
        actor->turn.movementRemaining = 4;
        moveDirection = secondCorpseDirection;
        check(tryInteractWithFacingEntity(), "move then loot is legal");
        check(actor->turn.moveActionUsed && actor->turn.standardActionUsed,
              "move then loot spends both main actions");

        tft.fillScreen(ST77XX_BLACK);
        drawEntity(*actor);
        drawEntity(*nextActor);
        check(hasOutline(*actor, ST77XX_CYAN),
              "active player receives cyan turn outline");
        check(!hasOutline(*nextActor, ST77XX_CYAN),
              "inactive monster has no turn outline");
        dirtyTileCount = 0;
        closeInventoryMenu();
        check(getCurrentCombatant() == nextActor,
              "closing move-then-loot advances the turn");
        check(std::string(getGameMessage()) ==
              std::string(getEntityName(nextActor)) + "'s turn.",
              "initiative transition emits named turn message once");
        check(dirtyTileCount > 0,
              "turn transition dirties old and new footprints");
        redrawDirtyTiles();
        check(!hasOutline(*actor, ST77XX_CYAN),
              "previous turn outline is removed");
        check(hasOutline(*nextActor, ST77XX_CYAN),
              "new active combatant receives outline");

        combat.active = false;
        tft.fillScreen(ST77XX_BLACK);
        drawEntity(*actor);
        drawEntity(*nextActor);
        check(!hasOutline(*actor, ST77XX_CYAN) &&
              !hasOutline(*nextActor, ST77XX_CYAN),
              "turn outlines disappear when combat ends");

        std::cout << "Production Batch 2A combat tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
