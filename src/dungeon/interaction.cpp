//
// Created by james on 7/12/2026.
//

#include "interaction.h"

#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/activemap.h"
#include "dungeon/combat.h"
#include "dungeon/loot.h"
#include "graphics/messagelog.h"
#include "input/inventorymenu.h"

bool tryInteractWithFacingEntity()
{
    // Looting is deliberately a post-combat map interaction. It should not
    // bypass the existing combat action economy while a fight is active.
    if (combat.active)
        return false;

    Entity* playerEntity = getActiveMapPlayer();

    if (playerEntity == nullptr || !playerEntity->active)
        return false;

    int targetX = playerEntity->x + directionOffsets[moveDirection].dx;
    int targetY = playerEntity->y + directionOffsets[moveDirection].dy;

    if (!isInsideActiveMap(targetX, targetY))
        return false;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities == nullptr)
        return false;

    Entity* target = getEntityAt(
        entities,
        entityCount,
        static_cast<uint8_t>(targetX),
        static_cast<uint8_t>(targetY));

    if (target == nullptr || target->type != ENTITY_MONSTER ||
        !isLootable(target->character))
    {
        return false;
    }

    // The generator is idempotent. This also supports an older corpse that
    // entered STATE_DEAD before the loot system was added.
    generateCorpseLoot(*target);

    if (target->loot.itemCount == 0)
    {
        finishLootingCorpse(*target);
        setGameMessage("Nothing useful remains.");
        return true;
    }

    openCorpseLootMenu(*target);
    return true;
}
