#include "loot.h"

#include <Arduino.h>

#include "characters/characters.h"
#include "characters/items.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "graphics/display.h"
#include "graphics/tiles.h"

uint16_t rollLootGold(const LootTable& table)
{
    if (table.maxGold < table.minGold)
        return 0;

    if (table.minGold == table.maxGold)
        return table.minGold;

    return static_cast<uint16_t>(
        random(table.minGold, static_cast<uint32_t>(table.maxGold) + 1));
}

namespace
{
struct WeightedLootEntry
{
    ItemID item;
    uint8_t weight;
};

const LootTable lootTables[LOOT_COUNT] =
{
    {  0,  0 }, // None
    {  1,  4 }, // Poor
    {  2,  8 }, // Common
    {  5, 12 }, // Uncommon
    { 10, 20 }, // Rare
    { 25, 50 }, // Boss
    {  3, 10 }, // Monster
    {  3, 12 }, // Humanoid
    {  1,  5 }, // Beast
    {  2,  8 }, // Undead
    {  5, 15 }, // Aberration
    {  5, 15 }, // Small chest
    { 15, 30 }, // Medium chest
    { 30, 60 }, // Large chest
    {  0,  0 }  // Skeleton Mage
};

static_assert(
    LOOT_COUNT == (sizeof(lootTables) / sizeof(lootTables[0])),
    "LootTableID and gold ranges are out of sync.");

const WeightedLootEntry poorLoot[] =
{
    { ITEM_NONE, 45 },
    { ITEM_RATIONS, 25 },
    { ITEM_TORCH, 20 },
    { ITEM_ROPE, 10 }
};

const WeightedLootEntry commonLoot[] =
{
    { ITEM_NONE, 15 },
    { ITEM_RATIONS, 25 },
    { ITEM_TORCH, 20 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 15 },
    { ITEM_MANA_POTION, 10 },
    { ITEM_SILVER_RING, 15 }
};

const WeightedLootEntry uncommonLoot[] =
{
    { ITEM_NONE, 10 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 30 },
    { ITEM_MANA_POTION, 10 },
    { ITEM_SILVER_RING, 25 },
    { ITEM_GOLD_RING, 15 },
    { ITEM_ROPE, 10 }
};

const WeightedLootEntry rareLoot[] =
{
    { ITEM_NONE, 5 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 30 },
    { ITEM_MANA_POTION, 10 },
    { ITEM_GOLD_RING, 25 },
    { ITEM_PEARL, 20 },
    { ITEM_RUBY, 10 }
};

const WeightedLootEntry bossLoot[] =
{
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 30 },
    { ITEM_MANA_POTION, 10 },
    { ITEM_GOLD_RING, 20 },
    { ITEM_PEARL, 15 },
    { ITEM_RUBY, 15 },
    { ITEM_GOLDEN_IDOL, 10 }
};

const WeightedLootEntry humanoidLoot[] =
{
    { ITEM_NONE, 5 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 25 },
    { ITEM_MANA_POTION, 10 },
    { ITEM_RATIONS, 20 },
    { ITEM_TORCH, 15 },
    { ITEM_ROPE, 10 },
    { ITEM_SILVER_RING, 15 }
};

const WeightedLootEntry beastLoot[] =
{
    { ITEM_NONE, 50 },
    { ITEM_RATIONS, 25 },
    { ITEM_TORCH, 10 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 10 },
    { ITEM_MANA_POTION, 5 }
};

const WeightedLootEntry undeadLoot[] =
{
    { ITEM_NONE, 25 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 20 },
    { ITEM_MANA_POTION, 10 },
    { ITEM_SILVER_RING, 25 },
    { ITEM_TORCH, 20 }
};

const WeightedLootEntry skeletonMageLoot[] =
{
    { ITEM_GOLD_RING, 50 },
    { ITEM_PEARL, 50 }
};

const WeightedLootEntry monsterLoot[] =
{
    { ITEM_NONE, 25 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 20 },
    { ITEM_MANA_POTION, 10 },
    { ITEM_RATIONS, 25 },
    { ITEM_PEARL, 20 }
};

const WeightedLootEntry aberrationLoot[] =
{
    { ITEM_NONE, 20 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 20 },
    { ITEM_MANA_POTION, 10 },
    { ITEM_SILVER_RING, 20 },
    { ITEM_PEARL, 20 },
    { ITEM_MYSTERIOUS_CRYSTAL, 10 }
};

const WeightedLootEntry chestSmallLoot[] =
{
    { ITEM_NONE, 20 },
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 25 },
    { ITEM_SILVER_RING, 30 },
    { ITEM_RATIONS, 25 }
};

const WeightedLootEntry chestMediumLoot[] =
{
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 25 },
    { ITEM_GOLD_RING, 25 },
    { ITEM_PEARL, 25 },
    { ITEM_RUBY, 25 }
};

