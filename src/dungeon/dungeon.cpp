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
#include "map/interaction.h"
#include "input/inventorymenu.h"
#include "input/menu.h"

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

void clearActiveDungeonEntities(Dungeon& dungeon)
{
    for (uint8_t i = 0; i < MAX_ENTITIES; ++i)
        dungeon.activeDungeonEntities[i] = Entity{};
    dungeon.entityCount = 0;
}

void resetActiveRoomTurnState(Dungeon& dungeon)
{
    for (uint8_t i = 0; i < dungeon.entityCount; ++i)
        dungeon.activeDungeonEntities[i].turn = TurnState{};
}

bool persistentMonsterIsLiving(const PersistentEntity& entity)
{
    return entity.type == ENTITY_MONSTER &&
        (entity.flags & PERSISTENT_ENTITY_ACTIVE) != 0 &&
        entity.payload.monster.state == STATE_ALIVE;
}

bool persistentTreasureChestIsEmpty(const PersistentEntity& entity)
{
    return entity.type == ENTITY_CHEST &&
        (entity.flags & PERSISTENT_ENTITY_ACTIVE) != 0 &&
        entity.payload.chest.loot.generated &&
        entity.payload.chest.loot.itemCount == 0 &&
        entity.payload.chest.loot.gold == 0;
}

