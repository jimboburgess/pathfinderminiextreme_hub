#include "dungeon/npcs.h"

#include "data/entities.h"
#include "dungeon/dungeon.h"
#include "dungeon/fountain.h"
#include "dungeon/furniture.h"
#include "dungeon/traps.h"
#include "graphics/messagelog.h"
#include "graphics/npcsprites.h"
#include "input/riddlemenu.h"

namespace
{
const NPCDefinition NPC_DEFINITIONS[] =
{
    {
        NPC_BERTRAM_RIDDLEMAN,
        "Bertram, Door Enthusiast",
        TEAM_NEUTRAL,
        bertram16x16,
        "Bertram watches you expectantly."
    }
};
}

const NPCDefinition* getNPCDefinition(NPCID id)
{
    for (const NPCDefinition& definition : NPC_DEFINITIONS)
        if (definition.id == id)
            return &definition;

    return nullptr;
}

bool placeDungeonNPC(DungeonRoom& room, NPCID id, int x, int y)
{
    if (getNPCDefinition(id) == nullptr || room.npcSpawn.id != NPC_NONE ||
        x < 0 || x >= ROOM_SIZE || y < 0 || y >= ROOM_SIZE ||
        room.map.tiles[y][x] != TILE_FLOOR ||
        getTrapAt(room, x, y) != nullptr ||
        getDungeonFurnitureAt(room, x, y) != nullptr ||
        isHealingFountainTile(room, x, y))
    {
        return false;
    }

    room.npcSpawn.id = id;
    room.npcSpawn.x = static_cast<int8_t>(x);
    room.npcSpawn.y = static_cast<int8_t>(y);
    return true;
}

bool isBlockingNeutralNPC(const Entity& entity)
{
    return entity.active && entity.type == ENTITY_NPC &&
        entity.character.team == TEAM_NEUTRAL;
}

bool isBertramRiddleman(const Entity& entity)
{
    return entity.active && entity.type == ENTITY_NPC &&
        entity.npcID == NPC_BERTRAM_RIDDLEMAN;
}

bool handleNPCInteraction(const Entity& npc)
{
    if (!npc.active || npc.type != ENTITY_NPC)
        return false;

    const NPCDefinition* definition = getNPCDefinition(npc.npcID);
    if (definition == nullptr)
        return false;

    if (npc.npcID == NPC_BERTRAM_RIDDLEMAN)
        return openBertramRiddle(npc);

    setGameMessage(definition->interactionMessage);
    return true;
}