const WeightedLootEntry chestLargeLoot[] =
{
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 20 },
    { ITEM_GOLD_RING, 25 },
    { ITEM_RUBY, 20 },
    { ITEM_EMERALD, 20 },
    { ITEM_GOLDEN_IDOL, 15 }
};

bool isNaturalWeapon(ItemID item)
{
    switch (item)
    {
        case ITEM_BITE:
        case ITEM_CLAWS:
        case ITEM_SLAM:
        case ITEM_TENTACLE:
        case ITEM_PSEUDOPOD:
            return true;

        default:
            return false;
    }
}

bool isNaturalArmor(ItemID item)
{
    return item >= ITEM_NATURAL_ARMOR_1 &&
           item <= ITEM_NATURAL_ARMOR_5;
}

void clearCorpseLoot(LootData& loot)
{
    for (uint8_t i = 0; i < MAX_CORPSE_LOOT_SLOTS; i++)
    {
        loot.slots[i].item = makeItemInstance(ITEM_NONE);
        loot.slots[i].quantity = 0;
    }

    loot.itemCount = 0;
    loot.gold = 0;
}

void addLootItem(LootData& loot, ItemID item, uint8_t quantity = 1)
{
    addItemToSlots(
        loot.slots,
        loot.itemCount,
        MAX_CORPSE_LOOT_SLOTS,
        item,
        quantity);
}

ItemID rollWeightedLoot(const WeightedLootEntry entries[], uint8_t count)
{
    uint16_t totalWeight = 0;

    for (uint8_t i = 0; i < count; i++)
        totalWeight += entries[i].weight;

    if (totalWeight == 0)
        return ITEM_NONE;

    uint16_t roll = random(totalWeight);

    for (uint8_t i = 0; i < count; i++)
    {
        if (roll < entries[i].weight)
            return entries[i].item;

        roll -= entries[i].weight;
    }

    return ITEM_NONE;
}

void addOneWeightedLoot(
    LootData& loot,
    const WeightedLootEntry entries[],
    uint8_t count)
{
    ItemID item = rollWeightedLoot(entries, count);

    if (item != ITEM_NONE)
        addLootItem(loot, item);
}

void addHumanoidEquipmentLoot(LootData& loot, const Monster& monster)
{
    const Item* weapon = getItem(monster.weapon);

    if (weapon != nullptr && weapon->type == ITEMTYPE_WEAPON &&
        !isNaturalWeapon(monster.weapon) && random(100) < 80)
    {
        addLootItem(loot, monster.weapon);
    }

    const Item* armor = getItem(monster.armor);

    if (armor != nullptr && armor->type == ITEMTYPE_ARMOR &&
        !isNaturalArmor(monster.armor) && random(100) < 55)
    {
        addLootItem(loot, monster.armor);
    }
}

void addLootForTable(LootData& loot, LootTableID table)
{
    switch (table)
    {
        case LOOT_POOR:
            addOneWeightedLoot(loot, poorLoot,
                               sizeof(poorLoot) / sizeof(poorLoot[0]));
            break;

        case LOOT_COMMON:
            addOneWeightedLoot(loot, commonLoot,
                               sizeof(commonLoot) / sizeof(commonLoot[0]));
            break;

        case LOOT_UNCOMMON:
            addOneWeightedLoot(loot, uncommonLoot,
                               sizeof(uncommonLoot) / sizeof(uncommonLoot[0]));
            break;

        case LOOT_RARE:
            addOneWeightedLoot(loot, rareLoot,
                               sizeof(rareLoot) / sizeof(rareLoot[0]));
            break;

        case LOOT_BOSS:
            addOneWeightedLoot(loot, bossLoot,
                               sizeof(bossLoot) / sizeof(bossLoot[0]));
            addOneWeightedLoot(loot, bossLoot,
                               sizeof(bossLoot) / sizeof(bossLoot[0]));
            break;

        case LOOT_HUMANOID:
            addOneWeightedLoot(loot, humanoidLoot,
                               sizeof(humanoidLoot) / sizeof(humanoidLoot[0]));
            break;

        case LOOT_BEAST:
            addOneWeightedLoot(loot, beastLoot,
                               sizeof(beastLoot) / sizeof(beastLoot[0]));
            break;

        case LOOT_UNDEAD:
            addOneWeightedLoot(loot, undeadLoot,
                               sizeof(undeadLoot) / sizeof(undeadLoot[0]));
            break;

        case LOOT_MONSTER:
            addOneWeightedLoot(loot, monsterLoot,
                               sizeof(monsterLoot) / sizeof(monsterLoot[0]));
            break;

        case LOOT_ABERRATION:
            addOneWeightedLoot(loot, aberrationLoot,
                               sizeof(aberrationLoot) / sizeof(aberrationLoot[0]));
            break;

        case LOOT_SKELETON_MAGE:
            addOneWeightedLoot(loot, skeletonMageLoot,
                               sizeof(skeletonMageLoot) /
                                   sizeof(skeletonMageLoot[0]));
            break;

        case LOOT_CHEST_SMALL:
            addOneWeightedLoot(loot, chestSmallLoot,
                               sizeof(chestSmallLoot) / sizeof(chestSmallLoot[0]));
            break;

        case LOOT_CHEST_MEDIUM:
            addOneWeightedLoot(loot, chestMediumLoot,
                               sizeof(chestMediumLoot) / sizeof(chestMediumLoot[0]));
            break;

        case LOOT_CHEST_LARGE:
            addOneWeightedLoot(loot, chestLargeLoot,
                               sizeof(chestLargeLoot) / sizeof(chestLargeLoot[0]));
            break;

        case LOOT_NONE:
        case LOOT_COUNT:
        default:
            break;
    }
}
}

