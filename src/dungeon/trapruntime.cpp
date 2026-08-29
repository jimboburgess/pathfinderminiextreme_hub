#include "dungeon/traps.h"

#include <Arduino.h>
#include <cstdio>

#include "data/dice.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/abilityresolver.h"
#include "dungeon/combat.h"
#include "dungeon/dungeon.h"
#include "dungeon/roomgen.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "map/activemap.h"

namespace
{
constexpr uint8_t RANDOM_TRAP_PERCENT = 25;
// Atmospheric clues deliberately outnumber real hazards. This preserves the
// uncertainty of dungeon dressing: cracks, blood, bones, and dust are often
// just remnants of an old expedition, not proof of a live trap.
constexpr uint8_t RED_HERRING_PERCENT = 78;
constexpr uint8_t EXTRA_RED_HERRING_PERCENT = 58;
constexpr uint8_t THIRD_RED_HERRING_PERCENT = 24;
constexpr int ROGUE_TRAPFINDING_RANGE = 3;
constexpr int ENTRY_CLEARANCE = 2;
constexpr int CONTENT_CLEARANCE = 1;

constexpr SuspicionType SPIKE_CLUES[] =
{
    SUSPICION_RAISED_TILE,
    SUSPICION_CRACKED_FLOOR,
    SUSPICION_FLOOR_GROOVES,
    SUSPICION_BLOODSTAIN,
    SUSPICION_BONES,
    SUSPICION_DISTURBED_DUST
};

constexpr SuspicionType DRESSING_CLUES[] =
{
    SUSPICION_CRACKED_FLOOR,
    SUSPICION_BLOODSTAIN,
    SUSPICION_BONES,
    SUSPICION_DISTURBED_DUST
};

struct FeatureCandidate
{
    uint8_t x;
    uint8_t y;
};

SuspicionType randomSpikeClue()
{
    constexpr uint8_t clueCount =
        sizeof(SPIKE_CLUES) / sizeof(SPIKE_CLUES[0]);
    return SPIKE_CLUES[static_cast<uint8_t>(random(clueCount))];
}

SuspicionType randomDressingClue()
{
    constexpr uint8_t clueCount =
        sizeof(DRESSING_CLUES) / sizeof(DRESSING_CLUES[0]);
    return DRESSING_CLUES[static_cast<uint8_t>(random(clueCount))];
}

int coordinateDistance(int first, int second)
{
    return first > second ? first - second : second - first;
}

int gridDistance(int firstX, int firstY, int secondX, int secondY)
{
    const int dx = coordinateDistance(firstX, secondX);
    const int dy = coordinateDistance(firstY, secondY);
    return dx > dy ? dx : dy;
}

bool isContentMarker(TileType tile)
{
    switch (tile)
    {
        case TILE_CHEST_SPAWN:
        case TILE_LOOT_SPAWN:
        case TILE_NPC_SPAWN:
        case TILE_EXIT:
        case TILE_PLAYER_START:
        case TILE_ENEMY_START:
        case TILE_GIANT_SPIDER_START:
        case TILE_SKELETON_MAGE_START:
        case TILE_SKELETON_START:
            return true;

        default:
            return false;
    }
}

bool isNearRoomEntry(const DungeonRoom& room, int x, int y)
{
    static constexpr RoomEntry entries[] =
    {
        ENTRY_START,
        ENTRY_NORTH,
        ENTRY_EAST,
        ENTRY_SOUTH,
        ENTRY_WEST
    };

    for (RoomEntry entry : entries)
    {
        uint8_t entryX = 0;
        uint8_t entryY = 0;
        if (getRoomEntryPosition(room, entry, entryX, entryY) &&
            gridDistance(x, y, entryX, entryY) <= ENTRY_CLEARANCE)
        {
            return true;
        }
    }

    uint8_t connectionCount = room.connectionCount;
    if (connectionCount > MAX_ROOM_CONNECTIONS)
        connectionCount = MAX_ROOM_CONNECTIONS;

    for (uint8_t i = 0; i < connectionCount; i++)
    {
        const RoomConnection& connection = room.connections[i];
        if (gridDistance(x, y, connection.x, connection.y) <=
            ENTRY_CLEARANCE)
        {
            return true;
        }
    }

    return false;
}

bool isNearContent(const DungeonRoom& room, int x, int y)
{
    for (int contentY = 0; contentY < ROOM_SIZE; contentY++)
    {
        for (int contentX = 0; contentX < ROOM_SIZE; contentX++)
        {
            if (isContentMarker(room.map.tiles[contentY][contentX]) &&
                gridDistance(x, y, contentX, contentY) <=
                    CONTENT_CLEARANCE)
            {
                return true;
            }
        }
    }

    return false;
}

bool remainsBypassable(DungeonRoom& room, int x, int y)
{
    const TileType original = room.map.tiles[y][x];
    room.map.tiles[y][x] = TILE_WALL;
    const bool connected = validateRoomConnectivity(room);
    room.map.tiles[y][x] = original;
    return connected;
}

bool isFeatureCandidate(DungeonRoom& room, int x, int y)
{
    return x > 0 && y > 0 && x < ROOM_SIZE - 1 && y < ROOM_SIZE - 1 &&
           room.map.tiles[y][x] == TILE_FLOOR &&
           getTrapAt(room, x, y) == nullptr &&
           getSuspicionAt(room, x, y) == SUSPICION_NONE &&
           !isNearRoomEntry(room, x, y) &&
           !isNearContent(room, x, y) &&
           remainsBypassable(room, x, y);
}

bool chooseFeatureCandidate(DungeonRoom& room, uint8_t& x, uint8_t& y)
{
    FeatureCandidate candidates[ROOM_SIZE * ROOM_SIZE];
    uint16_t candidateCount = 0;

    for (int candidateY = 1; candidateY < ROOM_SIZE - 1; candidateY++)
    {
        for (int candidateX = 1; candidateX < ROOM_SIZE - 1; candidateX++)
        {
            if (!isFeatureCandidate(room, candidateX, candidateY))
                continue;

            candidates[candidateCount++] =
            {
                static_cast<uint8_t>(candidateX),
                static_cast<uint8_t>(candidateY)
            };
        }
    }

    if (candidateCount == 0)
        return false;

    const FeatureCandidate& selected = candidates[
        static_cast<uint16_t>(random(candidateCount))];
    x = selected.x;
    y = selected.y;
    return true;
}

bool hasSpikeTrap(const DungeonRoom& room)
{
    for (const TrapInstance& trap : room.traps)
    {
        if (trap.id == TRAP_SPIKE_PLATE)
            return true;
    }

    return false;
}

bool consumeAuthoredTrapMarkers(
    DungeonRoom& room,
    uint8_t challengeLevel)
{
    bool added = false;

    for (int y = 0; y < ROOM_SIZE; y++)
    {
        for (int x = 0; x < ROOM_SIZE; x++)
        {
            if (room.map.tiles[y][x] != TILE_TRAP)
                continue;

            // TILE_TRAP is generation metadata. Runtime traps occupy normal
            // walkable floor and live in the room's persistent feature data.
            room.map.tiles[y][x] = TILE_FLOOR;

            if (getTrapAt(room, x, y) == nullptr &&
                addTrap(
                    room,
                    TRAP_SPIKE_PLATE,
                    x,
                    y,
                    randomTrapLevel(challengeLevel),
                    randomSpikeClue()))
            {
                added = true;
            }
        }
    }

    return added;
}

bool footprintContainsTile(
    const Entity& entity,
    int entityX,
    int entityY,
    int tileX,
    int tileY)
{
    return tileX >= entityX &&
           tileX < entityX + getEntityTileWidth(entity) &&
           tileY >= entityY &&
           tileY < entityY + getEntityTileHeight(entity);
}

void presentTrapTrigger(
    const Entity& entity,
    const TrapTriggerResult& result)
{
    char message[112];

    if (entity.type == ENTITY_PLAYER)
    {
        if (result.saveSucceeded)
        {
            snprintf(
                message,
                sizeof(message),
                "Spikes erupt! Reflex succeeds; no damage.");
        }
        else
        {
            snprintf(
                message,
                sizeof(message),
                "Spikes erupt! Reflex fails; %u damage.",
                static_cast<unsigned int>(result.damage));
        }
    }
    else if (result.saveSucceeded)
    {
        snprintf(
            message,
            sizeof(message),
            "%s triggers spikes! Reflex succeeds; no damage.",
            getEntityName(&entity));
    }
    else
    {
        snprintf(
            message,
            sizeof(message),
            "%s triggers spikes! Reflex fails; %u damage.",
            getEntityName(&entity),
            static_cast<unsigned int>(result.damage));
    }

    setGameMessage(message);
}
}

