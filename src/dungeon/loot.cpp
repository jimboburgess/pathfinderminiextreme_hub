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

struct LootLevelBand
{
    uint8_t maximumLevel;
    LootQualityWeights sources[LOOT_SOURCE_COUNT];
};

// Conditional equipment-quality percentages. Columns in every source row are:
// mundane, masterwork, effective +1, +2, +3, +4, +5.
// Source order is normal monster, strong monster, chest, boss, final treasure.
const LootLevelBand lootQualityTable[] =
{
    { 2, {
        {{100, 0,  0,  0,  0,  0, 0}},
        {{100, 0,  0,  0,  0,  0, 0}},
        {{100, 0,  0,  0,  0,  0, 0}},
        {{100, 0,  0,  0,  0,  0, 0}},
        {{100, 0,  0,  0,  0,  0, 0}}
    }},
    { 3, {
        {{95,  5,  0,  0,  0,  0, 0}},
        {{90, 10,  0,  0,  0,  0, 0}},
        {{82, 18,  0,  0,  0,  0, 0}},
        {{75, 25,  0,  0,  0,  0, 0}},
        {{65, 35,  0,  0,  0,  0, 0}}
    }},
    { 4, {
        {{88, 12,  0,  0,  0,  0, 0}},
        {{80, 20,  0,  0,  0,  0, 0}},
        {{68, 32,  0,  0,  0,  0, 0}},
        {{55, 45,  0,  0,  0,  0, 0}},
        {{45, 55,  0,  0,  0,  0, 0}}
    }},
    { 6, {
        {{70, 20, 10,  0,  0,  0, 0}},
        {{58, 25, 17,  0,  0,  0, 0}},
        {{45, 30, 25,  0,  0,  0, 0}},
        {{30, 30, 40,  0,  0,  0, 0}},
        {{20, 30, 50,  0,  0,  0, 0}}
    }},
    { 7, {
        {{69, 20, 10,  1,  0,  0, 0}},
        {{56, 24, 17,  3,  0,  0, 0}},
        {{42, 28, 25,  5,  0,  0, 0}},
        {{27, 27, 38,  8,  0,  0, 0}},
        {{17, 25, 46, 12,  0,  0, 0}}
    }},
    { 11, {
        {{55, 22, 18,  5,  0,  0, 0}},
        {{45, 22, 24,  9,  0,  0, 0}},
        {{30, 20, 35, 15,  0,  0, 0}},
        {{20, 15, 40, 25,  0,  0, 0}},
        {{12, 13, 40, 35,  0,  0, 0}}
    }},
    { 15, {
        {{45, 18, 18, 15,  4,  0, 0}},
        {{35, 18, 20, 20,  7,  0, 0}},
        {{22, 15, 23, 28, 12,  0, 0}},
        {{12, 12, 20, 36, 20,  0, 0}},
        {{ 8,  8, 18, 38, 28,  0, 0}}
    }},
    { 19, {
        {{35, 15, 14, 16, 15,  5, 0}},
        {{25, 14, 14, 18, 20,  9, 0}},
        {{15, 10, 13, 20, 28, 14, 0}},
        {{ 8,  8, 10, 18, 35, 21, 0}},
        {{ 5,  5,  8, 17, 37, 28, 0}}
    }},
    { 20, {
        {{30, 12, 10, 15, 20, 12, 1}},
        {{22, 10,  9, 15, 24, 18, 2}},
        {{12,  8,  8, 14, 27, 28, 3}},
        {{ 6,  6,  6, 12, 28, 36, 6}},
        {{ 4,  4,  5, 10, 27, 42, 8}}
    }}
};

struct LootSourceSettings
{
    uint8_t equipmentChance;
    uint8_t specialPropertyChance;
    uint8_t secondPropertyChance;
};

const LootSourceSettings lootSourceSettings[LOOT_SOURCE_COUNT] =
{
    {15, 12, 10}, // Normal monster
    {25, 18, 12}, // Strong monster
    {60, 25, 15}, // Chest
    {85, 35, 20}, // Boss
    {95, 45, 25}  // Final treasure
};

const ItemID equipmentWeapons[] =
{
    ITEM_CLUB, ITEM_DAGGER, ITEM_GREATCLUB, ITEM_LIGHT_HAMMER, ITEM_MACE,
    ITEM_MORNINGSTAR, ITEM_QUARTERSTAFF, ITEM_SHORTSPEAR, ITEM_SICKLE,
    ITEM_SPEAR, ITEM_BATTLEAXE, ITEM_FALCHION, ITEM_FLAIL, ITEM_GREATAXE,
    ITEM_GREATSWORD, ITEM_HEAVY_PICK, ITEM_LANCE, ITEM_LONGSWORD, ITEM_RAPIER,
    ITEM_SCIMITAR, ITEM_TRIDENT, ITEM_WARHAMMER, ITEM_SHORTBOW, ITEM_LONGBOW,
    ITEM_COMPOSITE_LONGBOW, ITEM_LIGHT_CROSSBOW, ITEM_HEAVY_CROSSBOW,
    ITEM_SLING, ITEM_SCYTHE
};

