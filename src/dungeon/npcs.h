#ifndef PATHFINDERMINIEXTREME_025_NPCS_H
#define PATHFINDERMINIEXTREME_025_NPCS_H

#include <stdint.h>

#include "characters/characters.h"
#include "data/game.h"
#include "dungeon/riddles.h"

struct DungeonRoom;
struct Entity;

enum NPCID : uint8_t
{
    NPC_NONE,
    NPC_BERTRAM_RIDDLEMAN
};

struct NPCDefinition
{
    NPCID id;
    const char* name;
    Team team;
    const uint16_t* sprite;
    const char* interactionMessage;
};

enum RiddleRoomState : uint8_t
{
    RIDDLE_ROOM_NONE,
    RIDDLE_ROOM_UNSOLVED,
    RIDDLE_ROOM_KEY_PRESENTED,
    RIDDLE_ROOM_KEY_COLLECTED,
    RIDDLE_ROOM_COMPLETE
};

enum RiddleRoomFlag : uint8_t
{
    RIDDLE_FLAG_NONE = 0,
    RIDDLE_FLAG_BYPASS_REACTION_SHOWN = 1 << 0,
    RIDDLE_FLAG_ATTACK_REFUSED = 1 << 1,
    RIDDLE_FLAG_RETRY_REQUIRED = 1 << 2,
    RIDDLE_FLAG_CAT_CHASE_ACTIVE = 1 << 3,
    RIDDLE_FLAG_CAT_JUST_CAUGHT = 1 << 4
};

// A generated-room blueprint. Once the room is first loaded, the NPC becomes
// a normal persistent Entity in DungeonRoomRuntime.
struct DungeonNPCSpawn
{
    NPCID id = NPC_NONE;
    int8_t x = -1;
    int8_t y = -1;
    RiddleState riddle;
    RiddleRoomState puzzleState = RIDDLE_ROOM_NONE;
    uint8_t lockedExitDirection = DIR_NORTH;
    int8_t keyX = -1;
    int8_t keyY = -1;
    // Room-owned one-bit dialogue history. This persists with the generated
    // room without enlarging every runtime Entity.
    uint8_t riddleFlags = RIDDLE_FLAG_NONE;
    uint8_t riddleAttemptsMade = 0;
    uint8_t catForcedEscapeThreshold = 0;
    uint8_t catCatchAttempts = 0;
};

static_assert(sizeof(DungeonNPCSpawn) == 18,
              "DungeonNPCSpawn puzzle state must remain compact");

const NPCDefinition* getNPCDefinition(NPCID id);

// Adds the room's single stage-one NPC spawn before its runtime is initialized.
// The helper validates terrain, doors, traps, furniture, and the fountain.
bool placeDungeonNPC(
    DungeonRoom& room, NPCID id, int x, int y);

bool isBlockingNeutralNPC(const Entity& entity);
bool isBertramRiddleman(const Entity& entity);

// Routes the NPC's current interaction stub. Returns false for an invalid or
// inactive NPC and never starts combat or modifies character resources.
bool handleNPCInteraction(const Entity& npc);

#endif