void generateCorpseLoot(Entity& corpse)
{
    if (corpse.type != ENTITY_MONSTER ||
        corpse.character.state != STATE_DEAD || corpse.loot.generated)
        return;

    clearCorpseLoot(corpse.loot);
    corpse.loot.generated = true;

    const Monster* monster = corpse.monster;

    if (monster == nullptr)
        monster = getMonster(corpse.monsterID);

    if (monster == nullptr)
        return;

    if (monster->lootTable == LOOT_HUMANOID)
        addHumanoidEquipmentLoot(corpse.loot, *monster);

    addLootForTable(corpse.loot, monster->lootTable);

    if (monster->lootTable >= LOOT_NONE &&
        monster->lootTable < LOOT_COUNT)
    {
        corpse.loot.gold = rollLootGold(lootTables[monster->lootTable]);
    }
}

void generateChestLoot(Entity& chest, LootTableID table)
{
    if (chest.type != ENTITY_CHEST || chest.loot.generated)
        return;

    clearCorpseLoot(chest.loot);
    chest.loot.generated = true;
    addLootForTable(chest.loot, table);
    chest.loot.gold = rollLootGold(lootTables[table]);
}

bool corpseHasLoot(const Entity& corpse)
{
    return corpse.active &&
           ((corpse.type == ENTITY_MONSTER &&
             corpse.character.state == STATE_DEAD) ||
            corpse.type == ENTITY_CHEST) &&
           corpse.loot.generated &&
           (corpse.loot.itemCount > 0 || corpse.loot.gold > 0);
}

uint16_t takeCorpseGold(Entity& corpse, Character& recipient)
{
    if (!corpseHasLoot(corpse) || corpse.loot.gold == 0)
        return 0;

    uint16_t gold = corpse.loot.gold;

    if (gold > UINT32_MAX - recipient.inventory.gold)
        recipient.inventory.gold = UINT32_MAX;
    else
        recipient.inventory.gold += gold;

    corpse.loot.gold = 0;
    return gold;
}

bool takeCorpseLootItem(
    Entity& corpse,
    uint8_t slotIndex,
    Character& recipient)
{
    if (!corpseHasLoot(corpse) || slotIndex >= corpse.loot.itemCount)
        return false;

    ItemInstance item = corpse.loot.slots[slotIndex].item;

    if (!addItem(recipient, item))
        return false;

    if (!removeItemFromSlots(
            corpse.loot.slots,
            corpse.loot.itemCount,
            MAX_CORPSE_LOOT_SLOTS,
            item))
    {
        removeItem(recipient, item);
        return false;
    }

    takeCorpseGold(corpse, recipient);

    if (corpse.loot.itemCount == 0)
        finishLootingCorpse(corpse);

    return true;
}

uint16_t takeAllCorpseLoot(Entity& corpse, Character& recipient)
{
    if (!corpseHasLoot(corpse))
        return 0;

    uint16_t goldTaken = takeCorpseGold(corpse, recipient);
    uint16_t taken = 0;
    bool madeProgress = true;

    while (corpse.loot.itemCount > 0 && madeProgress)
    {
        madeProgress = false;

        for (uint8_t index = 0;
             index < corpse.loot.itemCount;)
        {
            if (takeCorpseLootItem(corpse, index, recipient))
            {
                taken++;
                madeProgress = true;

                if (!corpse.active)
                    return taken;

                continue;
            }

            index++;
        }
    }

    if (corpse.loot.itemCount == 0 && goldTaken > 0)
        finishLootingCorpse(corpse);

    return taken;
}

void finishLootingCorpse(Entity& corpse)
{
    if (!corpse.active || (corpse.type != ENTITY_MONSTER &&
        corpse.type != ENTITY_CHEST) ||
        corpse.loot.itemCount != 0 || corpse.loot.gold != 0)
    {
        return;
    }

    markEntityFootprintDirty(corpse);
    if (corpse.type == ENTITY_CHEST)
    {
        corpse.sprite = chestopenwithout;
        return;
    }
    corpse.character.state = STATE_LOOTED;
    removeEntity(corpse);
}