uint8_t getLootLevelBandIndex(uint8_t characterLevel)
{
    if (characterLevel == 0)
        characterLevel = 1;
    for (uint8_t i = 0;
         i < sizeof(lootQualityTable) / sizeof(lootQualityTable[0]); i++)
    {
        if (characterLevel <= lootQualityTable[i].maximumLevel)
            return i;
    }
    return sizeof(lootQualityTable) / sizeof(lootQualityTable[0]) - 1;
}

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
    { ITEM_SILVER_RING, 15 },
    { ITEM_SCROLL_MAGIC_MISSILE, 5 },
    { ITEM_SCROLL_CURE_LIGHT_WOUNDS, 5 }
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
    { ITEM_PEARL, 15 },
    { ITEM_SCROLL_MAGIC_MISSILE, 5 },
    { ITEM_SCROLL_CURE_LIGHT_WOUNDS, 5 }
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
    { ITEM_RUBY, 20 },
    { ITEM_SCROLL_SLEEP, 10 },
    { ITEM_SCROLL_CURE_LIGHT_WOUNDS, 10 }
};

const WeightedLootEntry chestLargeLoot[] =
{
    { ITEM_POTION_CURE_LIGHT_WOUNDS, 20 },
    { ITEM_GOLD_RING, 25 },
    { ITEM_RUBY, 20 },
    { ITEM_EMERALD, 20 },
    { ITEM_GOLDEN_IDOL, 10 },
    { ITEM_SCROLL_GREASE, 10 },
    { ITEM_SCROLL_MAGIC_MISSILE, 10 },
    { ITEM_SCROLL_CURE_LIGHT_WOUNDS, 10 }
};

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

void addLootItem(LootData& loot,
                 const ItemInstance& item,
                 uint8_t quantity = 1)
{
    addItemToSlots(
        loot.slots,
        loot.itemCount,
        MAX_CORPSE_LOOT_SLOTS,
        item,
        quantity);
}

ItemID rollEquipmentBaseItem(LootQuality quality)
{
    // Masterwork armor has no implemented mechanical benefit in this pass, so
    // a masterwork-quality result deliberately selects a manufactured weapon.
    if (quality == LOOT_QUALITY_MASTERWORK)
    {
        return equipmentWeapons[random(
            sizeof(equipmentWeapons) / sizeof(equipmentWeapons[0]))];
    }

    const uint8_t categoryRoll = random(100);
    if (categoryRoll < 55)
    {
        return equipmentWeapons[random(
            sizeof(equipmentWeapons) / sizeof(equipmentWeapons[0]))];
    }
    if (categoryRoll < 85)
    {
        return static_cast<ItemID>(random(
            ITEM_PADDED_ARMOR, static_cast<long>(ITEM_FULL_PLATE) + 1));
    }
    return static_cast<ItemID>(random(
        ITEM_BUCKLER, static_cast<long>(ITEM_TOWER_SHIELD) + 1));
}

uint8_t rollWeaponMagicProperties(ItemID itemID,
                                  uint8_t effectiveBonus,
                                  LootSource source)
{
    if (effectiveBonus < 2 || source >= LOOT_SOURCE_COUNT ||
        random(100) >= lootSourceSettings[source].specialPropertyChance)
    {
        return WEAPON_PROPERTY_NONE;
    }

    WeaponProperty candidates[4] =
    {
        WEAPON_PROPERTY_FLAMING,
        WEAPON_PROPERTY_FROST,
        WEAPON_PROPERTY_SHOCK,
        WEAPON_PROPERTY_KEEN
    };
    uint8_t candidateCount = isKeenEligibleWeapon(itemID) ? 4 : 3;
    const uint8_t firstIndex = random(candidateCount);
    uint8_t properties = candidates[firstIndex];

    if (effectiveBonus >= 3 &&
        random(100) < lootSourceSettings[source].secondPropertyChance)
    {
        candidates[firstIndex] = candidates[candidateCount - 1];
        candidateCount--;
        properties |= candidates[random(candidateCount)];
    }
    return properties;
}

