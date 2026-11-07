#include "runtime.h"
#include "controller_slots.h"
#include "characters/abilities.h"
#include "characters/conditions.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/abilityresolver.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "graphics/actionpose.h"
#include "graphics/display.h"
#include "input/menu.h"
#include "map/activemap.h"

#include <Arduino.h>
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
    actor->x = 6;
    actor->y = 6;
    actor->active = true;
    actor->character.state = STATE_ALIVE;
    actor->character.team = TEAM_PLAYER;
    actor->character.level = 5;
    actor->character.abilities.strength = 16;
    actor->character.abilities.dexterity = 16;
    actor->character.magic.currentMP = 50;
    actor->character.magic.maxMP = 50;
    actor->sprite = getPlayerSprite(actor->character.characterClass);
    clearEntityActionPose(*actor);
    actor->character.equipment.equipped[SLOT_MELEE_WEAPON] =
        makeItemInstance(ITEM_LONGSWORD);
    actor->character.equipment.equipped[SLOT_RANGED_WEAPON] =
        makeItemInstance(ITEM_SHORTBOW);
    for (int y = 0; y < ROOM_HEIGHT; ++y)
        for (int x = 0; x < ROOM_WIDTH; ++x)
            dungeon.rooms[dungeon.currentRoom].map.tiles[y][x] = TILE_FLOOR;
    for (uint8_t i = 0; i < dungeon.entityCount; ++i)
        if (&dungeon.entities[i] != actor)
            dungeon.entities[i] = Entity{};
    combat = Combat{};
    return actor;
}

