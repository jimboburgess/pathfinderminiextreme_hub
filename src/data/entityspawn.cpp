//
// Created by james on 7/25/2026.
//

#include "entityspawn.h"
#include "../dungeon/monsters.h"

Entity* findFreeEntity(
    Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            return &entities[i];
    }

    return nullptr;
}

Entity* spawnEntity(
    Entity entities[],
    uint8_t& entityCount,
    EntityType type,
    uint8_t x,
    uint8_t y)
{
    Entity* entity = nullptr;

    entity = findFreeEntity(entities, entityCount);

    if (entity == nullptr)
    {
        if (entityCount >= MAX_ENTITIES)
            return nullptr;

        entity = &entities[entityCount++];
    }

    // Entity slots are reused. Reset all lifecycle, character, monster and
    // corpse-loot state so a new spawn cannot inherit data from an old one.
    *entity = Entity{};

    entity->active = true;

    entity->type = type;

    entity->x = x;
    entity->y = y;

    entity->monsterID = MONSTER_NONE;

    entity->sprite = nullptr;
    entity->spriteWidth = SPRITE_W;
    entity->spriteHeight = SPRITE_H;

    return entity;
}

Entity* spawnMonster(
    Entity* entities,
    uint8_t& entityCount,
    MonsterID monsterID,
    uint8_t x,
    uint8_t y)
{
    const Monster* monster = getMonster(monsterID);

    // Invalid IDs and zero-hit-die definitions cannot produce a valid living
    // monster. Reject them before consuming or reusing an entity slot.
    if (monster == nullptr || monster->hitDice == 0)
        return nullptr;

    Entity* entity = spawnEntity(
        entities,
        entityCount,
        ENTITY_MONSTER,
        x,
        y);

    if (entity == nullptr)
        return nullptr;

    entity->monsterID = monsterID;
    entity->monster = monster;

    entity->sprite = monster->sprite;

    if (monsterID == MONSTER_GIANT_SPIDER)
    {
        entity->spriteWidth = LRGSPRITE_W;
        entity->spriteHeight = LRGSPRITE_H;
    }

    entity->character.team = TEAM_MONSTER;
    entity->character.state = STATE_ALIVE;

    entity->character.abilities = monster->abilities;
    entity->character.speed = monster->speed;
    entity->character.level = monster->casterLevel;
    entity->character.magic.maxMP = monster->maxMP;
    entity->character.magic.currentMP = monster->maxMP;

    // Roll the definition's intended hit-die calculation exactly once, then
    // explicitly initialize both runtime health fields from that result.
    const uint16_t maxHP = getMonsterMaxHP(*monster);
    entity->character.health.maxHP = maxHP;
    entity->character.health.currentHP = entity->character.health.maxHP;

    // Put the creature's weapon in the matching slot.  Ranged monster
    // scripts can then deliberately choose their ranged weapon instead of
    // treating a bow as a melee attack.
    entity->character.equipment.equipped[SLOT_MELEE_WEAPON] =
        makeItemInstance(ITEM_NONE);
    entity->character.equipment.equipped[SLOT_RANGED_WEAPON] =
        makeItemInstance(ITEM_NONE);

    const Weapon* weapon = getWeapon(monster->weapon);

    if (weapon != nullptr && weapon->type == WEAPON_RANGED)
    {
        entity->character.equipment.equipped[SLOT_RANGED_WEAPON] =
            makeItemInstance(monster->weapon);
    }
    else
    {
        entity->character.equipment.equipped[SLOT_MELEE_WEAPON] =
            makeItemInstance(monster->weapon);
    }

    entity->character.equipment.equipped[SLOT_ARMOR] =
        makeItemInstance(monster->armor);

    return entity;
}


void removeEntity(Entity& entity)
{
    entity.active = false;
}

uint8_t getEntityTileWidth(const Entity& entity)
{
    uint8_t tileWidth =
        (entity.spriteWidth + SPRITE_W - 1) / SPRITE_W;

    return tileWidth > 0 ? tileWidth : 1;
}

uint8_t getEntityTileHeight(const Entity& entity)
{
    uint8_t tileHeight =
        (entity.spriteHeight + SPRITE_H - 1) / SPRITE_H;

    return tileHeight > 0 ? tileHeight : 1;
}

bool entityOccupiesTile(const Entity& entity, int tileX, int tileY)
{
    int right = entity.x + getEntityTileWidth(entity);
    int bottom = entity.y + getEntityTileHeight(entity);

    return tileX >= entity.x && tileX < right &&
           tileY >= entity.y && tileY < bottom;
}

Entity* getEntityAt(
    Entity entities[],
    uint8_t entityCount,
    uint8_t x,
    uint8_t y)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            continue;

        if (!entityOccupiesTile(entities[i], x, y))
            continue;

        return &entities[i];
    }

    return nullptr;
}

Entity* getPlayerEntity(
    Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            continue;

        if (entities[i].type == ENTITY_PLAYER)
            return &entities[i];
    }

    return nullptr;
}

const Entity* getPlayerEntity(
    const Entity entities[],
    uint8_t entityCount)
{
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (!entities[i].active)
            continue;

        if (entities[i].type == ENTITY_PLAYER)
            return &entities[i];
    }

    return nullptr;
}

void clearEntities(
    Entity entities[],
    uint8_t& entityCount)
{
    entityCount = 0;
}

const char* getEntityName(const Entity* entity)
{
    if (entity == nullptr)
    {
        return "Unknown";
    }

    switch (entity->type)
    {
        case ENTITY_PLAYER:
            return "Player";

        case ENTITY_MONSTER:
        {
            const Monster* monster = entity->monster;

            if (monster != nullptr)
            {
                return monster->name;
            }

            return "Monster";
        }

        default:
            return "Unknown";
    }
}