void addGeneratedEquipment(LootData& loot,
                           uint8_t characterLevel,
                           LootSource source)
{
    if (source >= LOOT_SOURCE_COUNT ||
        random(100) >= lootSourceSettings[source].equipmentChance)
    {
        return;
    }

    const LootQuality quality = selectLootQuality(
        characterLevel, source, static_cast<uint8_t>(random(100)));
    const ItemID baseItem = rollEquipmentBaseItem(quality);
    const uint8_t effectiveBonus = getLootQualityEffectiveBonus(quality);
    const uint8_t properties = effectiveBonus > 0 &&
        getWeapon(baseItem) != nullptr
        ? rollWeaponMagicProperties(baseItem, effectiveBonus, source)
        : WEAPON_PROPERTY_NONE;
    addLootItem(loot, createLootEquipment(baseItem, quality, properties));
}

LootSource getMonsterLootSource(const Monster& monster)
{
    if (monster.lootTable == LOOT_BOSS ||
        monster.lootTable == LOOT_SKELETON_MAGE)
    {
        return LOOT_SOURCE_BOSS;
    }
    return monster.challengeRating >= CR_THREE
        ? LOOT_SOURCE_STRONG_MONSTER : LOOT_SOURCE_NORMAL_MONSTER;
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
        isManufacturedWeapon(monster.weapon) && random(100) < 80)
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

const LootQualityWeights& getLootQualityWeights(uint8_t characterLevel,
                                                LootSource source)
{
    if (source >= LOOT_SOURCE_COUNT)
        source = LOOT_SOURCE_NORMAL_MONSTER;
    return lootQualityTable[getLootLevelBandIndex(characterLevel)].sources[
        source];
}

LootQuality selectLootQuality(uint8_t characterLevel,
                              LootSource source,
                              uint8_t percentileRoll)
{
    const LootQualityWeights& weights = getLootQualityWeights(
        characterLevel, source);
    uint8_t roll = percentileRoll % 100;
    for (uint8_t quality = 0; quality < LOOT_QUALITY_COUNT; quality++)
    {
        if (roll < weights.weights[quality])
            return static_cast<LootQuality>(quality);
        roll -= weights.weights[quality];
    }
    return LOOT_QUALITY_MUNDANE;
}

uint8_t getEquipmentDropChance(LootSource source)
{
    return source < LOOT_SOURCE_COUNT
        ? lootSourceSettings[source].equipmentChance : 0;
}

uint8_t getLootQualityEffectiveBonus(LootQuality quality)
{
    return quality >= LOOT_QUALITY_MAGIC_1 &&
           quality <= LOOT_QUALITY_MAGIC_5
        ? static_cast<uint8_t>(quality - LOOT_QUALITY_MAGIC_1 + 1) : 0;
}

ItemInstance createLootEquipment(ItemID baseItem,
                                 LootQuality quality,
                                 uint8_t desiredWeaponProperties)
{
    const Item* definition = getItem(baseItem);
    if (definition == nullptr)
        return makeItemInstance(ITEM_NONE);
    if (quality == LOOT_QUALITY_MASTERWORK)
        return makeMasterworkWeapon(baseItem);

    const uint8_t effectiveBonus = getLootQualityEffectiveBonus(quality);
    if (effectiveBonus == 0)
        return makeItemInstance(baseItem);
    if (definition->type == ITEMTYPE_WEAPON)
    {
        return makeMagicWeapon(
            baseItem, effectiveBonus, desiredWeaponProperties);
    }
    ItemInstance item = makeItemInstance(baseItem);
    if (definition->type == ITEMTYPE_ARMOR ||
        definition->type == ITEMTYPE_SHIELD)
    {
        item.enhancementBonus = static_cast<int8_t>(effectiveBonus);
    }
    return item;
}

void generateCorpseLoot(Entity& corpse, uint8_t characterLevel)
{
    if (corpse.type != ENTITY_MONSTER ||
        (corpse.character.state != STATE_DEAD &&
         corpse.character.state != STATE_TURNED) || corpse.loot.generated)
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
    addGeneratedEquipment(
        corpse.loot, characterLevel, getMonsterLootSource(*monster));

    if (monster->lootTable >= LOOT_NONE &&
        monster->lootTable < LOOT_COUNT)
    {
        corpse.loot.gold = rollLootGold(lootTables[monster->lootTable]);
    }
}

void generateChestLoot(Entity& chest,
                       LootTableID table,
                       uint8_t characterLevel,
                       LootSource source)
{
    if (chest.type != ENTITY_CHEST || chest.loot.generated)
        return;

    clearCorpseLoot(chest.loot);
    chest.loot.generated = true;
    addLootForTable(chest.loot, table);
    addGeneratedEquipment(chest.loot, characterLevel, source);
    if (table >= LOOT_NONE && table < LOOT_COUNT)
        chest.loot.gold = rollLootGold(lootTables[table]);
}

bool corpseHasLoot(const Entity& corpse)
{
    return corpse.active &&
           ((corpse.type == ENTITY_MONSTER &&
             (corpse.character.state == STATE_DEAD ||
              corpse.character.state == STATE_TURNED)) ||
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
