#include "characters/charactercreation.h"
#include "characters/conditions.h"
#include "characters/items.h"
#include "data/entities.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "dungeon/furniture.h"
#include "dungeon/traps.h"
#include "map/playermovement.h"
#include "map/skillactions.h"

#include <Arduino.h>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

Entity& makePlayer(uint8_t index, int x, int y)
{
    Entity& entity = dungeon.activeDungeonEntities[index];
    entity = Entity{};
    entity.type = ENTITY_PLAYER;
    entity.ownerPlayerID = index == 0 ? SINGLE_PLAYER_ID : index;
    entity.active = true;
    entity.x = static_cast<uint8_t>(x);
    entity.y = static_cast<uint8_t>(y);
    createCharacter(entity.character, CLASS_ROGUE, WEAPON_GROUP_BLADES);
    entity.character.state = STATE_ALIVE;
    entity.character.team = TEAM_PLAYER;
    entity.character.health.currentHP = 100;
    entity.character.health.maxHP = 100;
    entity.character.abilities.dexterity = 40;
    entity.character.abilities.strength = 40;
    return entity;
}

TrapInstance& resetFixture(uint8_t level = 5)
{
    dungeon = Dungeon{};
    dungeon.entities = dungeon.activeDungeonEntities;
    dungeon.entityCount = 0;
    dungeon.roomCount = 1;
    dungeon.currentRoom = 0;
    dungeon.loadedRoom = 0;
    dungeon.runActive = true;
    DungeonRoom& room = dungeon.rooms[0];
    for (int y = 0; y < ROOM_HEIGHT; y++)
        for (int x = 0; x < ROOM_WIDTH; x++)
            room.map.tiles[y][x] = TILE_FLOOR;
    check(addTrap(
        room, TRAP_COLLAPSING_PIT, 5, 5, level,
        SUSPICION_CRACKED_FLOOR), "pit fixture");
    gameState = GAME_DUNGEON;
    combat = Combat{};
    return *getTrapAt(room, 5, 5);
}

void testGeneratedPitSafety()
{
    bool found = false;
    for (unsigned seed = 1; seed <= 2000 && !found; seed++)
    {
        DungeonRoom room{};
        for (int y = 0; y < ROOM_HEIGHT; y++)
            for (int x = 0; x < ROOM_WIDTH; x++)
                room.map.tiles[y][x] =
                    x == 0 || y == 0 || x == ROOM_WIDTH - 1 ||
                    y == ROOM_HEIGHT - 1 ? TILE_WALL : TILE_FLOOR;
        randomSeed(seed);
        populateDungeonRoomFeatures(room, 5, true);
        for (const TrapInstance& trap : room.traps)
        {
            if (!isCollapsingPit(trap))
                continue;
            found = true;
            check(trap.level == 5 &&
                  getCollapsingPitDepthFeet(trap) == 20,
                  "generated pit fixes depth from dungeon level");
            for (int y = trap.y - 1; y <= trap.y + 1; y++)
                for (int x = trap.x - 1; x <= trap.x + 1; x++)
                    check(room.map.tiles[y][x] == TILE_FLOOR,
                          "generated footprint uses clear floor");
        }
    }
    check(found, "deterministic generation found a safe pit");
}
}