uint8_t randomTrapLevel(uint8_t challengeLevel)
{
    return selectTrapLevel(
        challengeLevel,
        static_cast<uint8_t>(random(100)));
}

void populateDungeonRoomFeatures(
    DungeonRoom& room,
    uint8_t challengeLevel,
    bool allowTrap)
{
    const bool authoredTrapAdded =
        consumeAuthoredTrapMarkers(room, challengeLevel);

    if (allowTrap && !authoredTrapAdded && !hasSpikeTrap(room) &&
        random(100) < RANDOM_TRAP_PERCENT)
    {
        uint8_t x = 0;
        uint8_t y = 0;
        if (chooseFeatureCandidate(room, x, y))
        {
            addTrap(
                room,
                TRAP_SPIKE_PLATE,
                x,
                y,
                randomTrapLevel(challengeLevel),
                randomSpikeClue());
        }
    }

    // Harmless clues are common independent dressing. A clue may share a
    // room with a real trap, but never shares its square or implies discovery.
    if (random(100) < RED_HERRING_PERCENT)
    {
        uint8_t clueCount = 1;
        if (random(100) < EXTRA_RED_HERRING_PERCENT)
            clueCount++;
        if (random(100) < THIRD_RED_HERRING_PERCENT)
            clueCount++;

        for (uint8_t clueIndex = 0; clueIndex < clueCount; clueIndex++)
        {
            uint8_t x = 0;
            uint8_t y = 0;
            if (!chooseFeatureCandidate(room, x, y))
                break;
            addSuspicion(room, randomDressingClue(), x, y);
        }
    }
}

