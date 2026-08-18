#include <Arduino.h>
#include <unity.h>

#include "../../src/data/entityspawn.h"
#include "../../src/graphics/monstersprites.h"

// Lightweight sprite definitions keep this embedded unit focused on monster
// data and spawning rather than linking the production bitmap assets.
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

static int nextHitPointRoll = 0;
static uint8_t hitPointRollCount = 0;
static int lastHitDieCount = 0;
static int lastHitDieSides = 0;

int rollDice(int count, int sides)
{
    hitPointRollCount++;
    lastHitDieCount = count;
    lastHitDieSides = sides;
    return nextHitPointRoll;
}

int getAbilityModifier(int score)
{
    return score >= 10 ? (score - 10) / 2 : (score - 11) / 2;
}

const Weapon* getWeapon(ItemID)
{
    return nullptr;
}

// spawnMonster() stocks explicitly configured monster consumables through the
// normal Character inventory. Keep this unit self-contained while preserving
// the production call boundary.
bool addItem(Character& character, ItemID item, uint8_t quantity)
{
    if (quantity == 0)
        return true;

    for (uint8_t i = 0; i < character.inventory.itemCount; i++)
    {
        InventorySlot& slot = character.inventory.slots[i];

        if (slot.item.itemID == item)
        {
            slot.quantity += quantity;
            return true;
        }
    }

    if (character.inventory.itemCount >= MAX_INVENTORY)
        return false;

    InventorySlot& slot =
        character.inventory.slots[character.inventory.itemCount++];
    slot.item = makeItemInstance(item);
    slot.quantity = quantity;
    return true;
}

#include "../../src/dungeon/monsters.cpp"
#include "../../src/data/entityspawn.cpp"

static void useHitPointRoll(int total)
{
    nextHitPointRoll = total;
    hitPointRollCount = 0;
    lastHitDieCount = 0;
    lastHitDieSides = 0;
}

void test_low_constitution_hp_never_wraps()
{
    const MonsterID noConstitutionMonsters[] =
    {
        MONSTER_SKELETON,
        MONSTER_ZOMBIE,
        MONSTER_GHOUL,
        MONSTER_WIGHT,
        MONSTER_GRAY_OOZE,
        MONSTER_VIOLET_FUNGUS
    };

    for (uint8_t i = 0;
         i < sizeof(noConstitutionMonsters) /
                 sizeof(noConstitutionMonsters[0]);
         i++)
    {
        const Monster* monster = getMonster(noConstitutionMonsters[i]);
        TEST_ASSERT_NOT_NULL(monster);

        // This is the minimum possible total for hitDice d8. The negative
        // Constitution modifier should clamp to one HP per die, not wrap.
        useHitPointRoll(monster->hitDice);
        TEST_ASSERT_EQUAL_UINT16(
            monster->hitDice, getMonsterMaxHP(*monster));
        TEST_ASSERT_EQUAL_UINT8(1, hitPointRollCount);
    }

    const Monster* zombie = getMonster(MONSTER_ZOMBIE);
    TEST_ASSERT_NOT_NULL(zombie);
    useHitPointRoll(16);
    TEST_ASSERT_EQUAL_UINT16(6, getMonsterMaxHP(*zombie));
}

