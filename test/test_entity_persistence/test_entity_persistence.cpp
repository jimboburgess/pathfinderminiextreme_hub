#include <Arduino.h>
#include <unity.h>

#include <cstring>

#include "../../src/dungeon/entitypersistence.h"
#include "../../src/graphics/monstersprites.h"
#include "../../src/graphics/npcsprites.h"
#include "../../src/graphics/tiles.h"

// Lightweight graphics definitions keep this test focused on persistence.
const uint16_t goblinSprite16x16r1[SPRITE_W * SPRITE_H] = {};
const uint16_t goblinSprite16x16r2[SPRITE_W * SPRITE_H] = {};
const uint16_t goblinArcher16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t bugbear16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t skeleton16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t skeletonMage16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t zombie16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t ghoul16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t wight16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t choker16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t giantspider32x32[LRGSPRITE_W * LRGSPRITE_H] = {};
const uint16_t spectator32x32[LRGSPRITE_W * LRGSPRITE_H] = {};
const uint16_t grayOoze16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t violetFungus16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t spectator16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t bertram16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t bertramCat16x16[SPRITE_W * SPRITE_H] = {};
const uint16_t chestclosed[SPRITE_W * SPRITE_H] = {};
const uint16_t chestopenwith[SPRITE_W * SPRITE_H] = {};
const uint16_t chestopenwithout[SPRITE_W * SPRITE_H] = {};

int rollDice(int count, int)
{
    return count;
}

int getAbilityModifier(int score)
{
    return score >= 10 ? (score - 10) / 2 : (score - 11) / 2;
}

const Weapon* getWeapon(ItemID)
{
    return nullptr;
}

bool addItem(Character& character, ItemID item, uint8_t quantity)
{
    if (quantity == 0 || character.inventory.itemCount >= MAX_INVENTORY)
        return quantity == 0;
    InventorySlot& slot =
        character.inventory.slots[character.inventory.itemCount++];
    slot.item = makeItemInstance(item);
    slot.quantity = quantity;
    return true;
}

const NPCDefinition* getNPCDefinition(NPCID id)
{
    static const NPCDefinition bertram =
    {
        NPC_BERTRAM_RIDDLEMAN,
        "Bertram, Door Enthusiast",
        TEAM_NEUTRAL,
        bertram16x16,
        "Bertram watches you expectantly."
    };
    return id == NPC_BERTRAM_RIDDLEMAN ? &bertram : nullptr;
}

#include "../../src/dungeon/monsters.cpp"
#include "../../src/data/entityspawn.cpp"
#include "../../src/dungeon/entitypersistence.cpp"

namespace
{
Entity makeMonster(MonsterID id, uint8_t x = 4, uint8_t y = 5)
{
    Entity entity{};
    entity.type = ENTITY_MONSTER;
    entity.active = true;
    entity.x = x;
    entity.y = y;
    TEST_ASSERT_TRUE(initializeMonsterDefinitionState(entity, id));
    return entity;
}

InventorySlot makeSlot(ItemID id, uint8_t quantity)
{
    InventorySlot slot{};
    slot.item = makeItemInstance(id);
    slot.quantity = quantity;
    return slot;
}

void assertSlotEquals(const InventorySlot& expected,
                      const InventorySlot& actual)
{
    TEST_ASSERT_EQUAL(expected.item.itemID, actual.item.itemID);
    TEST_ASSERT_EQUAL_INT8(expected.item.enhancementBonus,
                           actual.item.enhancementBonus);
    TEST_ASSERT_EQUAL_UINT8(expected.item.weaponProperties,
                            actual.item.weaponProperties);
    TEST_ASSERT_EQUAL_UINT8(expected.quantity, actual.quantity);
}

Entity roundTrip(const Entity& source)
{
    PersistentEntity packed{};
    Entity restored{};
    TEST_ASSERT_TRUE(packPersistentEntity(source, packed));
    TEST_ASSERT_TRUE(inflatePersistentEntity(packed, restored));
    return restored;
}
}