int main()
{
    try
    {
        testGeneratedPitSafety();
        TrapInstance& pit = resetFixture(5);
        Entity& center = makePlayer(0, 5, 5);
        Entity& north = makePlayer(1, 5, 4);
        Entity& flyingCorner = makePlayer(2, 4, 4);
        addCondition(flyingCorner.character, CONDITION_FLYING, 0, 10);
        dungeon.entityCount = 3;
        randomSeed(17);
        check(triggerTrapForEntityAt(center, center.x, center.y),
              "authoritative pit trigger");
        check(pit.triggered && pit.discovered && !isTrapActive(pit),
              "pit persists and becomes one-shot");
        check(hasCondition(center.character, CONDITION_PIT_BOTTOM) &&
              hasCondition(north.character, CONDITION_PIT_BOTTOM),
              "all cross occupants fall independently");
        check(center.character.health.currentHP < 100 &&
              north.character.health.currentHP < 100,
              "individual falling damage applied");
        check(!hasCondition(
                  flyingCorner.character, CONDITION_PIT_BOTTOM) &&
              flyingCorner.character.health.currentHP == 100,
              "flight prevents falling damage");
        const int centerHP = center.character.health.currentHP;
        check(!triggerTrapForEntityAt(center, center.x, center.y) &&
              center.character.health.currentHP == centerHP,
              "duplicate trigger cannot repeat damage");

        center.character.conditions = ConditionData{};
        center.x = 3;
        center.y = 5;
        dungeon.entityCount = 1;
        bool roomChanged = false;
        check(!tryMovePlayerAuthoritative(
                  dungeon, SINGLE_PLAYER_ID, DIR_EAST, true, roomChanged) &&
              center.x == 3 && center.y == 5,
              "upper-floor movement cannot enter open pit");

        addCondition(center.character, CONDITION_PIT_BOTTOM, 20, 0);
        center.x = 5;
        center.y = 5;
        check(tryMovePlayerAuthoritative(
                  dungeon, SINGLE_PLAYER_ID, DIR_EAST, true, roomChanged) &&
              center.x == 6 && center.y == 5,
              "pit-bottom movement stays within open cross");
        check(!tryMovePlayerAuthoritative(
                  dungeon, SINGLE_PLAYER_ID, DIR_EAST, true, roomChanged) &&
              center.x == 6 && center.y == 5,
              "ordinary movement cannot climb a vertical wall");

        lowerRopeIntoCollapsingPit(pit, 3, 5);
        randomSeed(2);
        check(useSkill(SKILL_CLIMB), "Climb action resolves");
        check(!hasCondition(center.character, CONDITION_PIT_BOTTOM),
              "successful Climb reaches upper floor");
        check(!isOpenCollapsingPitTile(
                  dungeon.rooms[0], center.x, center.y),
              "Climb lands on stable geometry");

        addCondition(center.character, CONDITION_PIT_BOTTOM, 20, 0);
        check(addItem(center.character, ITEM_POTION_FLY),
              "fly potion fixture");
        check(useFlyPotion(center.character) &&
              hasCondition(center.character, CONDITION_FLYING) &&
              !hasCondition(center.character, CONDITION_PIT_BOTTOM) &&
              !hasItem(center.character, ITEM_POTION_FLY),
              "fly potion grants traversal and leaves the pit bottom");
        removeCondition(center.character, CONDITION_FLYING);

        center.x = 4;
        center.y = 4;
        center.character.conditions = ConditionData{};
        moveDirection = DIR_SOUTH;
        randomSeed(3);
        check(useSkill(SKILL_ACROBATICS), "pit jump resolves");
        check(center.x == 4 && center.y == 6 &&
              !hasCondition(center.character, CONDITION_PIT_BOTTOM),
              "successful jump crosses one open section");

        center.x = 2;
        center.y = 5;
        check(addDungeonFurniture(
                  dungeon.rooms[0], FURNITURE_CRATE, 3, 5),
              "crate fixture");
        check(tryPushDungeonFurniture(
                  dungeon, center, 3, 5, 1, 0, 99) ==
                  FURNITURE_PUSH_SUCCEEDED,
              "crate can be pushed into one pit tile");
        check(dungeon.rooms[0].map.tiles[5][4] == TILE_CRATE &&
              dungeon.rooms[0].map.tiles[5][5] == TILE_FLOOR &&
              isOpenCollapsingPitTile(dungeon.rooms[0], 5, 5),
              "one crate does not fill the three-by-three pit");

        std::cout << "PASS collapsing pit runtime scenarios=12\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL " << exception.what() << '\n';
        return 1;
    }
}