void updateRoomCompletion(Dungeon& dungeon, uint8_t roomIndex)
{
    if (roomIndex >= dungeon.roomCount)
        return;

    DungeonRoomRuntime& runtime = dungeon.roomRuntime[roomIndex];

    if (!runtime.initialized)
        return;

    const bool roomIsActive = dungeon.loadedRoom == roomIndex &&
        dungeon.entities == dungeon.activeDungeonEntities;
    bool hasLivingMonster = false;
    if (roomIsActive)
    {
        for (uint8_t i = 0; i < dungeon.entityCount; ++i)
        {
            const Entity& entity = dungeon.entities[i];
            if (entity.active && entity.type == ENTITY_MONSTER &&
                entity.character.team == TEAM_MONSTER &&
                entity.character.state == STATE_ALIVE)
            {
                hasLivingMonster = true;
                break;
            }
        }
    }
    else if (runtime.persistenceReady)
    {
        for (uint8_t i = 0; i < runtime.persistentEntityCount; ++i)
            if (persistentMonsterIsLiving(
                    runtime.entityStorage.compact.persistentEntities[i]))
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
    else if (isBellPuzzleRoom(dungeon.rooms[roomIndex]))
    {
        dungeon.rooms[roomIndex].completed =
            dungeon.rooms[roomIndex].bellPuzzle.progress == BELL_PUZZLE_COMPLETE;
    }
    else if (isNumberTilePuzzleRoom(dungeon.rooms[roomIndex]))
    {
        dungeon.rooms[roomIndex].completed =
            dungeon.rooms[roomIndex].numberPuzzle.progress ==
                NUMBER_PUZZLE_COMPLETE;
    }
    else
    {
        dungeon.rooms[roomIndex].completed = !hasLivingMonster;
    }

    if (roomIndex == dungeon.treasureRoom)
    {
        if (roomIsActive)
        {
            for (uint8_t i = 0; i < dungeon.entityCount; ++i)
            {
                const Entity& entity = dungeon.entities[i];
                if (entity.active && entity.type == ENTITY_CHEST &&
                    entity.loot.generated && entity.loot.itemCount == 0 &&
                    entity.loot.gold == 0)
                    dungeon.finalTreasureLooted = true;
            }
        }
        else if (runtime.persistenceReady)
            for (uint8_t i = 0; i < runtime.persistentEntityCount; ++i)
                if (persistentTreasureChestIsEmpty(
                        runtime.entityStorage.compact.persistentEntities[i]))
                    dungeon.finalTreasureLooted = true;
    }

    if (roomIndex == dungeon.bossRoom)
        dungeon.finalEncounterCleared = !hasLivingMonster;
}

bool packActiveEntities(
    const Dungeon& dungeon, PersistentEntity destination[MAX_ENTITIES],
    uint8_t& packedCount)
{
    packedCount = 0;
    for (uint8_t i = 0; i < dungeon.entityCount; ++i)
    {
        const Entity& source = dungeon.activeDungeonEntities[i];
        if (source.type == ENTITY_PLAYER)
            continue;
        if (source.type == ENTITY_NONE)
        {
            if (source.active) return false;
            continue;
        }
        if (packedCount >= MAX_ENTITIES ||
            !packPersistentEntity(source, destination[packedCount]))
            return false;
        ++packedCount;
    }
    return true;
}

bool validatePersistentRoom(const DungeonRoomRuntime& runtime)
{
    if (!runtime.persistenceReady ||
        runtime.persistentEntityCount > MAX_ENTITIES)
        return false;
    Entity probe{};
    for (uint8_t i = 0; i < runtime.persistentEntityCount; ++i)
        if (!inflatePersistentEntity(
                runtime.entityStorage.compact.persistentEntities[i], probe))
            return false;
    return true;
}

void copyActivePlayerToGlobal(Dungeon& dungeon)
{
    Entity* playerEntity = getPlayerEntity(
        dungeon.activeDungeonEntities, dungeon.entityCount);
    if (playerEntity != nullptr)
        player = playerEntity->character;
}

bool inflateRoomEntities(Dungeon& dungeon, const DungeonRoomRuntime& runtime)
{
    clearActiveDungeonEntities(dungeon);
    for (uint8_t i = 0; i < runtime.persistentEntityCount; ++i)
    {
        if (!inflatePersistentEntity(
                runtime.entityStorage.compact.persistentEntities[i],
                dungeon.activeDungeonEntities[i]))
        {
            clearActiveDungeonEntities(dungeon);
            return false;
        }
        ++dungeon.entityCount;
    }
    return true;
}

void initializeRoomEntities(
    Dungeon& dungeon,
    DungeonRoom& room,
    DungeonRoomRuntime& runtime)
{
    clearActiveDungeonEntities(dungeon);
    runtime.persistentEntityCount = 0;
    runtime.persistenceReady = false;
    uint8_t themedSpawnIndex = 0;

    // Marker tiles are consumed exactly once for this dungeon run. Subsequent
    // visits inflate the compact snapshot instead of regenerating occupants.
    for (int y = 0; y < ROOM_HEIGHT; y++)
    {
        for (int x = 0; x < ROOM_WIDTH; x++)
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

    runtime.initialized = true;
}

Entity* attachDungeonPlayer(
    Dungeon& dungeon,
    DungeonRoomRuntime&)
{
    if (dungeon.entityCount >= MAX_ENTITIES)
        return nullptr;
    Entity& playerEntity = dungeon.entities[dungeon.entityCount++];
    playerEntity = Entity{};
    playerEntity.active = true;
    playerEntity.type = ENTITY_PLAYER;
    playerEntity.character = player;
    playerEntity.sprite = getPlayerSprite(
        playerEntity.character.characterClass);

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
    int bestDistance = ROOM_WIDTH + ROOM_HEIGHT;
    bool found = false;

    for (uint8_t y = 0; y < ROOM_HEIGHT; y++)
    {
        for (uint8_t x = 0; x < ROOM_WIDTH; x++)
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
        if (!loadRoom(dungeon, ENTRY_START))
        {
            setGameMessage("Could not resume dungeon.");
            return;
        }
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
        room.puzzleType = roomIndex == dungeon.riddleRoom
            ? PUZZLE_RIDDLEMAN
            : PUZZLE_NONE;
        if (room.type == ROOM_PUZZLE && roomIndex != dungeon.riddleRoom)
        {
            const uint8_t puzzleRoll = random(100);
            if (puzzleRoll < BELL_ROOM_SELECTION_CHANCE_PERCENT)
                room.puzzleType = PUZZLE_BELLS;
            else if (puzzleRoll < BELL_ROOM_SELECTION_CHANCE_PERCENT +
                                      NUMBER_TILE_ROOM_SELECTION_CHANCE_PERCENT)
                room.puzzleType = PUZZLE_NUMBER_TILES;
        }
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
            : (dungeon.rooms[i].puzzleType == PUZZLE_RIDDLEMAN ||
               dungeon.rooms[i].puzzleType == PUZZLE_BELLS
               || dungeon.rooms[i].puzzleType == PUZZLE_NUMBER_TILES
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

        if (dungeon.rooms[i].puzzleType == PUZZLE_BELLS)
        {
            Direction lockedDirection = dungeon.rooms[i].connections[0].direction;
            const uint8_t currentDistance = getRoomDistanceFromEntrance(dungeon, i);
            for (uint8_t c = 0; c < dungeon.rooms[i].connectionCount; ++c)
            {
                const RoomConnection& connection = dungeon.rooms[i].connections[c];
                const uint8_t neighbor = getRoomNeighbor(dungeon.rooms[i], connection.direction);
                if (neighbor != NO_ROOM &&
                    getRoomDistanceFromEntrance(dungeon, neighbor) > currentDistance)
                {
                    lockedDirection = connection.direction;
                    break;
                }
            }
            uint8_t sequenceRolls[MAX_BELL_SEQUENCE * 8] = {};
            for (uint8_t& roll : sequenceRolls)
                roll = static_cast<uint8_t>(random(256));
            if (!configureBellPuzzleRoom(
                    dungeon.rooms[i], lockedDirection,
                    player.level > 0 ? player.level : 1,
                    sequenceRolls, sizeof(sequenceRolls)))
            {
                dungeon.rooms[i].puzzleType = PUZZLE_NONE;
            }
            else
            {
                continue;
            }
        }

        if (dungeon.rooms[i].puzzleType == PUZZLE_NUMBER_TILES)
        {
            Direction lockedDirection = dungeon.rooms[i].connections[0].direction;
            const uint8_t currentDistance = getRoomDistanceFromEntrance(dungeon, i);
            for (uint8_t c = 0; c < dungeon.rooms[i].connectionCount; ++c)
            {
                const RoomConnection& connection = dungeon.rooms[i].connections[c];
                const uint8_t neighbor = getRoomNeighbor(
                    dungeon.rooms[i], connection.direction);
                if (neighbor != NO_ROOM &&
                    getRoomDistanceFromEntrance(dungeon, neighbor) > currentDistance)
                { lockedDirection = connection.direction; break; }
            }
            uint8_t numberRolls[128] = {};
            for (uint8_t& roll : numberRolls)
                roll = static_cast<uint8_t>(random(256));
            if (!configureNumberTilePuzzleRoom(
                    dungeon.rooms[i], lockedDirection,
                    player.level > 0 ? player.level : 1,
                    numberRolls, sizeof(numberRolls)))
                dungeon.rooms[i].puzzleType = PUZZLE_NONE;
            else
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

bool persistActiveDungeonRoom(Dungeon& dungeon)
{
    if (dungeon.loadedRoom >= dungeon.roomCount ||
        dungeon.entities != dungeon.activeDungeonEntities)
        return false;

    DungeonRoomRuntime& runtime = dungeon.roomRuntime[dungeon.loadedRoom];
    PersistentEntity* scratch =
        runtime.entityStorage.compact.transactionScratch;
    uint8_t packedCount = 0;

    // Preflight before any combat/UI/player state changes. A representation
    // failure therefore leaves the active room byte-for-byte available.
    if (!packActiveEntities(dungeon, scratch, packedCount))
        return false;

    // Clear every known pointer into the shared buffer before it can be reused.
    abortCombat();
    closeInventoryMenu();
    clearInteractionEntityReferences();
    closeMenu();
    copyActivePlayerToGlobal(dungeon);
    resetActiveRoomTurnState(dungeon);

    // abortCombat() removes combat-local Flat-Footed and resets TurnState.
    // Repack that cleaned state before atomically committing the room snapshot.
    if (!packActiveEntities(dungeon, scratch, packedCount))
        return false;

    updateRoomCompletion(dungeon, dungeon.loadedRoom);
    for (uint8_t i = 0; i < packedCount; ++i)
        runtime.entityStorage.compact.persistentEntities[i] = scratch[i];
    for (uint8_t i = packedCount; i < MAX_ENTITIES; ++i)
        runtime.entityStorage.compact.persistentEntities[i] =
            PersistentEntity{};
    runtime.persistentEntityCount = packedCount;
    runtime.persistenceReady = true;
    return true;
}

bool loadRoom(Dungeon& dungeon, RoomEntry entry)
{
    if (dungeon.currentRoom >= dungeon.roomCount)
        return false;

    const uint8_t targetRoom = dungeon.currentRoom;
    const uint8_t previousRoom = dungeon.loadedRoom;
    DungeonRoomRuntime& targetRuntime = dungeon.roomRuntime[targetRoom];

    // Validate an existing destination before disturbing the current room.
    if (targetRoom != previousRoom && targetRuntime.initialized &&
        !validatePersistentRoom(targetRuntime))
    {
        if (previousRoom < dungeon.roomCount)
            dungeon.currentRoom = previousRoom;
        return false;
    }

    if (previousRoom < dungeon.roomCount &&
        dungeon.entities == dungeon.activeDungeonEntities &&
        !persistActiveDungeonRoom(dungeon))
    {
        dungeon.currentRoom = previousRoom;
        return false;
    }

    // A same-room reload just created its first compact snapshot above.
    if (targetRuntime.initialized && !validatePersistentRoom(targetRuntime))
    {
        dungeon.currentRoom = previousRoom;
        return false;
    }

    clearMapEffects();
    clearActiveDungeonEntities(dungeon);
    dungeon.entities = dungeon.activeDungeonEntities;
    dungeon.loadedRoom = NO_ROOM;

    DungeonRoom& room = dungeon.rooms[targetRoom];
    DungeonRoomRuntime& runtime = dungeon.roomRuntime[targetRoom];

    if (!runtime.initialized)
        initializeRoomEntities(dungeon, room, runtime);
    else if (!inflateRoomEntities(dungeon, runtime))
    {
        dungeon.entities = nullptr;
        dungeon.currentRoom = previousRoom;
        return false;
    }

    resetActiveRoomTurnState(dungeon);
    runtime.bellEnteredCount = 0;
    runtime.numberCrossingActive = false;
    runtime.numberCurrentStep = 0;
    runtime.numberPreviousDigit = room.numberPuzzle.seedA;
    runtime.numberCurrentDigit = room.numberPuzzle.rule ==
        NUMBER_RULE_PREVIOUS_PLUS_CURRENT_MOD_10
            ? room.numberPuzzle.seedB : room.numberPuzzle.seedA;

    uint8_t entryX = ROOM_WIDTH / 2;
    uint8_t entryY = ROOM_HEIGHT / 2;

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
            dungeon.entities = nullptr;
            dungeon.currentRoom = previousRoom;
            clearActiveDungeonEntities(dungeon);
            return false;
        }
    }

    Entity* playerEntity = attachDungeonPlayer(dungeon, runtime);

    if (playerEntity == nullptr)
    {
        dungeon.entities = nullptr;
        dungeon.currentRoom = previousRoom;
        clearActiveDungeonEntities(dungeon);
        return false;
    }

    playerEntity->x = entryX;
    playerEntity->y = entryY;

    room.discovered = true;
    dungeon.loadedRoom = targetRoom;
    resetAwarenessTimer();
    return true;
}

bool suspendDungeonRun(Dungeon& dungeon)
{
    if (!dungeon.runActive)
        return false;

    if (!persistActiveDungeonRoom(dungeon))
        return false;
    clearActiveDungeonEntities(dungeon);
    dungeon.entities = nullptr;
    dungeon.loadedRoom = NO_ROOM;
    return true;
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

    clearActiveDungeonEntities(dungeon);

    for (uint8_t roomIndex = 0; roomIndex < MAX_ROOMS; roomIndex++)
    {
        DungeonRoomRuntime& runtime = dungeon.roomRuntime[roomIndex];
        runtime.persistentEntityCount = 0;
        runtime.initialized = false;
        runtime.persistenceReady = false;
        runtime.bellEnteredCount = 0;
        runtime.numberCrossingActive = false;
        runtime.numberCurrentStep = 0;
        runtime.numberPreviousDigit = 0;
        runtime.numberCurrentDigit = 0;

        for (uint8_t entityIndex = 0;
             entityIndex < MAX_ENTITIES;
             entityIndex++)
        {
            runtime.entityStorage.compact.persistentEntities[entityIndex] =
                PersistentEntity{};
            runtime.entityStorage.compact.transactionScratch[entityIndex] =
                PersistentEntity{};
        }

        dungeon.rooms[roomIndex].discovered = false;
        dungeon.rooms[roomIndex].completed = false;
        dungeon.rooms[roomIndex].npcSpawn = DungeonNPCSpawn{};
        dungeon.rooms[roomIndex].puzzleType = PUZZLE_NONE;
        dungeon.rooms[roomIndex].bellPuzzle = BellPuzzleState{};
        dungeon.rooms[roomIndex].numberPuzzle = NumberTilePuzzleState{};
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