void test_living_monster_round_trip_preserves_mutable_state()
{
    Entity source = makeMonster(MONSTER_GOBLIN_SCIMITAR, 14, 13);
    source.character.health.currentHP = 7;
    source.character.health.maxHP = 13;
    source.character.magic.currentMP = 0;
    source.character.inventory.itemCount = 3;
    source.character.inventory.slots[0] =
        makeSlot(ITEM_POTION_CURE_LIGHT_WOUNDS, 2);
    source.character.inventory.slots[1] = makeSlot(ITEM_MANA_POTION, 1);
    source.character.inventory.slots[2] =
        makeSlot(ITEM_SCROLL_MAGIC_MISSILE, 1);
    source.loot.generated = true;
    source.loot.gold = 27;
    source.loot.itemCount = 1;
    source.loot.slots[0] = makeSlot(ITEM_POTION_CURE_LIGHT_WOUNDS, 1);
    source.awareOfPlayer = true;
    source.revealedToPlayer = true;
    source.hasLastKnownPosition = true;
    source.lastKnownX = 12;
    source.lastKnownY = 10;
    source.idleDirection = DIR_WEST;
    source.idleStepsRemaining = 3;
    source.nextIdleActionTime = 123456;

    Entity restored = roundTrip(source);
    TEST_ASSERT_EQUAL(ENTITY_MONSTER, restored.type);
    TEST_ASSERT_EQUAL(MONSTER_GOBLIN_SCIMITAR, restored.monsterID);
    TEST_ASSERT_EQUAL_UINT8(14, restored.x);
    TEST_ASSERT_EQUAL_UINT8(13, restored.y);
    TEST_ASSERT_TRUE(restored.active);
    TEST_ASSERT_EQUAL_INT(7, restored.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(13, restored.character.health.maxHP);
    TEST_ASSERT_EQUAL_UINT8(3, restored.character.inventory.itemCount);
    for (uint8_t i = 0; i < 3; ++i)
        assertSlotEquals(source.character.inventory.slots[i],
                         restored.character.inventory.slots[i]);
    TEST_ASSERT_EQUAL_UINT16(27, restored.loot.gold);
    TEST_ASSERT_EQUAL_UINT8(1, restored.loot.itemCount);
    TEST_ASSERT_TRUE(restored.awareOfPlayer);
    TEST_ASSERT_TRUE(restored.revealedToPlayer);
    TEST_ASSERT_TRUE(restored.hasLastKnownPosition);
    TEST_ASSERT_EQUAL_UINT8(12, restored.lastKnownX);
    TEST_ASSERT_EQUAL_UINT8(10, restored.lastKnownY);
    TEST_ASSERT_EQUAL(DIR_WEST, restored.idleDirection);
    TEST_ASSERT_EQUAL_UINT8(3, restored.idleStepsRemaining);
    TEST_ASSERT_EQUAL_UINT32(123456, restored.nextIdleActionTime);
    TEST_ASSERT_EQUAL_PTR(getMonster(MONSTER_GOBLIN_SCIMITAR),
                          restored.monster);
    TEST_ASSERT_EQUAL_PTR(goblinSprite16x16r1, restored.sprite);
    TEST_ASSERT_EQUAL(TEAM_MONSTER, restored.character.team);
    TEST_ASSERT_EQUAL(
        getMonster(MONSTER_GOBLIN_SCIMITAR)->creatureType,
        restored.character.creatureType);
    TEST_ASSERT_FALSE(restored.visibleToPlayer);
    TEST_ASSERT_FALSE(restored.playerHasLineOfSight);
    TEST_ASSERT_FALSE(restored.turn.standardActionUsed);
}

void test_damaged_caster_round_trip_reconstructs_static_magic()
{
    Entity source = makeMonster(MONSTER_SKELETON_MAGE);
    source.character.health.maxHP = 24;
    source.character.health.currentHP = 9;
    source.character.magic.currentMP = 3;

    Entity restored = roundTrip(source);
    const Monster* definition = getMonster(MONSTER_SKELETON_MAGE);
    TEST_ASSERT_EQUAL_INT(9, restored.character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(24, restored.character.health.maxHP);
    TEST_ASSERT_EQUAL_INT(3, restored.character.magic.currentMP);
    TEST_ASSERT_EQUAL_INT(definition->maxMP,
                          restored.character.magic.maxMP);
    TEST_ASSERT_EQUAL_UINT8(definition->casterLevel,
                            restored.character.level);
    TEST_ASSERT_EQUAL_PTR(definition, restored.monster);
    TEST_ASSERT_EQUAL_PTR(definition->sprite, restored.sprite);
}

void test_conditions_round_trip_without_compaction()
{
    Entity source = makeMonster(MONSTER_ZOMBIE);
    ConditionData& conditions = source.character.conditions;
    conditions.count = 3;
    conditions.conditions[0].type = CONDITION_SLEEPING;
    conditions.conditions[0].value = 2;
    conditions.conditions[0].roundsRemaining = 4;
    conditions.conditions[1].type = CONDITION_WEBBED;
    conditions.conditions[1].value = 1;
    conditions.conditions[1].roundsRemaining = 6;
    conditions.conditions[2].type = CONDITION_PRONE;
    conditions.timedDamageCount = 1;
    conditions.timedDamage[0].damageType = 2;
    conditions.timedDamage[0].diceCount = 1;
    conditions.timedDamage[0].diceSides = 6;
    conditions.timedDamage[0].roundsRemaining = 3;
    conditions.timedDamage[0].sourceAbility = 17;
    conditions.energyResistanceCount = 1;
    conditions.energyResistances[0].damageType = 3;
    conditions.energyResistances[0].amount = 5;
    conditions.energyResistances[0].roundsRemaining = 8;
    conditions.energyProtectionCount = 1;
    conditions.energyProtections[0].damageType = 4;
    conditions.energyProtections[0].remainingAbsorption = 11;
    conditions.energyProtections[0].roundsRemaining = 7;
    conditions.poison.type = POISON_SLOWING_VENOM;
    conditions.poison.stage = 2;
    conditions.poison.roundsUntilSave = 5;
    conditions.poison.saveDC = 18;

    Entity restored = roundTrip(source);
    TEST_ASSERT_EQUAL_MEMORY(&conditions, &restored.character.conditions,
                             sizeof(ConditionData));
}

void test_monster_item_capacity_is_general_and_overflow_fails_cleanly()
{
    Entity source = makeMonster(MONSTER_GOBLIN_SCIMITAR);
    source.character.inventory.itemCount = MAX_PERSISTENT_MONSTER_ITEMS;
    for (uint8_t i = 0; i < MAX_PERSISTENT_MONSTER_ITEMS; ++i)
    {
        source.character.inventory.slots[i].item = makeItemInstance(
            (i & 1) ? ITEM_MANA_POTION : ITEM_POTION_CURE_LIGHT_WOUNDS);
        source.character.inventory.slots[i].quantity = i + 1;
    }
    Entity restored = roundTrip(source);
    TEST_ASSERT_EQUAL_UINT8(MAX_PERSISTENT_MONSTER_ITEMS,
                            restored.character.inventory.itemCount);

    source.character.inventory.itemCount =
        MAX_PERSISTENT_MONSTER_ITEMS + 1;
    source.character.inventory.slots[MAX_PERSISTENT_MONSTER_ITEMS].item =
        makeItemInstance(ITEM_POTION_CURE_LIGHT_WOUNDS);
    source.character.inventory.slots[MAX_PERSISTENT_MONSTER_ITEMS].quantity = 1;
    PersistentEntity packed{};
    TEST_ASSERT_FALSE(packPersistentEntity(source, packed));
    TEST_ASSERT_EQUAL(ENTITY_NONE, packed.type);
    TEST_ASSERT_EQUAL_UINT8(0, packed.flags);
}

void test_dead_unlooted_and_looted_monsters_round_trip()
{
    Entity corpse = makeMonster(MONSTER_BUGBEAR, 8, 9);
    corpse.character.state = STATE_DEAD;
    corpse.character.health.currentHP = -4;
    corpse.character.health.maxHP = 18;
    corpse.loot.generated = true;
    corpse.loot.gold = 91;
    corpse.loot.itemCount = 2;
    corpse.loot.slots[0] = makeSlot(ITEM_MANA_POTION, 1);
    corpse.loot.slots[1] = makeSlot(ITEM_POTION_CURE_LIGHT_WOUNDS, 2);
    Entity restoredCorpse = roundTrip(corpse);
    TEST_ASSERT_EQUAL(STATE_DEAD, restoredCorpse.character.state);
    TEST_ASSERT_EQUAL_INT(-4, restoredCorpse.character.health.currentHP);
    TEST_ASSERT_TRUE(restoredCorpse.loot.generated);
    TEST_ASSERT_EQUAL_UINT16(91, restoredCorpse.loot.gold);
    TEST_ASSERT_EQUAL_UINT8(2, restoredCorpse.loot.itemCount);

    corpse.active = false;
    corpse.character.state = STATE_LOOTED;
    corpse.loot = LootData{};
    Entity restoredLooted = roundTrip(corpse);
    TEST_ASSERT_FALSE(restoredLooted.active);
    TEST_ASSERT_EQUAL(STATE_LOOTED, restoredLooted.character.state);
    TEST_ASSERT_FALSE(restoredLooted.loot.generated);
}

void test_chest_round_trip_preserves_lock_open_and_remaining_loot()
{
    Entity chest{};
    chest.type = ENTITY_CHEST;
    chest.active = true;
    chest.x = 2;
    chest.y = 3;
    chest.locked = true;
    chest.opened = true;
    chest.loot.generated = true;
    chest.loot.gold = 12;
    chest.loot.itemCount = 2;
    chest.loot.slots[0] = makeSlot(ITEM_MANA_POTION, 2);
    chest.loot.slots[1] = makeSlot(ITEM_LONGSWORD, 1);
    chest.loot.slots[1].item.enhancementBonus = 2;
    chest.loot.slots[1].item.weaponProperties =
        WEAPON_PROPERTY_FLAMING | WEAPON_PROPERTY_KEEN;
    Entity restored = roundTrip(chest);
    TEST_ASSERT_TRUE(restored.locked);
    TEST_ASSERT_TRUE(restored.opened);
    TEST_ASSERT_EQUAL_UINT16(12, restored.loot.gold);
    TEST_ASSERT_EQUAL_UINT8(2, restored.loot.itemCount);
    assertSlotEquals(chest.loot.slots[1], restored.loot.slots[1]);
    TEST_ASSERT_EQUAL_PTR(chestopenwith, restored.sprite);

    chest.locked = false;
    chest.loot = LootData{};
    restored = roundTrip(chest);
    TEST_ASSERT_EQUAL_PTR(chestopenwithout, restored.sprite);
}

void test_npc_cat_key_and_basic_loot_round_trip()
{
    Entity npc{};
    npc.type = ENTITY_NPC;
    npc.active = true;
    npc.x = 6;
    npc.y = 7;
    TEST_ASSERT_TRUE(initializeNPCDefinitionState(
        npc, NPC_BERTRAM_RIDDLEMAN));
    Entity restoredNPC = roundTrip(npc);
    TEST_ASSERT_EQUAL(NPC_BERTRAM_RIDDLEMAN, restoredNPC.npcID);
    TEST_ASSERT_EQUAL(TEAM_NEUTRAL, restoredNPC.character.team);
    TEST_ASSERT_EQUAL_PTR(bertram16x16, restoredNPC.sprite);

    Entity cat{};
    cat.type = ENTITY_RIDDLE_CAT;
    cat.active = true;
    cat.x = 10;
    cat.y = 11;
    cat.character.team = TEAM_NEUTRAL;
    cat.character.state = STATE_ALIVE;
    Entity restoredCat = roundTrip(cat);
    TEST_ASSERT_EQUAL(ENTITY_RIDDLE_CAT, restoredCat.type);
    TEST_ASSERT_EQUAL_UINT8(10, restoredCat.x);
    TEST_ASSERT_EQUAL_UINT8(11, restoredCat.y);
    TEST_ASSERT_EQUAL(TEAM_NEUTRAL, restoredCat.character.team);
    TEST_ASSERT_EQUAL_PTR(bertramCat16x16, restoredCat.sprite);

    Entity key{};
    key.type = ENTITY_PUZZLE_KEY;
    key.active = true;
    key.x = 1;
    key.y = 2;
    Entity restoredKey = roundTrip(key);
    TEST_ASSERT_EQUAL(ENTITY_PUZZLE_KEY, restoredKey.type);
    TEST_ASSERT_TRUE(restoredKey.active);

    Entity loot{};
    loot.type = ENTITY_LOOT;
    loot.active = false;
    loot.x = 3;
    loot.y = 4;
    Entity restoredLoot = roundTrip(loot);
    TEST_ASSERT_EQUAL(ENTITY_LOOT, restoredLoot.type);
    TEST_ASSERT_FALSE(restoredLoot.active);
}

void test_players_invalid_coordinates_and_unsupported_payload_are_rejected()
{
    PersistentEntity packed{};
    Entity entity{};
    entity.type = ENTITY_PLAYER;
    entity.x = 0;
    entity.y = 0;
    TEST_ASSERT_FALSE(packPersistentEntity(entity, packed));

    entity = Entity{};
    entity.type = ENTITY_PUZZLE_KEY;
    entity.x = ROOM_WIDTH;
    entity.y = 0;
    TEST_ASSERT_FALSE(packPersistentEntity(entity, packed));
    entity.x = 0;
    entity.y = ROOM_HEIGHT;
    TEST_ASSERT_FALSE(packPersistentEntity(entity, packed));

    entity.x = ROOM_WIDTH - 1;
    entity.y = ROOM_HEIGHT - 1;
    entity.character.health.currentHP = 1;
    TEST_ASSERT_FALSE(packPersistentEntity(entity, packed));

    PersistentEntity invalid{};
    invalid.type = ENTITY_MONSTER;
    invalid.x = ROOM_WIDTH;
    invalid.y = 0;
    Entity stale = makeMonster(MONSTER_ZOMBIE);
    TEST_ASSERT_FALSE(inflatePersistentEntity(invalid, stale));
    TEST_ASSERT_EQUAL(ENTITY_NONE, stale.type);
    TEST_ASSERT_FALSE(stale.active);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_living_monster_round_trip_preserves_mutable_state);
    RUN_TEST(test_damaged_caster_round_trip_reconstructs_static_magic);
    RUN_TEST(test_conditions_round_trip_without_compaction);
    RUN_TEST(test_monster_item_capacity_is_general_and_overflow_fails_cleanly);
    RUN_TEST(test_dead_unlooted_and_looted_monsters_round_trip);
    RUN_TEST(test_chest_round_trip_preserves_lock_open_and_remaining_loot);
    RUN_TEST(test_npc_cat_key_and_basic_loot_round_trip);
    RUN_TEST(test_players_invalid_coordinates_and_unsupported_payload_are_rejected);
    UNITY_END();
}

void loop()
{
}