void test_zombie_database_fields_and_id_lookup_are_aligned()
{
    TEST_ASSERT_NULL(getMonster(MONSTER_NONE));
    TEST_ASSERT_NULL(getMonster(MONSTER_COUNT));

    for (uint8_t id = MONSTER_GOBLIN_SCIMITAR; id < MONSTER_COUNT; id++)
    {
        TEST_ASSERT_EQUAL_PTR(
            &monsterDatabase[id],
            getMonster(static_cast<MonsterID>(id)));
    }

    const Monster& zombie = monsterDatabase[MONSTER_ZOMBIE];
    TEST_ASSERT_EQUAL_STRING("Zombie", zombie.name);
    TEST_ASSERT_EQUAL_UINT8(2, zombie.hitDice);
    TEST_ASSERT_EQUAL_UINT8(0, zombie.abilities.constitution);
    TEST_ASSERT_EQUAL_UINT8(1, zombie.baseAttack);
    TEST_ASSERT_EQUAL_UINT8(14, zombie.armorClass);
    TEST_ASSERT_EQUAL_INT8(3, zombie.fortitude);
    TEST_ASSERT_EQUAL_INT8(0, zombie.reflex);
    TEST_ASSERT_EQUAL_INT8(3, zombie.will);
    TEST_ASSERT_EQUAL_UINT8(4, zombie.speed);
    TEST_ASSERT_EQUAL(ITEM_SLAM, zombie.weapon);
    TEST_ASSERT_EQUAL(ITEM_NATURAL_ARMOR_2, zombie.armor);
    TEST_ASSERT_EQUAL(CR_ONE_HALF, zombie.challengeRating);
    TEST_ASSERT_EQUAL(LOOT_UNDEAD, zombie.lootTable);
    TEST_ASSERT_EQUAL(SCRIPT_MELEE, zombie.script);
    TEST_ASSERT_EQUAL(CREATURE_ZOMBIE, zombie.creatureType);

    const Monster& spectator = monsterDatabase[MONSTER_SPECTATOR];
    TEST_ASSERT_EQUAL(
        ABILITY_MAGIC_MISSILE, spectator.specialAbilities[0]);
    TEST_ASSERT_EQUAL(SCRIPT_SPELLCASTER, spectator.script);
    TEST_ASSERT_EQUAL_UINT8(6, spectator.maxMP);
    TEST_ASSERT_EQUAL_UINT8(4, spectator.casterLevel);
    TEST_ASSERT_EQUAL(CREATURE_BEHOLDER, spectator.creatureType);

    const Monster& skeletonMage = monsterDatabase[MONSTER_SKELETON_MAGE];
    TEST_ASSERT_EQUAL_STRING("Skeleton Mage", skeletonMage.name);
    TEST_ASSERT_EQUAL_PTR(skeletonMage16x16, skeletonMage.sprite);
    TEST_ASSERT_EQUAL_UINT8(3, skeletonMage.hitDice);
    TEST_ASSERT_EQUAL_UINT8(1, skeletonMage.baseAttack);
    TEST_ASSERT_EQUAL_UINT8(15, skeletonMage.armorClass);
    TEST_ASSERT_EQUAL_INT8(5, skeletonMage.fortitude);
    TEST_ASSERT_EQUAL_INT8(1, skeletonMage.reflex);
    TEST_ASSERT_EQUAL_INT8(5, skeletonMage.will);
    TEST_ASSERT_EQUAL_UINT8(6, skeletonMage.speed);
    TEST_ASSERT_EQUAL(ITEM_SCYTHE, skeletonMage.weapon);
    TEST_ASSERT_EQUAL(ITEM_NATURAL_ARMOR_3, skeletonMage.armor);
    TEST_ASSERT_EQUAL(ABILITY_COLOR_SPRAY, skeletonMage.specialAbilities[0]);
    TEST_ASSERT_EQUAL(ABILITY_GREASE, skeletonMage.specialAbilities[1]);
    TEST_ASSERT_EQUAL(SCRIPT_SPELLCASTER, skeletonMage.script);
    TEST_ASSERT_EQUAL_UINT8(8, skeletonMage.maxMP);
    TEST_ASSERT_EQUAL_UINT8(3, skeletonMage.casterLevel);
    TEST_ASSERT_EQUAL(CREATURE_SKELETON, skeletonMage.creatureType);
}

