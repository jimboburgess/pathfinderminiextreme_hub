#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "characters/combat_rules.h"

namespace
{
bool check(bool condition, const char* expression, const char* file, int line)
{
    if (condition)
        return true;
    std::cerr << file << ':' << line << ": check failed: " << expression
              << '\n';
    return false;
}

#define CHECK(expression) \
    do \
    { \
        if (!check((expression), #expression, __FILE__, __LINE__)) return false; \
    } while (false)

bool testAbilityModifiers()
{
    CHECK(calculateAbilityModifier(1) == -5);
    CHECK(calculateAbilityModifier(9) == -1);
    CHECK(calculateAbilityModifier(10) == 0);
    CHECK(calculateAbilityModifier(11) == 0);
    CHECK(calculateAbilityModifier(12) == 1);
    CHECK(calculateAbilityModifier(18) == 4);
    return true;
}

bool testClassProgression()
{
    CHECK(calculateBaseAttackBonus(CLASS_FIGHTER, 1) == 1);
    CHECK(calculateBaseAttackBonus(CLASS_FIGHTER, 20) == 20);
    CHECK(calculateBaseAttackBonus(CLASS_ROGUE, 8) == 6);
    CHECK(calculateBaseAttackBonus(CLASS_CLERIC, 20) == 15);
    CHECK(calculateBaseAttackBonus(CLASS_WIZARD, 12) == 6);
    CHECK(calculateBaseAttackBonus(CLASS_FIGHTER, 0) == 1);
    CHECK(calculateBaseAttackBonus(CLASS_FIGHTER, 255) == 20);
    CHECK(calculateBaseHitPoints(CLASS_FIGHTER, 1) == 10);
    CHECK(calculateBaseHitPoints(CLASS_FIGHTER, 20) == 114);
    CHECK(calculateBaseHitPoints(CLASS_WIZARD, 20) == 51);
    return true;
}

bool testSavingThrows()
{
    CHECK(calculateBaseSave(CLASS_FIGHTER, SAVE_NONE, 10) == 0);
    CHECK(calculateBaseSave(CLASS_FIGHTER, SAVE_FORTITUDE, 1) == 2);
    CHECK(calculateBaseSave(CLASS_FIGHTER, SAVE_REFLEX, 3) == 1);
    CHECK(calculateBoundedSave(CLASS_CLERIC, SAVE_FORTITUDE, 20) == 12);
    CHECK(calculateBoundedSave(CLASS_CLERIC, SAVE_WILL, 20) == 12);
    CHECK(calculateBoundedSave(CLASS_CLERIC, SAVE_REFLEX, 20) == 6);
    CHECK(calculateBoundedSave(CLASS_WIZARD, SAVE_WILL, 255) == 12);
    CHECK(calculateSavingThrowBonus(6, 3) == 9);
    CHECK(calculateSavingThrowBonus(6, 3, -2) == 7);
    return true;
}

bool testIterativeAttacks()
{
    CHECK(calculateIterativeAttackCount(0) == 1);
    CHECK(calculateIterativeAttackCount(5) == 1);
    CHECK(calculateIterativeAttackCount(6) == 2);
    CHECK(calculateIterativeAttackCount(11) == 3);
    CHECK(calculateIterativeAttackCount(16) == 4);
    CHECK(calculateIterativeAttackCount(25) == 4);
    CHECK(calculateIterativeAttackBAB(16, 3) == 1);
    CHECK(calculateSequenceAttackBAB(11, 0, 1) == 11);
    CHECK(calculateSequenceAttackBAB(11, 1, 1) == 11);
    CHECK(calculateSequenceAttackBAB(11, 2, 1) == 6);
    return true;
}

bool testFighterFeatureProgression()
{
    CHECK(hasFighterFeatureAtLevel(
        CLASS_FIGHTER, 1, FIGHTER_POWER_ATTACK));
    CHECK(!hasFighterFeatureAtLevel(
        CLASS_FIGHTER, 1, FIGHTER_WEAPON_FOCUS));
    CHECK(!hasFighterFeatureAtLevel(
        CLASS_ROGUE, 20, FIGHTER_WEAPON_MASTERY));
    CHECK(calculateFighterWeaponAttackBonus(
        CLASS_FIGHTER, 2, WEAPON_GROUP_BLADES, WEAPON_GROUP_BLADES) == 1);
    CHECK(calculateFighterWeaponAttackBonus(
        CLASS_FIGHTER, 8, WEAPON_GROUP_BLADES, WEAPON_GROUP_BLADES) == 3);
    CHECK(calculateFighterWeaponDamageBonus(
        CLASS_FIGHTER, 5, WEAPON_GROUP_BLADES, WEAPON_GROUP_BLADES) == 3);
    CHECK(calculateFighterWeaponDamageBonus(
        CLASS_FIGHTER, 12, WEAPON_GROUP_BLADES, WEAPON_GROUP_BLADES) == 6);
    CHECK(calculateFighterWeaponAttackBonus(
        CLASS_FIGHTER, 20, WEAPON_GROUP_BLADES, WEAPON_GROUP_AXES) == 0);
    CHECK(calculateFighterBonusMaxHP(CLASS_FIGHTER, 14) == 28);
    return true;
}

bool testEquipmentBonuses()
{
    ItemInstance ordinary = makeItemInstance(ITEM_LONGSWORD);
    ItemInstance masterwork = ordinary;
    masterwork.weaponProperties = WEAPON_PROPERTY_MASTERWORK;
    ItemInstance enhanced = ordinary;
    enhanced.enhancementBonus = 3;

    CHECK(calculateItemWeaponAttackBonus(true, ordinary) == 0);
    CHECK(calculateItemWeaponAttackBonus(true, masterwork) == 1);
    CHECK(calculateItemWeaponDamageBonus(true, masterwork) == 0);
    CHECK(calculateItemWeaponAttackBonus(true, enhanced) == 3);
    CHECK(calculateItemWeaponDamageBonus(true, enhanced) == 3);
    CHECK(calculateItemWeaponAttackBonus(false, enhanced) == 0);
    CHECK(calculateItemWeaponDamageBonus(false, enhanced) == 0);
    return true;
}

bool testArmorClassCalculations()
{
    CHECK(calculateArmorClass(3, 6, 2, 0, 0, 1, 0) == 22);
    CHECK(calculateTouchArmorClass(3, 0, 1, 0, -2) == 12);
    CHECK(calculateAttackBonus(6, 4, 1, 2, -1) == 12);
    CHECK(calculateAttackBonus(6, 3, 1, 2, -1) == 11);
    return true;
}

bool testNaturalAttackRollRules()
{
    const AttackRollResult naturalOne =
        resolveAttackRoll(1, 100, 10, 20, false, 20);
    CHECK(!naturalOne.hit);
    CHECK(!naturalOne.criticalThreat);

    const AttackRollResult naturalTwenty =
        resolveAttackRoll(20, -100, 99, 20, false, 20);
    CHECK(naturalTwenty.hit);
    CHECK(naturalTwenty.criticalThreat);
    CHECK(naturalTwenty.criticalConfirmed);
    return true;
}

bool testCriticalThreatAndConfirmation()
{
    AttackRollResult result = resolveAttackRoll(19, 8, 20, 19, false, 1);
    CHECK(result.hit);
    CHECK(result.criticalThreat);
    CHECK(!result.criticalConfirmed);
    CHECK(result.attackTotal == 27);
    CHECK(result.confirmationTotal == 9);

    result = resolveAttackRoll(19, 8, 20, 19, false, 12);
    CHECK(result.criticalConfirmed);
    CHECK(result.confirmationTotal == 20);

    result = resolveAttackRoll(19, 8, 20, 19, true);
    CHECK(result.criticalConfirmed);
    return true;
}

bool testImprovedCriticalAndKeen()
{
    CHECK(calculateCriticalThreatMinimum(20, false, false) == 20);
    CHECK(calculateCriticalThreatMinimum(19, true, false) == 17);
    CHECK(calculateCriticalThreatMinimum(18, false, true) == 15);
    CHECK(calculateCriticalThreatMinimum(18, true, true) == 15);
    CHECK(calculateAutomaticCriticalConfirmation(
        CLASS_FIGHTER, 20, true));
    CHECK(!calculateAutomaticCriticalConfirmation(
        CLASS_FIGHTER, 19, true));
    CHECK(!calculateAutomaticCriticalConfirmation(
        CLASS_FIGHTER, 20, false));
    return true;
}

bool testPowerAttack()
{
    CHECK(calculatePowerAttackPenalty(CLASS_ROGUE, 20, 15) == 0);
    CHECK(calculatePowerAttackPenalty(CLASS_FIGHTER, 0, 1) == 0);
    CHECK(calculatePowerAttackPenalty(CLASS_FIGHTER, 1, 1) == -1);
    CHECK(calculatePowerAttackPenalty(CLASS_FIGHTER, 20, 20) == -6);
    CHECK(calculatePowerAttackDamageBonus(-5, WEAPON_MELEE,
        WEAPON_PROP_NONE) == 10);
    CHECK(calculatePowerAttackDamageBonus(-5, WEAPON_MELEE,
        WEAPON_PROP_TWO_HANDED) == 15);
    CHECK(calculatePowerAttackDamageBonus(-5, WEAPON_RANGED,
        WEAPON_PROP_TWO_HANDED) == 0);
    return true;
}

bool testPhysicalDamage()
{
    PhysicalDamageInput damage;
    damage.rolledWeaponDamage = 4;
    damage.abilityModifier = 3;
    damage.itemBonus = 1;
    damage.classBonus = 2;
    damage.conditionBonus = -1;
    damage.powerAttackBonus = 4;
    damage.criticalMultiplier = 2;
    CHECK(calculatePhysicalWeaponDamage(damage) == 13);
    damage.criticalConfirmed = true;
    CHECK(calculatePhysicalWeaponDamage(damage) == 26);

    PhysicalDamageInput minimum;
    minimum.rolledWeaponDamage = 1;
    minimum.abilityModifier = -5;
    CHECK(calculatePhysicalWeaponDamage(minimum) == 1);
    minimum.criticalConfirmed = true;
    minimum.criticalMultiplier = 3;
    CHECK(calculatePhysicalWeaponDamage(minimum) == 3);
    return true;
}

bool testSeededRandomReplay()
{
    SeededCombatRandom first(12345);
    SeededCombatRandom second(12345);
    SeededCombatRandom different(54321);
    bool differs = false;
    for (int i = 0; i < 64; ++i)
    {
        const int firstRoll = first.rollDie(20);
        const int secondRoll = second.rollDie(20);
        const int differentRoll = different.rollDie(20);
        CHECK(firstRoll == secondRoll);
        CHECK(firstRoll >= 1 && firstRoll <= 20);
        if (firstRoll != differentRoll)
            differs = true;
    }
    CHECK(differs);
    CHECK(first.rollDie(0) == 0);
    return true;
}

struct TestCase
{
    const char* name;
    bool (*run)();
};

bool testFixedSeedSimulationReplay();

bool runSelfTests(bool quiet)
{
    const TestCase tests[] = {
        {"ability modifiers", testAbilityModifiers},
        {"class progression", testClassProgression},
        {"saving throws", testSavingThrows},
        {"iterative attacks", testIterativeAttacks},
        {"fighter features", testFighterFeatureProgression},
        {"equipment bonuses", testEquipmentBonuses},
        {"armor and attack bonuses", testArmorClassCalculations},
        {"natural attack rolls", testNaturalAttackRollRules},
        {"critical confirmation", testCriticalThreatAndConfirmation},
        {"improved critical and keen", testImprovedCriticalAndKeen},
        {"power attack", testPowerAttack},
        {"physical damage", testPhysicalDamage},
        {"seeded replay", testSeededRandomReplay},
        {"fixed-seed simulation", testFixedSeedSimulationReplay},
    };

    size_t failures = 0;
    for (const TestCase& test : tests)
    {
        if (!quiet)
            std::cout << "[ RUN      ] " << test.name << '\n';
        if (test.run())
        {
            if (!quiet)
                std::cout << "[       OK ] " << test.name << '\n';
        }
        else
        {
            std::cout << "[  FAILED  ] " << test.name << '\n';
            ++failures;
        }
    }

    if (!quiet)
    {
        std::cout << sizeof(tests) / sizeof(tests[0]) - failures
                  << " passed, " << failures << " failed\n";
    }
    return failures == 0;
}

struct SimulationStats
{
    uint64_t attempts = 0;
    uint64_t hits = 0;
    uint64_t criticalThreats = 0;
    uint64_t confirmedCriticals = 0;
    uint64_t naturalOnes = 0;
    uint64_t naturalTwenties = 0;
    uint64_t totalDamage = 0;
    uint32_t fingerprint = 2166136261UL;
};

void hashValue(SimulationStats& stats, uint32_t value)
{
    stats.fingerprint ^= value;
    stats.fingerprint *= 16777619UL;
}

SimulationStats simulateAttacks(uint32_t seed,
                                uint32_t attacks,
                                bool powerAttack)
{
    const Weapon longsword = {
        WEAPON_MELEE,
        DAMAGE_SLASHING,
        1,
        8,
        19,
        2,
        0,
        WEAPON_PROP_NONE,
        WEAPON_GROUP_BLADES
    };
    ItemInstance weaponItem = makeItemInstance(ITEM_LONGSWORD);
    weaponItem.enhancementBonus = 1;
    weaponItem.weaponProperties = WEAPON_PROPERTY_KEEN;

    const uint8_t fighterLevel = 5;
    const int baseAttackBonus =
        calculateBaseAttackBonus(CLASS_FIGHTER, fighterLevel);
    const int normalAttackBonus = calculateAttackBonus(
        baseAttackBonus,
        calculateAbilityModifier(18),
        calculateItemWeaponAttackBonus(true, weaponItem),
        calculateFighterWeaponAttackBonus(
            CLASS_FIGHTER,
            fighterLevel,
            WEAPON_GROUP_BLADES,
            longsword.group),
        0);
    const int powerAttackPenalty = powerAttack
        ? calculatePowerAttackPenalty(
            CLASS_FIGHTER, fighterLevel, baseAttackBonus)
        : 0;
    const int powerAttackDamage = powerAttack
        ? calculatePowerAttackDamageBonus(
            powerAttackPenalty, longsword.type, longsword.properties)
        : 0;
    const uint8_t threatMinimum = calculateCriticalThreatMinimum(
        longsword.criticalThreat,
        hasFighterFeatureAtLevel(
            CLASS_FIGHTER, fighterLevel, FIGHTER_IMPROVED_CRITICAL),
        hasWeaponProperty(weaponItem, WEAPON_PROPERTY_KEEN));
    const int targetArmorClass = 18;

    SeededCombatRandom random(seed);
    SimulationStats stats;
    for (uint32_t index = 0; index < attacks; ++index)
    {
        const int attackRoll = random.rollDie(20);
        const int confirmationRoll = random.rollDie(20);
        const int weaponDamageRoll = random.rollDie(longsword.damageSides);
        const AttackRollResult result = resolveAttackRoll(
            attackRoll,
            normalAttackBonus + powerAttackPenalty,
            targetArmorClass,
            threatMinimum,
            false,
            confirmationRoll);

        ++stats.attempts;
        if (attackRoll == 1) ++stats.naturalOnes;
        if (attackRoll == 20) ++stats.naturalTwenties;
        if (result.criticalThreat) ++stats.criticalThreats;
        if (result.criticalConfirmed) ++stats.confirmedCriticals;

        int damage = 0;
        if (result.hit)
        {
            ++stats.hits;
            PhysicalDamageInput input;
            input.rolledWeaponDamage = weaponDamageRoll;
            input.abilityModifier = calculateAbilityModifier(18);
            input.itemBonus = calculateItemWeaponDamageBonus(true, weaponItem);
            input.classBonus = calculateFighterWeaponDamageBonus(
                CLASS_FIGHTER,
                fighterLevel,
                WEAPON_GROUP_BLADES,
                longsword.group);
            input.powerAttackBonus = powerAttackDamage;
            input.criticalMultiplier = longsword.criticalMultiplier;
            input.criticalConfirmed = result.criticalConfirmed;
            damage = calculatePhysicalWeaponDamage(input);
            stats.totalDamage += static_cast<uint64_t>(damage);
        }

        hashValue(stats, static_cast<uint32_t>(attackRoll));
        hashValue(stats, static_cast<uint32_t>(confirmationRoll));
        hashValue(stats, static_cast<uint32_t>(weaponDamageRoll));
        hashValue(stats, static_cast<uint32_t>(damage));
    }
    return stats;
}

bool testFixedSeedSimulationReplay()
{
    const SimulationStats normal = simulateAttacks(12345, 10000, false);
    const SimulationStats normalReplay = simulateAttacks(12345, 10000, false);
    const SimulationStats powerAttack = simulateAttacks(12345, 10000, true);
    CHECK(normal.fingerprint == normalReplay.fingerprint);
    CHECK(normal.totalDamage == normalReplay.totalDamage);
    CHECK(normal.fingerprint == 0x06EBD918UL);
    CHECK(powerAttack.fingerprint == 0x9D46DD0AUL);
    CHECK(normal.hits == 7503);
    CHECK(powerAttack.hits == 6506);
    return true;
}

bool parseUint32(const char* text, uint32_t& value)
{
    if (text == nullptr || *text == '\0' || *text == '-')
        return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX)
        return false;
    value = static_cast<uint32_t>(parsed);
    return true;
}

void printHumanStats(const char* label, const SimulationStats& stats)
{
    const double attempts = static_cast<double>(stats.attempts);
    std::cout << label << ": attempts=" << stats.attempts
              << ", hits=" << stats.hits
              << ", misses=" << stats.attempts - stats.hits
              << ", threats=" << stats.criticalThreats
              << ", confirmed=" << stats.confirmedCriticals
              << ", natural_1=" << stats.naturalOnes
              << ", natural_20=" << stats.naturalTwenties
              << ", total_damage=" << stats.totalDamage
              << ", hit_rate=" << std::fixed << std::setprecision(4)
              << (attempts == 0.0 ? 0.0 : stats.hits / attempts)
              << ", average_damage="
              << (attempts == 0.0 ? 0.0 : stats.totalDamage / attempts)
              << ", fingerprint=0x" << std::hex << stats.fingerprint
              << std::dec << '\n';
}

void printJsonStats(const char* label,
                    const SimulationStats& stats,
                    bool trailingComma)
{
    const double attempts = static_cast<double>(stats.attempts);
    std::cout << "    \"" << label << "\": {"
              << "\"attempts\":" << stats.attempts
              << ",\"hits\":" << stats.hits
              << ",\"misses\":" << stats.attempts - stats.hits
              << ",\"critical_threats\":" << stats.criticalThreats
              << ",\"confirmed_criticals\":" << stats.confirmedCriticals
              << ",\"natural_ones\":" << stats.naturalOnes
              << ",\"natural_twenties\":" << stats.naturalTwenties
              << ",\"total_damage\":" << stats.totalDamage
              << ",\"hit_rate\":"
              << (attempts == 0.0 ? 0.0 : stats.hits / attempts)
              << ",\"average_damage\":"
              << (attempts == 0.0 ? 0.0 : stats.totalDamage / attempts)
              << ",\"fingerprint\":" << stats.fingerprint << "}"
              << (trailingComma ? "," : "") << '\n';
}
}

int main(int argc, char* argv[])
{
    uint32_t seed = 12345;
    uint32_t attacks = 10000;
    bool selfTestOnly = false;
    bool json = false;

    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp(argv[index], "--self-test") == 0)
            selfTestOnly = true;
        else if (std::strcmp(argv[index], "--json") == 0)
            json = true;
        else if ((std::strcmp(argv[index], "--seed") == 0 ||
                  std::strcmp(argv[index], "--attacks") == 0) &&
                 index + 1 < argc)
        {
            uint32_t parsed = 0;
            if (!parseUint32(argv[index + 1], parsed))
            {
                std::cerr << "Invalid value for " << argv[index] << '\n';
                return 2;
            }
            if (std::strcmp(argv[index], "--seed") == 0)
                seed = parsed;
            else
                attacks = parsed;
            ++index;
        }
        else
        {
            std::cerr << "Usage: pfme_combat_tests [--self-test] [--seed N] "
                         "[--attacks N] [--json]\n";
            return 2;
        }
    }

    if (attacks == 0)
    {
        std::cerr << "--attacks must be greater than zero\n";
        return 2;
    }

    if (!runSelfTests(json))
        return 1;
    if (selfTestOnly)
        return 0;

    const SimulationStats normal = simulateAttacks(seed, attacks, false);
    const SimulationStats powerAttack = simulateAttacks(seed, attacks, true);

    if (json)
    {
        std::cout << "{\n"
                  << "  \"seed\":" << seed << ",\n"
                  << "  \"paired_roll_streams\":true,\n"
                  << "  \"scenarios\": {\n";
        printJsonStats("normal", normal, true);
        printJsonStats("power_attack", powerAttack, false);
        std::cout << "  }\n}\n";
    }
    else
    {
        std::cout << "Simulation seed " << seed
                  << " using paired attack/confirmation/damage rolls\n";
        printHumanStats("normal", normal);
        printHumanStats("power_attack", powerAttack);
    }
    return 0;
}
