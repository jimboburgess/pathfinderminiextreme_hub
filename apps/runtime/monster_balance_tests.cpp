#include "characters/characters.h"
#include "characters/combat_rules.h"
#include "characters/items.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/entitytraits.h"
#include "dungeon/dungeon.h"
#include "dungeon/monsters.h"

#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace
{
enum class MonsterRole : uint8_t
{
    FODDER,
    NORMAL,
    TOUGH,
    ELITE,
    BOSS
};

struct BalanceExpectation
{
    MonsterID id;
    MonsterRole role;
    uint8_t minimumLevel;
    uint8_t maximumLevel;
};

constexpr BalanceExpectation expectations[] =
{
    {MONSTER_GOBLIN_SCIMITAR, MonsterRole::FODDER, 1, 2},
    {MONSTER_GOBLIN_ARCHER, MonsterRole::NORMAL, 1, 2},
    {MONSTER_BUGBEAR, MonsterRole::TOUGH, 1, 3},
    {MONSTER_SKELETON, MonsterRole::NORMAL, 1, 2},
    {MONSTER_ZOMBIE, MonsterRole::FODDER, 1, 2},
    {MONSTER_GHOUL, MonsterRole::NORMAL, 1, 3},
    {MONSTER_WIGHT, MonsterRole::TOUGH, 2, 4},
    {MONSTER_GIANT_SPIDER, MonsterRole::TOUGH, 1, 3},
    {MONSTER_GRAY_OOZE, MonsterRole::NORMAL, 1, 3},
    {MONSTER_VIOLET_FUNGUS, MonsterRole::NORMAL, 2, 4},
    {MONSTER_CHOKER, MonsterRole::TOUGH, 2, 4},
    {MONSTER_SPECTATOR, MonsterRole::BOSS, 3, 5},
    {MONSTER_SKELETON_MAGE, MonsterRole::ELITE, 2, 4},
    {MONSTER_GOBLIN_CHIEFTAIN, MonsterRole::BOSS, 1, 3},
    {MONSTER_GIANT_SPIDER_QUEEN, MonsterRole::BOSS, 2, 4}
};

static_assert(sizeof(expectations) / sizeof(expectations[0]) ==
              MONSTER_COUNT - 1,
              "Every production monster needs balance metadata");

void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

const char* roleName(MonsterRole role)
{
    switch (role)
    {
        case MonsterRole::FODDER: return "Fodder";
        case MonsterRole::NORMAL: return "Normal";
        case MonsterRole::TOUGH: return "Tough";
        case MonsterRole::ELITE: return "Elite";
        case MonsterRole::BOSS: return "Boss";
    }
    return "Unknown";
}

const char* challengeName(ChallengeRating challenge)
{
    switch (challenge)
    {
        case CR_ONE_EIGHTH: return "1/8";
        case CR_ONE_QUARTER: return "1/4";
        case CR_ONE_THIRD: return "1/3";
        case CR_ONE_HALF: return "1/2";
        case CR_ONE: return "1";
        case CR_TWO: return "2";
        case CR_THREE: return "3";
        case CR_FOUR: return "4";
        case CR_FIVE: return "5";
        default: return "?";
    }
}

int hitPercent(int attackBonus, int armorClass)
{
    int hits = 0;
    for (int roll = 1; roll <= 20; ++roll)
        if (resolveAttackRoll(roll, attackBonus, armorClass, 21, false).hit)
            ++hits;
    return hits * 5;
}

Character attacker(CharacterClass characterClass,
                   uint8_t level,
                   int strength,
                   int dexterity,
                   ItemID meleeWeapon,
                   ItemID rangedWeapon,
                   WeaponGroup trainedGroup = WEAPON_GROUP_NONE,
                   int8_t enhancement = 0)
{
    Character character{};
    character.characterClass = characterClass;
    character.level = level;
    character.abilities.strength = strength;
    character.abilities.dexterity = dexterity;
    setFighterTrainedWeaponGroup(character, trainedGroup);
    character.equipment.equipped[SLOT_MELEE_WEAPON] =
        makeItemInstance(meleeWeapon);
    character.equipment.equipped[SLOT_RANGED_WEAPON] =
        makeItemInstance(rangedWeapon);
    character.equipment.equipped[SLOT_MELEE_WEAPON].enhancementBonus =
        enhancement;
    character.equipment.equipped[SLOT_RANGED_WEAPON].enhancementBonus =
        enhancement;
    return character;
}

int monsterAttackBonus(const Monster& monster)
{
    const Weapon* weapon = getWeapon(monster.weapon);
    if (weapon == nullptr) return monster.baseAttack;
    const int abilityScore = weapon->type == WEAPON_RANGED
        ? monster.abilities.dexterity
        : monster.abilities.strength;
    return monster.baseAttack + getAbilityModifier(abilityScore);
}

int monsterAverageDamageTimesTwo(const Monster& monster)
{
    const Weapon* weapon = getWeapon(monster.weapon);
    if (weapon == nullptr) return 0;
    int damageTimesTwo = weapon->damageDice * (weapon->damageSides + 1);
    if (weapon->type == WEAPON_MELEE)
        damageTimesTwo += 2 * getAbilityModifier(monster.abilities.strength);
    return damageTimesTwo < 2 ? 2 : damageTimesTwo;
}

int monsterAverageHPTimesTwo(const Monster& monster)
{
    const int constitutionModifier = monster.abilities.constitution == 0
        ? 0
        : getAbilityModifier(monster.abilities.constitution);
    const int averageTimesTwo = monster.hitDice * 9 +
                                monster.hitDice * constitutionModifier * 2;
    return averageTimesTwo < monster.hitDice * 2
        ? monster.hitDice * 2
        : averageTimesTwo;
}

int oldEquipmentArmorClass(MonsterID id)
{
    Entity monster{};
    check(initializeMonsterDefinitionState(monster, id),
          "monster initialization");
    return getArmorClass(monster.character);
}

int roleArmorClassCap(MonsterRole role)
{
    switch (role)
    {
        case MonsterRole::FODDER: return 13;
        case MonsterRole::NORMAL: return 14;
        case MonsterRole::TOUGH: return 16;
        case MonsterRole::ELITE:
        case MonsterRole::BOSS: return 17;
    }
    return 17;
}

void checkEncounter(const MonsterID* monsters,
                    size_t count,
                    int attackBonus,
                    int minimumHitPercent)
{
    for (size_t index = 0; index < count; ++index)
    {
        const Monster* monster = getMonster(monsters[index]);
        check(monster != nullptr, "encounter monster exists");
        check(hitPercent(attackBonus, monster->armorClass) >= minimumHitPercent,
              "low-level encounter hit chance");
    }
}

void printHalfValue(int valueTimesTwo)
{
    std::cout << valueTimesTwo / 2;
    if (valueTimesTwo % 2 != 0) std::cout << ".5";
}
}

