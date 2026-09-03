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
#include "graphics/elementalvisual.h"
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
constexpr unsigned long ELEMENTAL_TRAP_WARNING_MS = 6000UL;

constexpr SuspicionType SPIKE_CLUES[] =
{
    SUSPICION_RAISED_TILE,
    SUSPICION_CRACKED_FLOOR,
    SUSPICION_FLOOR_GROOVES,
    SUSPICION_BLOODSTAIN,
    SUSPICION_BONES,
    SUSPICION_DISTURBED_DUST
};
constexpr SuspicionType ARROW_CLUES[] =
{
    SUSPICION_WALL_HOLE, SUSPICION_SMALL_HOLES, SUSPICION_FLOOR_GROOVES,
    SUSPICION_WIRE_OR_STRING, SUSPICION_SCRATCHES
};
constexpr SuspicionType DART_CLUES[] =
{
    SUSPICION_SMALL_HOLES, SUSPICION_WALL_HOLE, SUSPICION_BLOODSTAIN,
    SUSPICION_BONES, SUSPICION_DISCOLORED_TILE
};
constexpr SuspicionType GREASE_CLUES[] =
{
    SUSPICION_DISCOLORED_TILE,
    SUSPICION_DISTURBED_DUST,
    SUSPICION_FLOOR_GROOVES
};
constexpr SuspicionType WEB_CLUES[] =
{
    SUSPICION_SMALL_HOLES,
    SUSPICION_WALL_HOLE,
    SUSPICION_DISTURBED_DUST,
    SUSPICION_BONES
};
constexpr SuspicionType SLEEP_CLUES[] =
{
    SUSPICION_DISTURBED_DUST,
    SUSPICION_BONES,
    SUSPICION_DISCOLORED_TILE
};
constexpr SuspicionType COLOR_SPRAY_CLUES[] =
{
    SUSPICION_DISCOLORED_TILE,
    SUSPICION_SCORCH_MARK,
    SUSPICION_OZONE_MARK
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

SuspicionType randomProjectileClue(TrapID id)
{
    const SuspicionType* clues = id == TRAP_POISON_DART ? DART_CLUES : ARROW_CLUES;
    const uint8_t count = id == TRAP_POISON_DART
        ? sizeof(DART_CLUES) / sizeof(DART_CLUES[0])
        : sizeof(ARROW_CLUES) / sizeof(ARROW_CLUES[0]);
    return clues[static_cast<uint8_t>(random(count))];
}

SuspicionType randomElementalClue(TrapID id)
{
    static const SuspicionType fire[] = {SUSPICION_SCORCH_MARK, SUSPICION_DISCOLORED_TILE, SUSPICION_SMALL_HOLES, SUSPICION_BONES};
    static const SuspicionType frost[] = {SUSPICION_FROST, SUSPICION_DISCOLORED_TILE, SUSPICION_CRACKED_FLOOR};
    static const SuspicionType electric[] = {SUSPICION_OZONE_MARK, SUSPICION_SCORCH_MARK, SUSPICION_WIRE_OR_STRING, SUSPICION_SMALL_HOLES};
    static const SuspicionType acid[] = {SUSPICION_CORROSION, SUSPICION_DISCOLORED_TILE, SUSPICION_FLOOR_GROOVES, SUSPICION_BONES};
    const SuspicionType* clues = fire;
    uint8_t count = sizeof(fire) / sizeof(fire[0]);
    if (id == TRAP_FROST) { clues = frost; count = sizeof(frost) / sizeof(frost[0]); }
    else if (id == TRAP_ELECTRIC) { clues = electric; count = sizeof(electric) / sizeof(electric[0]); }
    else if (id == TRAP_ACID) { clues = acid; count = sizeof(acid) / sizeof(acid[0]); }
    return clues[random(count)];
}

SuspicionType randomSpellClue(TrapID id)
{
    const SuspicionType* clues = GREASE_CLUES;
    uint8_t count = sizeof(GREASE_CLUES) / sizeof(GREASE_CLUES[0]);
    if (id == TRAP_WEB)
    {
        clues = WEB_CLUES;
        count = sizeof(WEB_CLUES) / sizeof(WEB_CLUES[0]);
    }
    else if (id == TRAP_SLEEP)
    {
        clues = SLEEP_CLUES;
        count = sizeof(SLEEP_CLUES) / sizeof(SLEEP_CLUES[0]);
    }
    else if (id == TRAP_COLOR_SPRAY)
    {
        clues = COLOR_SPRAY_CLUES;
        count = sizeof(COLOR_SPRAY_CLUES) / sizeof(COLOR_SPRAY_CLUES[0]);
    }
    return clues[static_cast<uint8_t>(random(count))];
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

Entity* traceProjectileTarget(const DungeonRoom& room, const TrapInstance& trap)
{
    if (!isProjectileTrap(trap) || trap.sourceX < 0 || trap.sourceY < 0)
        return nullptr;
    const DirectionOffset offset = directionOffsets[trap.direction];
    int x = trap.sourceX + offset.dx;
    int y = trap.sourceY + offset.dy;
    while (x >= 0 && x < ROOM_SIZE && y >= 0 && y < ROOM_SIZE)
    {
        if (isTileBlockingSight(room.map.tiles[y][x]))
            return nullptr;
        for (uint8_t i = 0; i < dungeon.entityCount; i++)
        {
            Entity& candidate = dungeon.entities[i];
            if (candidate.active && candidate.character.state == STATE_ALIVE &&
                footprintContainsTile(candidate, candidate.x, candidate.y, x, y))
                return &candidate;
        }
        // Barrels/crates/braziers are not LOS blockers but stop a physical dart.
        if (!isDungeonFloorTerrain(room.map.tiles[y][x]))
            return nullptr;
        x += offset.dx;
        y += offset.dy;
    }
    return nullptr;
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

const char* elementalWarningMessage(TrapID id)
{
    switch (id) {
        case TRAP_FIRE: return "The floor begins to glow red-hot!";
        case TRAP_FROST: return "Frost rapidly spreads across the floor!";
        case TRAP_ELECTRIC: return "The air crackles with electricity!";
        case TRAP_ACID: return "Green liquid begins bubbling through the cracks!";
        default: return "The trap begins to charge!";
    }
}

const char* elementalResolutionMessage(TrapID id)
{
    switch (id) {
        case TRAP_FIRE: return "Flames erupt!";
        case TRAP_FROST: return "A wave of freezing cold erupts!";
        case TRAP_ELECTRIC: return "Electricity arcs across the floor!";
        case TRAP_ACID: return "Acid sprays across the area!";
        default: return "The trap discharges!";
    }
}

bool configureRandomSourcedTrap(DungeonRoom& room, TrapInstance& trap)
{
    static const Direction directions[] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
    for (Direction direction : directions)
    {
        const DirectionOffset offset = directionOffsets[direction];
        const int sourceX = trap.x - offset.dx * 2;
        const int sourceY = trap.y - offset.dy * 2;
        const int laneX = trap.x - offset.dx;
        const int laneY = trap.y - offset.dy;
        if (sourceX <= 0 || sourceX >= ROOM_SIZE - 1 || sourceY <= 0 || sourceY >= ROOM_SIZE - 1 ||
            laneX < 0 || laneX >= ROOM_SIZE || laneY < 0 || laneY >= ROOM_SIZE ||
            room.map.tiles[sourceY][sourceX] != TILE_WALL ||
            !isDungeonFloorTerrain(room.map.tiles[laneY][laneX]))
            continue;
        return isProjectileTrap(trap)
            ? configureProjectileTrap(trap, sourceX, sourceY, direction)
            : configureDirectionalTrap(trap, sourceX, sourceY, direction);
    }
    return false;
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
            const TrapID trapID = selectGeneratedTrapID(
                challengeLevel,
                static_cast<uint8_t>(random(100)),
                static_cast<uint8_t>(random(100)));
            if (!addTrap(
                room,
                trapID,
                x,
                y,
                randomTrapLevel(challengeLevel),
                trapID == TRAP_SPIKE_PLATE ? randomSpikeClue() :
                    ((trapID == TRAP_ARROW || trapID == TRAP_POISON_DART)
                        ? randomProjectileClue(trapID)
                        : (getTrapDefinition(trapID)->abilityId != ABILITY_NONE
                            ? randomSpellClue(trapID)
                            : randomElementalClue(trapID)))))
            {
                // Leave the room otherwise unchanged; harmless dressing may
                // still be added below.
            }
            else
            {
                TrapInstance* trap = getTrapAt(room, x, y);
                if (trap != nullptr &&
                    (isProjectileTrap(*trap) || trap->id == TRAP_COLOR_SPRAY) &&
                    !configureRandomSourcedTrap(room, *trap))
                {
                    *trap = TrapInstance{};
                }
            }
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

void updateElementalTrapCharges()
{
    if (gameState != GAME_DUNGEON || !dungeon.runActive ||
        dungeon.currentRoom >= MAX_ROOMS)
        return;
    DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    for (TrapInstance& trap : room.traps)
    {
        if (!trap.charging || !isElementalTrap(trap))
            continue;
        const bool due = combat.active
            ? combat.combatRound > trap.chargingCombatRound
            : static_cast<long>(millis() - trap.chargingUntilMillis) >= 0;
        if (!due)
            continue;

        const TrapDefinition* definition = getTrapDefinition(trap.id);
        trap.charging = false;
        trap.triggered = true;
        trap.discovered = true;
        const AreaFlashTile flashTile = {trap.x, trap.y};
        playAreaDamageFlash(definition->damageType, &flashTile, 1);
        ElementalVisualTile visualTile;
        visualTile.x = trap.x;
        visualTile.y = trap.y;
        startElementalVisualEffect(definition->damageType, &visualTile, 1);
        for (uint8_t i = 0; i < dungeon.entityCount; i++)
        {
            Entity& target = dungeon.entities[i];
            if (!target.active || target.character.state != STATE_ALIVE ||
                !footprintContainsTile(target, target.x, target.y, trap.x, trap.y))
                continue;
            const AbilitySavingThrow save = resolveSavingThrow(
                target.character, SAVE_REFLEX, getTrapSaveDC(trap));
            int damage = rollDice(getTrapDamageDice(trap), getTrapDamageSides(trap));
            if (save.result == SAVE_RESULT_SUCCESS)
                damage /= 2;
            applyCombatDamage(target, damage, definition->damageType);
        }
        markTileDirty(trap.x, trap.y);
        setGameMessage(elementalResolutionMessage(trap.id));
    }
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

        if (isElementalTrap(trap))
        {
            trap.charging = true;
            trap.discovered = true;
            trap.chargingCombatRound = combat.active ? combat.combatRound : 0;
            trap.chargingUntilMillis = millis() + ELEMENTAL_TRAP_WARNING_MS;
            markTileDirty(trap.x, trap.y);
            setGameMessage(elementalWarningMessage(trap.id));
            return true;
        }

        if (isSpellTrap(trap))
        {
            EnvironmentalAbilityContext context;
            if (!buildEnvironmentalAbilityContext(trap, context))
                return false;

            resolveEnvironmentalAbility(context, definition->abilityId);
            trap.triggered = true;
            trap.discovered = true;
            markTileDirty(trap.x, trap.y);

            switch (trap.id)
            {
                case TRAP_GREASE:
                    setGameMessage("Grease spreads across the floor!");
                    break;
                case TRAP_WEB:
                    setGameMessage("Webs burst across the area!");
                    break;
                case TRAP_SLEEP:
                    setGameMessage("Drowsy magic fills the air!");
                    break;
                case TRAP_COLOR_SPRAY:
                    setGameMessage("Brilliant colors spray from the wall!");
                    break;
                default:
                    break;
            }
            return true;
        }

        Entity* target = &entity;
        if (isProjectileTrap(trap))
        {
            target = traceProjectileTarget(room, trap);
            trap.triggered = true;
            trap.discovered = true;
            markTileDirty(trap.x, trap.y);
            if (target == nullptr)
            {
                setGameMessage("A hidden mechanism clicks. It hits nothing.");
                return true;
            }
        }

        const AbilitySavingThrow savingThrow = resolveSavingThrow(
            target->character,
            definition->saveType,
            getTrapSaveDC(trap));
        const bool saveSucceeded =
            savingThrow.result == SAVE_RESULT_SUCCESS;
        const int rolledDamage = rollDice(
            getTrapDamageDice(trap),
            getTrapDamageSides(trap));

        result = resolveTrapTrigger(trap, saveSucceeded, rolledDamage);
        if (!result.triggered)
            return false;

        if (result.damage > 0)
            applyCombatDamage(*target, result.damage, definition->damageType);

        bool slowingVenomApplied = false;
        if (trap.id == TRAP_POISON_DART && result.damage > 0)
        {
            const AbilitySavingThrow poisonSave = resolveSavingThrow(
                target->character, SAVE_FORTITUDE, getTrapSaveDC(trap));
            slowingVenomApplied = poisonSave.result != SAVE_RESULT_SUCCESS &&
                applySlowingVenom(target->character, getTrapSaveDC(trap));
        }

        markTileDirty(trap.x, trap.y);
        if (isProjectileTrap(trap))
        {
            if (target->type == ENTITY_PLAYER)
                setGameMessage(slowingVenomApplied
                    ? "Your leg is beginning to feel numb."
                    : (trap.id == TRAP_POISON_DART && result.damage > 0
                        ? "Something strikes your leg. 1 damage."
                        : "An arrow shoots from the wall!"));
            else
                presentTrapTrigger(*target, result);
        }
        else
            presentTrapTrigger(entity, result);
        return true;
    }

    return false;
}