void updateRogueTrapAwareness(Entity& playerEntity)
{
    if (gameState != GAME_DUNGEON || !dungeon.runActive ||
        dungeon.currentRoom >= MAX_ROOMS || !playerEntity.active ||
        playerEntity.type != ENTITY_PLAYER ||
        playerEntity.character.state != STATE_ALIVE ||
        playerEntity.character.characterClass != CLASS_ROGUE)
    {
        return;
    }

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    bool discoveredAny = false;

    for (TrapInstance& trap : room.traps)
    {
        if (trap.id == TRAP_NONE || trap.discovered ||
            trap.rogueDiscoveryAttempted ||
            getEntityGridDistanceToTile(playerEntity, trap.x, trap.y) >
                ROGUE_TRAPFINDING_RANGE ||
            !hasLineOfSightFromFootprintAt(
                playerEntity,
                playerEntity.x,
                playerEntity.y,
                trap.x,
                trap.y))
        {
            continue;
        }

        const int perceptionTotal = rollDie(20) + getSkillBonus(
            playerEntity.character, SKILL_PERCEPTION);
        const TrapDiscoveryResult discovery =
            attemptRogueTrapDiscovery(trap, perceptionTotal);

        if (discovery != TRAP_DISCOVERY_SUCCESS)
            continue;

        discoveredAny = true;
        markTileDirty(trap.x, trap.y);
    }

    if (discoveredAny)
        setGameMessage("You discover a pressure plate!");
}

bool triggerTrapForEntityAt(
    Entity& entity,
    int targetX,
    int targetY)
{
    TrapTriggerResult result;

    if (gameState != GAME_DUNGEON || !dungeon.runActive ||
        dungeon.currentRoom >= MAX_ROOMS || !entity.active ||
        entity.character.state != STATE_ALIVE)
    {
        return false;
    }

    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];

    for (TrapInstance& trap : room.traps)
    {
        if (!isTrapActive(trap) ||
            !footprintContainsTile(
                entity, targetX, targetY, trap.x, trap.y))
        {
            continue;
        }

        const TrapDefinition* definition = getTrapDefinition(trap.id);
        if (definition == nullptr)
            return false;

        const AbilitySavingThrow savingThrow = resolveSavingThrow(
            entity.character,
            definition->saveType,
            getTrapSaveDC(trap));
        const bool saveSucceeded =
            savingThrow.result == SAVE_RESULT_SUCCESS;
        const int rolledDamage = rollDice(
            getTrapDamageDice(trap),
            getTrapDamageSides(trap));

        result = resolveTrapTrigger(
            trap, saveSucceeded, rolledDamage);
        if (!result.triggered)
            return false;

        if (result.damage > 0)
            applyEnvironmentalDamage(entity, result.damage);

        markTileDirty(trap.x, trap.y);
        presentTrapTrigger(entity, result);
        return true;
    }

    return false;
}