int main()
{
    try
    {
        const Character fighter1 = attacker(
            CLASS_FIGHTER, 1, 14, 12, ITEM_LONGSWORD, ITEM_SHORTBOW,
            WEAPON_GROUP_BLADES);
        const Character fighter2 = attacker(
            CLASS_FIGHTER, 2, 14, 12, ITEM_LONGSWORD, ITEM_SHORTBOW,
            WEAPON_GROUP_BLADES);
        const Character rogue1 = attacker(
            CLASS_ROGUE, 1, 14, 16, ITEM_DAGGER, ITEM_LIGHT_CROSSBOW);
        const Character rogue2 = attacker(
            CLASS_ROGUE, 2, 14, 16, ITEM_DAGGER, ITEM_LIGHT_CROSSBOW);
        const Character wizard1 = attacker(
            CLASS_WIZARD, 1, 12, 14, ITEM_QUARTERSTAFF, ITEM_SHORTBOW);
        const Character wizard2 = attacker(
            CLASS_WIZARD, 2, 12, 14, ITEM_QUARTERSTAFF, ITEM_SHORTBOW);
        const Character cleric1 = attacker(
            CLASS_CLERIC, 1, 14, 12, ITEM_MACE, ITEM_LIGHT_CROSSBOW);
        const Character cleric2 = attacker(
            CLASS_CLERIC, 2, 14, 12, ITEM_MACE, ITEM_LIGHT_CROSSBOW);
        const Character fighter3 = attacker(
            CLASS_FIGHTER, 3, 14, 12, ITEM_LONGSWORD, ITEM_SHORTBOW,
            WEAPON_GROUP_BLADES);
        const Character fighter5 = attacker(
            CLASS_FIGHTER, 5, 16, 12, ITEM_LONGSWORD, ITEM_SHORTBOW,
            WEAPON_GROUP_BLADES, 1);

        check(getMeleeAttackBonus(fighter1) == 3, "level one fighter bonus");
        check(getMeleeAttackBonus(fighter2) == 5, "level two fighter bonus");
        check(getMeleeAttackBonus(rogue1) == 2 &&
              getRangedAttackBonus(rogue1) == 3,
              "level one rogue bonuses");
        check(getMeleeAttackBonus(rogue2) == 3 &&
              getRangedAttackBonus(rogue2) == 4,
              "level two rogue bonuses");
        check(getMeleeAttackBonus(wizard1) == 1 &&
              getRangedAttackBonus(wizard1) == 2,
              "level one wizard bonuses");
        check(getMeleeAttackBonus(wizard2) == 2 &&
              getRangedAttackBonus(wizard2) == 3,
              "level two wizard bonuses");
        check(getMeleeAttackBonus(cleric1) == 2 &&
              getRangedAttackBonus(cleric1) == 1,
              "level one cleric bonuses");
        check(getMeleeAttackBonus(cleric2) == 3 &&
              getRangedAttackBonus(cleric2) == 2,
              "level two cleric bonuses");
        check(getMeleeAttackBonus(fighter3) == 6, "level three fighter bonus");
        check(getMeleeAttackBonus(fighter5) == 11, "level five fighter bonus");

        check(hitPercent(3, 12) == 60, "plus three versus AC twelve");
        check(hitPercent(3, 18) == 30, "plus three versus AC eighteen");
        check(hitPercent(20, 40) == 5, "natural twenty always hits");
        check(hitPercent(40, 1) == 95, "natural one always misses");

        std::cout << "Monster | CR | Role | Levels | AC(old-effective->new) | "
                     "AvgHP | Atk | AvgDmg | L1F% | L2F% | Saves\n";

        bool seen[MONSTER_COUNT] = {};
        for (const BalanceExpectation& expectation : expectations)
        {
            check(!seen[expectation.id], "duplicate monster balance row");
            seen[expectation.id] = true;
            const Monster* monster = getMonster(expectation.id);
            check(monster != nullptr, "balance monster exists");
            check(monster->armorClass <= roleArmorClassCap(expectation.role),
                  "monster exceeds role AC cap");
            check(monsterAttackBonus(*monster) <= 7,
                  "monster attack bonus outlier");
            check(monster->fortitude <= 8 && monster->reflex <= 8 &&
                  monster->will <= 8,
                  "monster save outlier");

            Entity entity{};
            entity.type = ENTITY_MONSTER;
            check(initializeMonsterDefinitionState(entity, expectation.id),
                  "entity AC fixture");
            check(getEntityArmorClass(entity) == monster->armorClass,
                  "definition AC is authoritative");
            check(getEntityArmorClass(entity, 4) == monster->armorClass + 4,
                  "situational AC remains additive");

            std::cout << monster->name << " | "
                      << challengeName(monster->challengeRating) << " | "
                      << roleName(expectation.role) << " | "
                      << unsigned(expectation.minimumLevel) << '-'
                      << unsigned(expectation.maximumLevel) << " | "
                      << oldEquipmentArmorClass(expectation.id) << "->"
                      << unsigned(monster->armorClass) << " | ";
            printHalfValue(monsterAverageHPTimesTwo(*monster));
            std::cout << " | +" << monsterAttackBonus(*monster) << " | ";
            printHalfValue(monsterAverageDamageTimesTwo(*monster));
            std::cout << " | "
                      << hitPercent(getMeleeAttackBonus(fighter1),
                                    monster->armorClass)
                      << " | "
                      << hitPercent(getMeleeAttackBonus(fighter2),
                                    monster->armorClass)
                      << " | " << int(monster->fortitude) << '/'
                      << int(monster->reflex) << '/'
                      << int(monster->will) << '\n';
        }
        for (MonsterID id = MONSTER_GOBLIN_SCIMITAR;
             id < MONSTER_COUNT;
             id = static_cast<MonsterID>(id + 1))
            check(seen[id], "missing monster balance row");

        const MonsterID twoGoblins[] =
            {MONSTER_GOBLIN_SCIMITAR, MONSTER_GOBLIN_SCIMITAR};
        const MonsterID goblinArcher[] =
            {MONSTER_GOBLIN_SCIMITAR, MONSTER_GOBLIN_ARCHER};
        const MonsterID goblinBoss[] =
            {MONSTER_GOBLIN_SCIMITAR, MONSTER_GOBLIN_ARCHER,
             MONSTER_GOBLIN_CHIEFTAIN};
        const MonsterID skeletons[] =
            {MONSTER_SKELETON, MONSTER_SKELETON};
        const MonsterID zombies[] =
            {MONSTER_ZOMBIE, MONSTER_ZOMBIE};
        const MonsterID spiders[] = {MONSTER_GIANT_SPIDER};
        const MonsterID mixed[] =
            {MONSTER_GOBLIN_SCIMITAR, MONSTER_ZOMBIE,
             MONSTER_GIANT_SPIDER};
        const int fighter1Bonus = getMeleeAttackBonus(fighter1);
        checkEncounter(twoGoblins, 2, fighter1Bonus, 60);
        checkEncounter(goblinArcher, 2, fighter1Bonus, 55);
        checkEncounter(goblinBoss, 3, fighter1Bonus, 50);
        checkEncounter(skeletons, 2, fighter1Bonus, 55);
        checkEncounter(zombies, 2, fighter1Bonus, 70);
        checkEncounter(spiders, 1, fighter1Bonus, 50);
        checkEncounter(mixed, 3, fighter1Bonus, 50);

        check(getThemedMonster(ENCOUNTER_GOBLIN, 0) ==
                  MONSTER_GOBLIN_SCIMITAR &&
              getThemedMonster(ENCOUNTER_GOBLIN, 1) ==
                  MONSTER_GOBLIN_ARCHER,
              "goblin theme opens with low-level enemies");
        check(getThemedMonster(ENCOUNTER_UNDEAD, 0) == MONSTER_SKELETON &&
              getThemedMonster(ENCOUNTER_UNDEAD, 1) == MONSTER_ZOMBIE,
              "undead theme opens with low-level enemies");
        check(getThemedMonster(ENCOUNTER_ABERRATION, 0) ==
                  MONSTER_GRAY_OOZE &&
              getThemedMonster(ENCOUNTER_ABERRATION, 1) ==
                  MONSTER_VIOLET_FUNGUS,
              "aberration theme opens with hittable enemies");

        check(hitPercent(getMeleeAttackBonus(fighter3),
                         getMonster(MONSTER_SPECTATOR)->armorClass) == 55,
              "level three fighter versus spectator");
        check(hitPercent(getMeleeAttackBonus(fighter5),
                         getMonster(MONSTER_SPECTATOR)->armorClass) == 80,
              "level five progression remains meaningful");
        check(hitPercent(getMeleeAttackBonus(fighter5),
                         getMonster(MONSTER_GOBLIN_SCIMITAR)->armorClass) == 95,
              "higher-level fighter outgrows fodder");

        std::cout << "Profiles: L1 fighter +" << getMeleeAttackBonus(fighter1)
                  << ", focus reference +4, L2 fighter +"
                  << getMeleeAttackBonus(fighter2)
                  << ", L3 fighter +" << getMeleeAttackBonus(fighter3)
                  << ", L5 fighter +" << getMeleeAttackBonus(fighter5)
                  << "\nPASS monster AC, class profiles, encounter bands, "
                     "offense, saves, and progression\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
