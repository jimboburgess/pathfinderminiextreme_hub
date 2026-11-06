#include "runtime.h"
#include "controller_slots.h"
#include "characters/abilities.h"
#include "characters/conditions.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/abilityresolver.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "graphics/display.h"
#include "input/menu.h"
#include "map/activemap.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

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

Entity* prepareOpenRoom()
{
    Entity* actor = getActiveMapPlayer();
    check(actor != nullptr, "active player");
    actor->x = 7;
    actor->y = 6;
    actor->character.characterClass = CLASS_WIZARD;
    actor->character.level = 5;
    actor->character.state = STATE_ALIVE;
    actor->character.team = TEAM_PLAYER;
    actor->character.magic.currentMP = 40;
    actor->character.magic.maxMP = 40;
    actor->turn = TurnState{};
    for (int y = 0; y < ROOM_HEIGHT; ++y)
        for (int x = 0; x < ROOM_WIDTH; ++x)
            dungeon.rooms[dungeon.currentRoom].map.tiles[y][x] = TILE_FLOOR;
    for (uint8_t index = 0; index < dungeon.entityCount; ++index)
    {
        Entity& entity = dungeon.entities[index];
        if (&entity != actor && entity.type == ENTITY_MONSTER)
            entity.active = false;
    }
    return actor;
}