void test_reused_entity_gets_one_fresh_monster_hp_roll()
{
    Entity entities[MAX_ENTITIES] = {};
    uint8_t entityCount = 1;
    Entity& oldEntity = entities[0];

    oldEntity.active = false;
    oldEntity.type = ENTITY_PLAYER;
    oldEntity.character.health.maxHP = 64000;
    oldEntity.character.health.currentHP = 63000;
    oldEntity.character.team = TEAM_PLAYER;
    oldEntity.character.state = STATE_LOOTED;
    oldEntity.character.magic.maxMP = 99;
    oldEntity.character.magic.currentMP = 88;
    oldEntity.character.level = 99;
    oldEntity.monsterID = MONSTER_SPECTATOR;
    oldEntity.monster = getMonster(MONSTER_SPECTATOR);
    oldEntity.loot.generated = true;
    oldEntity.turn.standardActionUsed = true;

    useHitPointRoll(2);
    Entity* zombie = spawnMonster(
        entities, entityCount, MONSTER_ZOMBIE, 4, 5);

    TEST_ASSERT_EQUAL_PTR(&oldEntity, zombie);
    TEST_ASSERT_EQUAL_UINT8(1, entityCount);
    TEST_ASSERT_TRUE(zombie->active);
    TEST_ASSERT_EQUAL(ENTITY_MONSTER, zombie->type);
    TEST_ASSERT_EQUAL_UINT8(4, zombie->x);
    TEST_ASSERT_EQUAL_UINT8(5, zombie->y);
    TEST_ASSERT_EQUAL(TEAM_MONSTER, zombie->character.team);
    TEST_ASSERT_EQUAL(STATE_ALIVE, zombie->character.state);
    TEST_ASSERT_EQUAL(
        CREATURE_ZOMBIE, zombie->character.creatureType);
    TEST_ASSERT_EQUAL_UINT16(2, zombie->character.health.maxHP);
    TEST_ASSERT_EQUAL_UINT16(2, zombie->character.health.currentHP);
    TEST_ASSERT_EQUAL(MONSTER_ZOMBIE, zombie->monsterID);
    TEST_ASSERT_EQUAL_PTR(getMonster(MONSTER_ZOMBIE), zombie->monster);
    TEST_ASSERT_EQUAL_INT(0, zombie->character.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(0, zombie->character.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT8(0, zombie->character.level);
    TEST_ASSERT_FALSE(zombie->loot.generated);
    TEST_ASSERT_FALSE(zombie->turn.standardActionUsed);
    TEST_ASSERT_EQUAL_UINT8(1, hitPointRollCount);
    TEST_ASSERT_EQUAL_INT(2, lastHitDieCount);
    TEST_ASSERT_EQUAL_INT(8, lastHitDieSides);
}

void test_spellcaster_spawn_receives_definition_mp_pool()
{
    Entity entities[MAX_ENTITIES] = {};
    uint8_t entityCount = 0;
    const Monster* spectatorDefinition = getMonster(MONSTER_SPECTATOR);
    TEST_ASSERT_NOT_NULL(spectatorDefinition);

    useHitPointRoll(spectatorDefinition->hitDice);
    Entity* spectator = spawnMonster(
        entities, entityCount, MONSTER_SPECTATOR, 3, 4);

    TEST_ASSERT_NOT_NULL(spectator);
    TEST_ASSERT_EQUAL_PTR(spectatorDefinition, spectator->monster);
    TEST_ASSERT_EQUAL(MONSTER_SPECTATOR, spectator->monsterID);
    TEST_ASSERT_EQUAL_INT(
        spectatorDefinition->maxMP, spectator->character.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(
        spectatorDefinition->maxMP, spectator->character.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT8(
        spectatorDefinition->casterLevel, spectator->character.level);
    TEST_ASSERT_EQUAL(
        CREATURE_BEHOLDER, spectator->character.creatureType);
    TEST_ASSERT_EQUAL_UINT8(1, hitPointRollCount);
}

void test_skeleton_mage_spawns_from_normal_monster_data()
{
    Entity entities[MAX_ENTITIES] = {};
    uint8_t entityCount = 0;
    const Monster* definition = getMonster(MONSTER_SKELETON_MAGE);
    TEST_ASSERT_NOT_NULL(definition);

    useHitPointRoll(12);
    Entity* skeletonMage = spawnMonster(
        entities, entityCount, MONSTER_SKELETON_MAGE, 7, 8);

    TEST_ASSERT_NOT_NULL(skeletonMage);
    TEST_ASSERT_EQUAL_PTR(definition, skeletonMage->monster);
    TEST_ASSERT_EQUAL(MONSTER_SKELETON_MAGE, skeletonMage->monsterID);
    TEST_ASSERT_EQUAL_PTR(skeletonMage16x16, skeletonMage->sprite);
    TEST_ASSERT_EQUAL_UINT8(SPRITE_W, skeletonMage->spriteWidth);
    TEST_ASSERT_EQUAL_UINT8(SPRITE_H, skeletonMage->spriteHeight);
    TEST_ASSERT_EQUAL(TEAM_MONSTER, skeletonMage->character.team);
    TEST_ASSERT_EQUAL(STATE_ALIVE, skeletonMage->character.state);
    TEST_ASSERT_EQUAL(CREATURE_SKELETON, skeletonMage->character.creatureType);
    TEST_ASSERT_EQUAL_UINT16(24, skeletonMage->character.health.maxHP);
    TEST_ASSERT_EQUAL_UINT16(24, skeletonMage->character.health.currentHP);
    TEST_ASSERT_EQUAL_INT(8, skeletonMage->character.magic.maxMP);
    TEST_ASSERT_EQUAL_INT(8, skeletonMage->character.magic.currentMP);
    TEST_ASSERT_EQUAL_UINT8(3, skeletonMage->character.level);
    TEST_ASSERT_EQUAL_UINT8(2, skeletonMage->character.inventory.itemCount);
    TEST_ASSERT_EQUAL(
        ITEM_POTION_CURE_LIGHT_WOUNDS,
        skeletonMage->character.inventory.slots[0].item.itemID);
    TEST_ASSERT_EQUAL_UINT8(2, skeletonMage->character.inventory.slots[0].quantity);
    TEST_ASSERT_EQUAL(
        ITEM_MANA_POTION,
        skeletonMage->character.inventory.slots[1].item.itemID);
    TEST_ASSERT_EQUAL_UINT8(2, skeletonMage->character.inventory.slots[1].quantity);
    TEST_ASSERT_EQUAL(
        ITEM_SCYTHE,
        skeletonMage->character.equipment.equipped[SLOT_MELEE_WEAPON].itemID);
}

void test_invalid_monster_does_not_consume_entity_slot()
{
    Entity entities[MAX_ENTITIES] = {};
    uint8_t entityCount = 0;

    TEST_ASSERT_NULL(spawnMonster(
        entities, entityCount, MONSTER_NONE, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(0, entityCount);
    TEST_ASSERT_FALSE(entities[0].active);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_low_constitution_hp_never_wraps);
    RUN_TEST(test_zombie_database_fields_and_id_lookup_are_aligned);
    RUN_TEST(test_reused_entity_gets_one_fresh_monster_hp_roll);
    RUN_TEST(test_spellcaster_spawn_receives_definition_mp_pool);
    RUN_TEST(test_skeleton_mage_spawns_from_normal_monster_data);
    RUN_TEST(test_invalid_monster_does_not_consume_entity_slot);
    UNITY_END();
}

void loop()
{
}
