//
// Created by james on 7/12/2026.
//

#include "dungeon.h"
#include <Arduino.h>
#include "roomgen.h"
#include "dungeongraph.h"
#include "riddlepuzzle.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "combat.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "map/awareness.h"
#include "map/mapeffects.h"

Dungeon dungeon;

namespace
{
MonsterID getThemedMonster(EncounterTheme theme, uint8_t spawnIndex)
{
    switch (theme)
    {
        case ENCOUNTER_GOBLIN:
        {
            static constexpr MonsterID monsters[] = {
                MONSTER_GOBLIN_SCIMITAR,
                MONSTER_GOBLIN_ARCHER,
                MONSTER_BUGBEAR};
            return monsters[spawnIndex % 3];
        }

        case ENCOUNTER_UNDEAD:
        {
            static constexpr MonsterID monsters[] = {
                MONSTER_SKELETON,
                MONSTER_ZOMBIE,
                MONSTER_GHOUL,
                MONSTER_WIGHT};
            return monsters[spawnIndex % 4];
        }

        case ENCOUNTER_ABERRATION:
        {
            static constexpr MonsterID monsters[] = {
                MONSTER_GRAY_OOZE,
                MONSTER_VIOLET_FUNGUS,
                MONSTER_CHOKER,
                MONSTER_SPECTATOR};
            return monsters[spawnIndex % 4];
        }

        case ENCOUNTER_NONE:
        default:
            return MONSTER_GOBLIN_SCIMITAR;
    }
}

RoomType selectMiddleRoomType()
{
    const uint8_t roll = random(100);

    if (roll < 30)
        return ROOM_COMBAT;
    if (roll < 60)
        return ROOM_AMBUSH;
    if (roll < 90)
        return ROOM_PUZZLE;

    return ROOM_EMPTY;
}

void resetRoomTurnState(DungeonRoomRuntime& runtime)
{
    for (uint8_t i = 0; i < runtime.entityCount; i++)
        runtime.entities[i].turn = TurnState{};
}

void updateRoomCompletion(Dungeon& dungeon, uint8_t roomIndex)
{
    if (roomIndex >= dungeon.roomCount)
        return;

    DungeonRoomRuntime& runtime = dungeon.roomRuntime[roomIndex];

    if (!runtime.initialized)
        return;

    bool hasLivingMonster = false;

    for (uint8_t i = 0; i < runtime.entityCount; i++)
    {
        const Entity& entity = runtime.entities[i];

        if (entity.active && entity.type == ENTITY_MONSTER &&
            entity.character.team == TEAM_MONSTER &&
            entity.character.state == STATE_ALIVE)
        {
            hasLivingMonster = true;
            break;
        }
    }

    if (isRiddlemanPuzzleRoom(dungeon.rooms[roomIndex]))
    {
        dungeon.rooms[roomIndex].completed =
            dungeon.rooms[roomIndex].npcSpawn.puzzleState == RIDDLE_ROOM_COMPLETE;
    }
    else
    {
        dungeon.rooms[roomIndex].completed = !hasLivingMonster;
    }

    if (roomIndex == dungeon.treasureRoom)
    {
        for (uint8_t i = 0; i < runtime.entityCount; i++)
        {
            const Entity& entity = runtime.entities[i];
            if (entity.active && entity.type == ENTITY_CHEST &&
                entity.loot.generated && entity.loot.itemCount == 0 &&
                entity.loot.gold == 0)
            {
                dungeon.finalTreasureLooted = true;
            }
        }
    }

    if (roomIndex == dungeon.bossRoom)
        dungeon.finalEncounterCleared = !hasLivingMonster;
}

void detachLoadedDungeonPlayer(Dungeon& dungeon)
{
    if (dungeon.loadedRoom >= MAX_ROOMS || dungeon.entities == nullptr)
        return;

    DungeonRoomRuntime& runtime =
        dungeon.roomRuntime[dungeon.loadedRoom];
    runtime.entityCount = dungeon.entityCount;

    Entity* playerEntity = nullptr;

    if (runtime.playerSlot < runtime.entityCount)
    {
        Entity& reservedPlayer = runtime.entities[runtime.playerSlot];

        if (reservedPlayer.active && reservedPlayer.type == ENTITY_PLAYER)
            playerEntity = &reservedPlayer;
    }

    // Defensive compatibility for a room created before the reserved-slot
    // convention was established.
    if (playerEntity == nullptr)
        playerEntity = getPlayerEntity(runtime.entities, runtime.entityCount);

    if (playerEntity != nullptr)
    {
        player = playerEntity->character;
        *playerEntity = Entity{};
    }

    resetRoomTurnState(runtime);
    updateRoomCompletion(dungeon, dungeon.loadedRoom);
}

void initializeRoomEntities(
    Dungeon& dungeon,
    DungeonRoom& room,
    DungeonRoomRuntime& runtime)
{
    runtime.entityCount = 0;
    runtime.playerSlot = NO_ENTITY_SLOT;

    for (uint8_t i = 0; i < MAX_ENTITIES; i++)
        runtime.entities[i] = Entity{};

    dungeon.entityCount = 0;
    uint8_t themedSpawnIndex = 0;

    // Marker tiles are consumed exactly once for this dungeon run. Subsequent
    // visits bind the same runtime array instead of reconstructing monsters.
    for (int y = 0; y < ROOM_SIZE; y++)
    {
        for (int x = 0; x < ROOM_SIZE; x++)
        {
            switch (room.map.tiles[y][x])
            {
                case TILE_ENEMY_START:
                    spawnMonster(
                        dungeon.entities,
                        dungeon.entityCount,
                        getThemedMonster(
                            room.encounterTheme,
                            themedSpawnIndex++),
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_GIANT_SPIDER_START:
                    spawnMonster(
                        dungeon.entities,
                        dungeon.entityCount,
                        MONSTER_GIANT_SPIDER,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_SKELETON_MAGE_START:
                    spawnMonster(
                        dungeon.entities,
                        dungeon.entityCount,
                        MONSTER_SKELETON_MAGE,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_SKELETON_START:
                    spawnMonster(
                        dungeon.entities,
                        dungeon.entityCount,
                        MONSTER_SKELETON,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_CHEST_SPAWN:
                {
                    Entity* chest = spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_CHEST,
                        x,
                        y);
                    if (chest != nullptr)
                    {
                        chest->locked = true;
                        chest->opened = false;
                        chest->sprite = chestclosed;
                    }
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;
                }

                case TILE_LOOT_SPAWN:
                    spawnEntity(
                        dungeon.entities,
                        dungeon.entityCount,
                        ENTITY_LOOT,
                        x,
                        y);
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                case TILE_NPC_SPAWN:
                    if (room.npcSpawn.id == NPC_NONE)
                    {
                        room.npcSpawn.id = NPC_BERTRAM_RIDDLEMAN;
                        room.npcSpawn.x = static_cast<int8_t>(x);
                        room.npcSpawn.y = static_cast<int8_t>(y);
                    }
                    room.map.tiles[y][x] = TILE_FLOOR;
                    break;

                default:
                    break;
            }
        }
    }

    if (room.npcSpawn.id != NPC_NONE)
    {
        if (room.npcSpawn.id == NPC_BERTRAM_RIDDLEMAN &&
            room.npcSpawn.riddle.id == RIDDLE_NONE)
        {
            const uint8_t shuffleRolls[3] = {
                static_cast<uint8_t>(random(256)),
                static_cast<uint8_t>(random(256)),
                static_cast<uint8_t>(random(256))};
            initializeRiddleState(
                room.npcSpawn.riddle,
                static_cast<RiddleID>(random(RIDDLE_COUNT)),
                shuffleRolls);
        }
        spawnNPC(
            dungeon.entities,
            dungeon.entityCount,
            room.npcSpawn.id,
            static_cast<uint8_t>(room.npcSpawn.x),
            static_cast<uint8_t>(room.npcSpawn.y));
    }

    runtime.entityCount = dungeon.entityCount;
    runtime.initialized = true;
}

Entity* attachDungeonPlayer(
    Dungeon& dungeon,
    DungeonRoomRuntime& runtime)
{
    if (runtime.playerSlot == NO_ENTITY_SLOT)
    {
        if (dungeon.entityCount >= MAX_ENTITIES)
            return nullptr;

        runtime.playerSlot = dungeon.entityCount++;
    }

    if (runtime.playerSlot >= MAX_ENTITIES)
        return nullptr;

    if (runtime.playerSlot >= dungeon.entityCount)
        dungeon.entityCount = runtime.playerSlot + 1;

    Entity& playerEntity = dungeon.entities[runtime.playerSlot];
    playerEntity = Entity{};
    playerEntity.active = true;
    playerEntity.type = ENTITY_PLAYER;
    playerEntity.character = player;
    playerEntity.sprite = getPlayerSprite(
        playerEntity.character.characterClass);

    runtime.entityCount = dungeon.entityCount;
    return &playerEntity;
}

bool findSafePlayerEntry(
    const Dungeon& dungeon,
    const DungeonRoom& room,
    uint8_t requestedX,
    uint8_t requestedY,
    uint8_t& resultX,
    uint8_t& resultY)
{
    int bestDistance = ROOM_SIZE * 2;
    bool found = false;

    for (uint8_t y = 0; y < ROOM_SIZE; y++)
    {
        for (uint8_t x = 0; x < ROOM_SIZE; x++)
        {
            if (room.map.tiles[y][x] != TILE_FLOOR ||
                getEntityAt(
                    dungeon.entities,
                    dungeon.entityCount,
                    x,
                    y) != nullptr)
            {
                continue;
            }

            const int distance =
                abs(static_cast<int>(x) - requestedX) +
                abs(static_cast<int>(y) - requestedY);

            if (!found || distance < bestDistance)
            {
                found = true;
                bestDistance = distance;
                resultX = x;
                resultY = y;
            }
        }
    }

    return found;
}
}

const char* roomTypeName(RoomType type)
{
    switch (type)
    {
        case ROOM_ENTRANCE:     return "Entrance";
        case ROOM_COMBAT:       return "Combat";
        case ROOM_AMBUSH:       return "Ambush";
        case ROOM_PUZZLE:       return "Puzzle";
        case ROOM_TREASURE:     return "Treasure";
        case ROOM_EMPTY:        return "Empty";
        case ROOM_BOSS:         return "Boss";
    }

    return "Unknown";
}

DungeonRubblePlan createDungeonRubblePlan(
    uint8_t themeRoll,
    uint8_t guaranteedMiddleRoomRoll,
    uint8_t optionalRoomChanceRoll,
    uint8_t optionalMiddleRoomRoll)
{
    DungeonRubblePlan plan{};
    plan.enabled = themeRoll < DUNGEON_RUBBLE_THEME_CHANCE_PERCENT;

    if (!plan.enabled)
        return plan;

    const uint8_t guaranteedRoom =
        guaranteedMiddleRoomRoll % MIDDLE_ROOM_COUNT;
    plan.middleRooms[guaranteedRoom] = true;

    // At most one additional middle room receives rubble. This keeps the
    // theme visible without making every ordinary room difficult terrain.
    if (optionalRoomChanceRoll < OPTIONAL_RUBBLE_ROOM_CHANCE_PERCENT)
    {
        const uint8_t offset = 1 + optionalMiddleRoomRoll %
            (MIDDLE_ROOM_COUNT - 1);
        const uint8_t optionalRoom =
            (guaranteedRoom + offset) % MIDDLE_ROOM_COUNT;
        plan.middleRooms[optionalRoom] = true;
    }

    return plan;
}

void enterDungeon()
{
    // Entering a map must never inherit a stale encounter. Return to Town is
    // the primary cleanup path; this is the requested defensive safety net.
    abortCombat();

    const bool resumingRun = hasResumableDungeon(dungeon);

    // A completed run is not a resumable adventure. Normally town travel has
    // already reset it, but keep entry defensive if completion was reached by
    // another path.
    if (dungeon.runActive && !resumingRun)
        resetDungeonRun(dungeon);

    if (resumingRun)
    {
        dungeon.currentRoom = 0;
        loadRoom(dungeon, ENTRY_START);
    }
    else
    {
        generateDungeon(dungeon);
    }

    gameState = GAME_DUNGEON;
    setGameMessage(resumingRun ? "Resumed dungeon" : "Entered dungeon");

    Entity* player = getPlayerEntity(
        dungeon.entities,
        dungeon.entityCount);

    if (player)
    {
        previousPlayerPosition.x = player->x;
        previousPlayerPosition.y = player->y;
    }

    moveDirection = DIR_EAST;
    previousMoveDirection = moveDirection;

    backgroundNeedsRedraw = true;

    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

void generateDungeon(Dungeon& dungeon)
{
    resetDungeonRun(dungeon);
    dungeon.currentRoom = 0;

    const uint8_t targetRoomCount = static_cast<uint8_t>(random(
        MIN_DUNGEON_ROOMS, MAX_DUNGEON_ROOMS + 1));
    bool topologyGenerated = false;
    for (uint8_t attempt = 0; attempt < 24 && !topologyGenerated; attempt++)
        topologyGenerated = generateDungeonTopology(dungeon, targetRoomCount);
    if (!topologyGenerated)
        return;

    Direction riddleExitDirection = DIR_NORTH;
    const Direction directions[4] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
    for (uint8_t roomIndex = 1; roomIndex < dungeon.roomCount; ++roomIndex)
    {
        if (roomIndex == dungeon.bossRoom || roomIndex == dungeon.treasureRoom) continue;
        for (Direction direction : directions)
        {
            if (getRoomNeighbor(dungeon.rooms[roomIndex], direction) == dungeon.bossRoom)
            {
                dungeon.riddleRoom = roomIndex;
                riddleExitDirection = direction;
                break;
            }
        }
        if (dungeon.riddleRoom != NO_ROOM) break;
    }
    if (dungeon.riddleRoom == NO_ROOM)
        return;

    dungeon.hasRubbleTheme =
        random(100) < DUNGEON_RUBBLE_THEME_CHANCE_PERCENT;
    bool rubbleRooms[MAX_ROOMS] = {};
    bool ordinaryRubbleSelected = false;
    uint8_t firstOrdinaryRoom = NO_ROOM;
    for (uint8_t roomIndex = 1; roomIndex < dungeon.roomCount; roomIndex++)
    {
        if (roomIndex == dungeon.bossRoom ||
            roomIndex == dungeon.treasureRoom)
            continue;
        if (firstOrdinaryRoom == NO_ROOM)
            firstOrdinaryRoom = roomIndex;
        if (dungeon.hasRubbleTheme &&
            random(100) < OPTIONAL_RUBBLE_ROOM_CHANCE_PERCENT)
        {
            rubbleRooms[roomIndex] = true;
            ordinaryRubbleSelected = true;
        }

        RoomType type = roomIndex == dungeon.riddleRoom
            ? ROOM_PUZZLE : selectMiddleRoomType();
        DungeonRoom& room = dungeon.rooms[roomIndex];
        room.type = type;
        room.encounterTheme =
            (room.type == ROOM_COMBAT || room.type == ROOM_AMBUSH)
                ? static_cast<EncounterTheme>(random(
                    ENCOUNTER_GOBLIN,
                    ENCOUNTER_ABERRATION + 1))
                : ENCOUNTER_NONE;
    }

    if (dungeon.hasRubbleTheme && !ordinaryRubbleSelected &&
        firstOrdinaryRoom != NO_ROOM)
        rubbleRooms[firstOrdinaryRoom] = true;

    dungeon.rooms[0].encounterTheme = ENCOUNTER_NONE;
    dungeon.rooms[dungeon.bossRoom].type = ROOM_BOSS;
    dungeon.rooms[dungeon.bossRoom].encounterTheme = ENCOUNTER_NONE;
    dungeon.rooms[dungeon.treasureRoom].type = ROOM_TREASURE;
    dungeon.rooms[dungeon.treasureRoom].encounterTheme = ENCOUNTER_NONE;

    dungeon.rooms[0].discovered = true;

    // Generate every room
    for (uint8_t i = 0; i < dungeon.roomCount; i++)
    {
        populateRoomConnections(dungeon.rooms[i]);
        dungeon.rooms[i].shape = dungeon.rooms[i].type == ROOM_ENTRANCE
            ? SHAPE_ENTRANCE
            : (i == dungeon.riddleRoom
                ? SHAPE_SQUARE : randomProductionRoomShape(dungeon.rooms[i]));
        generateRoom(dungeon.rooms[i]);

        if (i == dungeon.riddleRoom)
        {
            const uint8_t shuffleRolls[3] = {
                static_cast<uint8_t>(random(256)),
                static_cast<uint8_t>(random(256)),
                static_cast<uint8_t>(random(256))};
            if (!configureRiddlemanPuzzleRoom(
                    dungeon.rooms[i], riddleExitDirection,
                    static_cast<RiddleID>(random(RIDDLE_COUNT)), shuffleRolls))
            {
                resetDungeonRun(dungeon);
                return;
            }
            continue;
        }

        // Major blockers precede difficult terrain and runtime traps so every
        // later placement system sees pillar squares as unavailable.
        populatePillarTerrain(
            dungeon.rooms[i], random(100), random(PILLAR_LAYOUT_COUNT));

        populateDungeonFurniture(
            dungeon.rooms[i], random(100), random(100), random(100),
            random(100));

        if (dungeon.hasRubbleTheme)
        {
            if (i == dungeon.bossRoom)
                populateBossRubbleTerrain(dungeon.rooms[i]);
            else if (rubbleRooms[i])
                populateRubbleTerrain(dungeon.rooms[i]);
        }

        if (i == 0)
            placeHealingFountain(dungeon.rooms[i]);

        // Ordinary graph rooms remain eligible for the existing mixed trap
        // pool. The Entrance, Boss, and final Treasure room stay authored.
        const bool allowRandomTrap =
            i > 0 && i != dungeon.bossRoom &&
            i != dungeon.treasureRoom;
        if (i > 0)
        {
            populateDungeonRoomFeatures(
                dungeon.rooms[i],
                player.level > 0 ? player.level : 1,
                allowRandomTrap);
        }
    }

    dungeon.runActive = true;
    dumpDungeonTopology(dungeon);

    // Load the starting room
    loadRoom(dungeon, ENTRY_START);
}

void loadRoom(Dungeon& dungeon, RoomEntry entry)
{
    if (dungeon.currentRoom >= dungeon.roomCount)
        return;
    clearMapEffects();
    detachLoadedDungeonPlayer(dungeon);

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    DungeonRoomRuntime& runtime =
        dungeon.roomRuntime[dungeon.currentRoom];

    dungeon.entities = runtime.entities;
    dungeon.entityCount = runtime.entityCount;

    if (!runtime.initialized)
        initializeRoomEntities(dungeon, room, runtime);

    resetRoomTurnState(runtime);

    uint8_t entryX = ROOM_SIZE / 2;
    uint8_t entryY = ROOM_SIZE / 2;

    // Cardinal entries use the destination room's connection. The centered
    // values above remain only as a defensive fallback for corrupted data.
    getRoomEntryPosition(room, entry, entryX, entryY);

    // A surviving monster may have moved onto a doorway before the player
    // fled. Resume at the requested entrance when it is free, otherwise use
    // the nearest connected floor tile instead of overlapping an entity.
    if (room.map.tiles[entryY][entryX] != TILE_FLOOR ||
        getEntityAt(
            dungeon.entities,
            dungeon.entityCount,
            entryX,
            entryY) != nullptr)
    {
        if (!findSafePlayerEntry(
                dungeon, room, entryX, entryY, entryX, entryY))
        {
            return;
        }
    }

    Entity* playerEntity = attachDungeonPlayer(dungeon, runtime);

    if (playerEntity == nullptr)
        return;

    playerEntity->x = entryX;
    playerEntity->y = entryY;

    room.discovered = true;
    dungeon.loadedRoom = dungeon.currentRoom;
    runtime.entityCount = dungeon.entityCount;
    resetAwarenessTimer();
}

void suspendDungeonRun(Dungeon& dungeon)
{
    if (!dungeon.runActive)
        return;

    // Remove the transient map copy of the player now, before town rest,
    // shopping, or loading can change the authoritative global Character.
    // Monsters and other room occupants remain exactly where they are.
    detachLoadedDungeonPlayer(dungeon);
}

void resetDungeonRun(Dungeon& dungeon)
{
    dungeon.runActive = false;
    dungeon.entities = nullptr;
    dungeon.entityCount = 0;
    dungeon.loadedRoom = NO_ROOM;
    dungeon.currentRoom = 0;
    dungeon.roomCount = 0;
    dungeon.bossRoom = NO_ROOM;
    dungeon.treasureRoom = NO_ROOM;
    dungeon.riddleRoom = NO_ROOM;
    dungeon.hasRubbleTheme = false;
    dungeon.finalEncounterCleared = false;
    dungeon.finalTreasureLooted = false;
    dungeon.completed = false;

    for (uint8_t roomIndex = 0; roomIndex < MAX_ROOMS; roomIndex++)
    {
        DungeonRoomRuntime& runtime = dungeon.roomRuntime[roomIndex];
        runtime.entityCount = 0;
        runtime.playerSlot = NO_ENTITY_SLOT;
        runtime.initialized = false;

        for (uint8_t entityIndex = 0;
             entityIndex < MAX_ENTITIES;
             entityIndex++)
        {
            runtime.entities[entityIndex] = Entity{};
        }

        dungeon.rooms[roomIndex].discovered = false;
        dungeon.rooms[roomIndex].completed = false;
        dungeon.rooms[roomIndex].npcSpawn = DungeonNPCSpawn{};
        dungeon.rooms[roomIndex].north = NO_ROOM;
        dungeon.rooms[roomIndex].east = NO_ROOM;
        dungeon.rooms[roomIndex].south = NO_ROOM;
        dungeon.rooms[roomIndex].west = NO_ROOM;
        dungeon.rooms[roomIndex].dungeonX = NO_DUNGEON_COORDINATE;
        dungeon.rooms[roomIndex].dungeonY = NO_DUNGEON_COORDINATE;
        clearRoomConnections(dungeon.rooms[roomIndex]);
        for (TrapInstance& trap : dungeon.rooms[roomIndex].traps)
            trap = TrapInstance{};
        for (SuspicionInstance& suspicion :
             dungeon.rooms[roomIndex].suspicions)
        {
            suspicion = SuspicionInstance{};
        }
    }
}

void updateCurrentDungeonRoomCompletion(Dungeon& dungeon)
{
    if (!dungeon.runActive || dungeon.loadedRoom >= dungeon.roomCount)
        return;

    DungeonRoomRuntime& runtime =
        dungeon.roomRuntime[dungeon.loadedRoom];
    runtime.entityCount = dungeon.entityCount;
    updateRoomCompletion(dungeon, dungeon.loadedRoom);
}

bool isDungeonRunComplete(const Dungeon& dungeon)
{
    return dungeon.runActive && dungeon.finalEncounterCleared &&
           dungeon.finalTreasureLooted;
}

bool hasResumableDungeon(const Dungeon& dungeon)
{
    return dungeon.runActive && !dungeon.completed;
}

void markDungeonCompletedOnTownReturn(Dungeon& dungeon)
{
    if (isDungeonRunComplete(dungeon))
        dungeon.completed = true;
}