Entity* spawnTarget(int x, int y)
{
    Entity* target = spawnMonster(
        dungeon.entities, dungeon.entityCount,
        MONSTER_GIANT_SPIDER,
        static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    check(target != nullptr, "spawn target");
    target->active = true;
    target->awareOfPlayer = false;
    target->revealedToPlayer = true;
    target->character.state = STATE_ALIVE;
    target->character.team = TEAM_MONSTER;
    target->character.health.maxHP = 100;
    target->character.health.currentHP = 100;
    return target;
}

void resetExploration(Entity& actor)
{
    combat = Combat{};
    actor.turn = TurnState{};
    closeMenu();
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
        Entity* actor = prepareOpenRoom();

        check(feetToTiles(30) == 6, "thirty feet is six tiles");
        check(isAbilityTargetTileInRange(*actor, ABILITY_FIREBALL, 13, 6),
              "target range includes six-tile boundary");
        check(!isAbilityTargetTileInRange(*actor, ABILITY_FIREBALL, 14, 6),
              "target range excludes seventh tile");
        check(isAbilityTargetTileInRange(*actor, ABILITY_BLESS, 14, 11),
              "room-wide ally buff range matches resolver legality");
        check(isTileInRadiusAbilityArea(
                  *actor, ABILITY_FIREBALL, 10, 6, 11, 7),
              "burst includes radius boundary");
        check(!isTileInRadiusAbilityArea(
                  *actor, ABILITY_FIREBALL, 10, 6, 12, 6),
              "burst excludes beyond radius");
        check(isTileInDirectionalAbilityArea(
                  *actor, ABILITY_BURNING_HANDS, DIR_NORTH, 7, 3),
              "north cone reaches forward");
        check(isTileInDirectionalAbilityArea(
                  *actor, ABILITY_BURNING_HANDS, DIR_EAST, 10, 6),
              "east cone reaches forward");
        check(isTileInDirectionalAbilityArea(
                  *actor, ABILITY_BURNING_HANDS, DIR_SOUTH, 7, 9),
              "south cone reaches forward");
        check(isTileInDirectionalAbilityArea(
                  *actor, ABILITY_BURNING_HANDS, DIR_WEST, 4, 6),
              "west cone reaches forward");
        check(!isTileInDirectionalAbilityArea(
                  *actor, ABILITY_BURNING_HANDS, DIR_EAST, 7, 3),
              "east cone rotates away from north tile");
        check(isTileInDirectionalAbilityArea(
                  *actor, ABILITY_LIGHTNING_BOLT, DIR_EAST, 13, 6),
              "line reaches six-tile boundary");
        check(isTileInCenteredAbilityArea(
                  *actor, ABILITY_CHANNEL_ENERGY, 13, 6),
              "Channel Energy includes six-tile boundary");
        check(!isTileInCenteredAbilityArea(
                  *actor, ABILITY_CHANNEL_ENERGY, 14, 6),
              "Channel Energy excludes seventh tile");
        check(isTileInCenteredAbilityArea(
                  *actor, ABILITY_TURN_UNDEAD, 13, 6),
              "Turn Undead shares the six-tile radius");
        check(!isTileInRadiusAbilityArea(
                  *actor, ABILITY_FIREBALL, 0, 0, -1, 0),
              "radius preview clips at map edge");
        dungeon.rooms[dungeon.currentRoom].map.tiles[6][9] = TILE_WALL;
        check(!isTileInRadiusAbilityArea(
                  *actor, ABILITY_FIREBALL, 10, 6, 11, 6),
              "radius preview follows resolver line of sight");
        check(!isTileInCenteredAbilityArea(
                  *actor, ABILITY_TURN_UNDEAD, 11, 6),
              "Turn Undead preview follows resolver line of sight");
        check(isTileInCenteredAbilityArea(
                  *actor, ABILITY_CHANNEL_ENERGY, 11, 6),
              "Channel Energy keeps its wall-independent radius rule");
        dungeon.rooms[dungeon.currentRoom].map.tiles[6][9] = TILE_FLOOR;

        learnAbility(actor->character, ABILITY_CURE_LIGHT_WOUNDS);
        check(isMenuItemVisible(MENU_CAST_SPELL),
              "Cast Spell is visible during exploration");
        actor->character.health.maxHP = 30;
        actor->character.health.currentHP = 20;
        actor->turn.standardActionUsed = true;
        const int healMP = actor->character.magic.currentMP;
        resetExploration(*actor);
        actor->turn.standardActionUsed = true;
        beginPlayerAbility(ABILITY_CURE_LIGHT_WOUNDS);
        check(isPlayerTargetingAbility(), "exploration heal targets normally");
        confirmPlayerAbility();
        check(!combat.active, "exploration heal does not start combat");
        check(actor->character.health.currentHP > 20, "exploration heal applies");
        check(actor->character.magic.currentMP == healMP - 2,
              "exploration heal spends MP on commit");

        learnAbility(actor->character, ABILITY_SHIELD);
        removeCondition(actor->character, CONDITION_BUFF_AC);
        const int shieldMP = actor->character.magic.currentMP;
        beginPlayerAbility(ABILITY_SHIELD);
        check(!combat.active, "exploration buff does not start combat");
        check(actor->character.magic.currentMP == shieldMP - 2,
              "exploration buff spends MP");
        check(hasCondition(actor->character, CONDITION_BUFF_AC),
              "exploration buff applies shared condition");

        Entity* target = spawnTarget(10, 6);
        learnAbility(actor->character, ABILITY_MAGIC_MISSILE);
        const int cancelMP = actor->character.magic.currentMP;
        beginPlayerAbility(ABILITY_MAGIC_MISSILE);
        check(isPlayerTargetingAbility(), "hostile spell targeting starts");
        cancelPlayerAbility();
        check(!isPlayerTargetingAbility(), "cancel clears targeting");
        check(!combat.active, "cancel does not start combat");
        check(actor->character.magic.currentMP == cancelMP,
              "cancel spends no MP");

        learnAbility(actor->character, ABILITY_FIREBALL);
        closeMenu();
        beginPlayerAbility(ABILITY_FIREBALL);
        check(isPlayerTargetingGroundAbility(), "ground spell targeting");
        wait(30);
        int originX = 0;
        int originY = 0;
        check(getSelectedAbilityGroundTarget(originX, originY),
              "ground origin selected");
        const uint16_t* frame = pixels();
        check(frame[(originY * TILE_SIZE + 1) * 240 +
                    originX * TILE_SIZE + 1] == 0xC81F,
              "AoE preview reaches production framebuffer");
        cancelPlayerAbility();
        check(!combat.active, "ground cancel remains exploration");

        resetExploration(*actor);
        actor->character.magic.currentMP = 40;
        actor->turn.standardActionUsed = false;
        const int targetHP = target->character.health.currentHP;
        beginPlayerAbility(ABILITY_MAGIC_MISSILE);
        check(getSelectedAbilityTarget() == target,
              "hostile target selected");
        confirmPlayerAbility();
        check(combat.active, "hostile exploration cast starts combat");
        check(combat.openingAbilityInProgress,
              "hostile spell is the opening action");
        check(combat.abilityResolutionPending,
              "opening result uses normal ability presentation");
        check(target->character.health.currentHP < targetHP,
              "opening spell resolves once");
        const int resolvedHP = target->character.health.currentHP;
        check(actor->turn.standardActionUsed,
              "opening spell commits the standard action");
        for (int i = 0; i < 800 && combat.openingAbilityInProgress; ++i)
            wait(10);
        check(!combat.openingAbilityInProgress,
              "opening spell transitions to initiative");
        check(combat.openingAbilityPlayerID == actor->ownerPlayerID,
              "opening caster action marker survives initiative setup");
        check(combat.active && combat.phase == COMBAT_INITIATIVE,
              "normal initiative follows opening spell");
        check(target->character.health.currentHP == resolvedHP,
              "opening spell is not duplicated");
        uint8_t actorTurn = 0;
        for (; actorTurn < combat.combatantCount; ++actorTurn)
            if (combat.initiativeOrder[actorTurn] == actor) break;
        check(actorTurn < combat.combatantCount,
              "opening caster remains in initiative");
        combat.currentTurnIndex = actorTurn;
        wait(2100);
        Entity* openingCasterTurn = getCurrentCombatant();
        check(openingCasterTurn != nullptr, "opening caster turn exists");
        check(openingCasterTurn->ownerPlayerID == actor->ownerPlayerID,
              "opening caster owns the selected initiative turn");
        check(combat.openingAbilityPlayerID == INVALID_PLAYER_ID,
              "opening action marker is consumed at turn start");
        check(openingCasterTurn->turn.standardActionUsed,
              "opening spell gives no free second standard action");
        check(!openingCasterTurn->turn.moveActionUsed,
              "opening caster retains the first move action");

        resetExploration(*actor);
        actor->character.characterClass = CLASS_CLERIC;
        actor->character.classAbilities.channelEnergyCurrent = 1;
        beginChannelEnergyPreview();
        check(isPlayerTargetingCenteredAbility(),
              "Channel Energy shows centered preview");
        cancelPlayerAbility();
        check(actor->character.classAbilities.channelEnergyCurrent == 1,
              "centered preview cancel spends no use");

        combat = Combat{};
        combat.active = true;
        combat.phase = COMBAT_TURN;
        combat.waitingForPlayer = true;
        combat.combatantCount = 1;
        combat.initiativeOrder[0] = actor;
        actor->turn = TurnState{};
        learnAbility(actor->character, ABILITY_TURN_UNDEAD);
        beginPlayerAbility(ABILITY_TURN_UNDEAD);
        check(isPlayerTargetingCenteredAbility(),
              "Turn Undead shows centered preview");
        cancelPlayerAbility();

        std::cout << "PASS exploration spellcasting, shared geometry, "
                     "opening hostile casts, and ability previews\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << "; "
                  << diagnostic() << '\n';
        return 1;
    }
}