Entity* addMonster(MonsterID monsterID, int x, int y)
{
    Entity* monster = spawnMonster(
        dungeon.entities, dungeon.entityCount, monsterID,
        static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    check(monster != nullptr, "monster");
    monster->active = true;
    monster->revealedToPlayer = true;
    monster->awareOfPlayer = true;
    monster->character.state = STATE_ALIVE;
    monster->character.team = TEAM_MONSTER;
    monster->character.health.currentHP = 100;
    monster->character.health.maxHP = 100;
    return monster;
}

void prepareCombat(Entity& actor, Entity& target)
{
    combat = Combat{};
    combat.active = true;
    combat.phase = COMBAT_TURN;
    combat.waitingForPlayer = true;
    combat.combatantCount = 2;
    combat.currentTurnIndex = 0;
    combat.initiativeOrder[0] = &actor;
    combat.initiativeOrder[1] = &target;
    actor.turn = TurnState{};
    actor.turn.turnActive = true;
    actor.turn.movementRemaining = 6;
}

uint32_t footprintHash(const Entity& entity)
{
    uint32_t hash = 2166136261u;
    const uint16_t* frame = pixels();
    for (int y = entity.y * TILE_SIZE;
         y < entity.y * TILE_SIZE + entity.spriteHeight; ++y)
        for (int x = entity.x * TILE_SIZE;
             x < entity.x * TILE_SIZE + entity.spriteWidth; ++x)
            hash = (hash ^ frame[y * 240 + x]) * 16777619u;
    return hash;
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
        Entity* actor = prepareActor();
        Entity* target = addMonster(MONSTER_GOBLIN_SCIMITAR, 7, 6);

        moveDirection = DIR_EAST;
        prepareCombat(*actor, *target);
        beginPlayerAttack(COMBAT_ATTACK_MELEE);
        confirmPlayerAttack();
        check(isEntityActionPoseActive(*actor), "melee attack action pose");

        clearEntityActionPose(*actor);
        target->x = 10;
        prepareCombat(*actor, *target);
        beginPlayerAttack(COMBAT_ATTACK_RANGED);
        combat.selectedTargetIndex = static_cast<int8_t>(target - dungeon.entities);
        confirmPlayerAttack();
        check(isEntityActionPoseActive(*actor), "ranged attack action pose");

        clearEntityActionPose(*actor);
        Entity* spider = addMonster(MONSTER_GIANT_SPIDER, 7, 6);
        combat = Combat{};
        beginMonsterAttack(spider, actor, COMBAT_ATTACK_MELEE);
        check(isEntityActionPoseActive(*spider), "natural attack action pose");

        combat = Combat{};
        actor->character.characterClass = CLASS_WIZARD;
        actor->sprite = getPlayerSprite(CLASS_WIZARD);
        learnAbility(actor->character, ABILITY_MAGIC_MISSILE);
        clearEntityActionPose(*actor);
        check(resolveAbility(*actor, target, ABILITY_MAGIC_MISSILE).result ==
                  ABILITY_RESULT_SUCCESS,
              "offensive spell resolves");
        check(isEntityActionPoseActive(*actor), "offensive spell action pose");

        learnAbility(actor->character, ABILITY_CURE_LIGHT_WOUNDS);
        actor->character.health.maxHP = 30;
        actor->character.health.currentHP = 10;
        clearEntityActionPose(*actor);
        check(resolveAbility(*actor, actor, ABILITY_CURE_LIGHT_WOUNDS).result ==
                  ABILITY_RESULT_SUCCESS,
              "healing spell resolves");
        check(isEntityActionPoseActive(*actor), "healing spell action pose");

        learnAbility(actor->character, ABILITY_SHIELD);
        clearEntityActionPose(*actor);
        check(resolveAbility(*actor, actor, ABILITY_SHIELD).result ==
                  ABILITY_RESULT_SUCCESS,
              "buff spell resolves");
        check(isEntityActionPoseActive(*actor), "buff spell action pose");

        actor->character.characterClass = CLASS_CLERIC;
        actor->sprite = getPlayerSprite(CLASS_CLERIC);
        actor->character.classAbilities.channelEnergyCurrent = 1;
        actor->character.health.currentHP = 10;
        clearEntityActionPose(*actor);
        check(useChannelEnergy(*actor), "Channel Energy resolves");
        check(isEntityActionPoseActive(*actor), "Channel Energy action pose");

        Entity* skeleton = addMonster(MONSTER_SKELETON, 8, 6);
        learnAbility(actor->character, ABILITY_TURN_UNDEAD);
        actor->character.magic.currentMP = 50;
        clearEntityActionPose(*actor);
        prepareCombat(*actor, *skeleton);
        beginPlayerAbility(ABILITY_TURN_UNDEAD);
        check(!isEntityActionPoseActive(*actor),
              "Turn Undead preview has no action pose");
        confirmPlayerAbility();
        check(isEntityActionPoseActive(*actor), "Turn Undead action pose");

        actor->character.characterClass = CLASS_WIZARD;
        actor->sprite = getPlayerSprite(CLASS_WIZARD);
        clearEntityActionPose(*actor);
        actor->character.magic.currentMP = 50;
        beginPlayerAbility(ABILITY_MAGIC_MISSILE);
        check(!isEntityActionPoseActive(*actor), "preview has no action pose");
        cancelPlayerAbility();
        check(!isEntityActionPoseActive(*actor), "cancel has no action pose");
        actor->character.magic.currentMP = 0;
        check(resolveAbility(*actor, target, ABILITY_MAGIC_MISSILE).result !=
                  ABILITY_RESULT_SUCCESS,
              "rejected spell");
        check(!isEntityActionPoseActive(*actor), "rejected spell has no pose");

        actor->character.magic.currentMP = 50;
        triggerEntityActionPose(*actor);
        delay(999);
        check(isEntityActionPoseActive(*actor), "pose remains at 999 ms");
        triggerEntityActionPose(*actor);
        delay(999);
        check(isEntityActionPoseActive(*actor), "repeat extends pose");
        delay(1);
        dirtyTileCount = 0;
        updateEntityActionPoses();
        check(!isEntityActionPoseActive(*actor), "pose expires at 1000 ms");
        check(dirtyTileCount > 0, "expiration dirties footprint");

        tft.fillScreen(0);
        clearEntityActionPose(*actor);
        drawEntity(*actor);
        const uint32_t normalHash = footprintHash(*actor);
        triggerEntityActionPose(*actor);
        tft.fillScreen(0);
        drawEntity(*actor);
        check(footprintHash(*actor) != normalHash, "action frame is distinct");

        addCondition(actor->character, CONDITION_POISONED, 1, 2);
        tft.fillScreen(0);
        drawEntity(*actor);
        check(footprintHash(*actor) != normalHash,
              "poison tint applies to action frame");

        removeCondition(actor->character, CONDITION_POISONED);
        actor->character.state = STATE_ALIVE;
        prepareCombat(*actor, *target);
        tft.fillScreen(0);
        drawEntity(*actor);
        check(pixels()[actor->y * TILE_SIZE * 240 + actor->x * TILE_SIZE] ==
                  ST77XX_CYAN,
              "active-turn outline remains visible over action frame");

        actor->character.state = STATE_UNCONSCIOUS;
        tft.fillScreen(0);
        drawEntity(*actor);
        const uint32_t koActionHash = footprintHash(*actor);
        clearEntityActionPose(*actor);
        tft.fillScreen(0);
        drawEntity(*actor);
        check(footprintHash(*actor) == koActionHash, "KO overrides action frame");

        std::cout << "action pose tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
