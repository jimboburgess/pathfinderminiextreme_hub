#include "combat_rules.h"

const FighterFeatureGrant fighterFeatureProgression[] =
{
    {  1, FIGHTER_POWER_ATTACK },
    {  2, FIGHTER_WEAPON_FOCUS },
    {  3, FIGHTER_TOUGHNESS },
    {  4, FIGHTER_WEAPON_SPECIALIZATION },
    {  5, FIGHTER_WEAPON_TRAINING_1 },
    {  8, FIGHTER_GREATER_WEAPON_FOCUS },
    {  9, FIGHTER_WEAPON_TRAINING_2 },
    { 10, FIGHTER_IMPROVED_CRITICAL },
    { 12, FIGHTER_GREATER_WEAPON_SPECIALIZATION },
    { 13, FIGHTER_WEAPON_TRAINING_3 },
    { 14, FIGHTER_GREATER_TOUGHNESS },
    { 17, FIGHTER_WEAPON_TRAINING_4 },
    { 20, FIGHTER_WEAPON_MASTERY }
};

const uint8_t fighterFeatureProgressionCount =
    sizeof(fighterFeatureProgression) / sizeof(fighterFeatureProgression[0]);

namespace
{
const uint8_t highBAB[MAX_CHARACTER_LEVEL] =
{
    1,2,3,4,5,
    6,7,8,9,10,
    11,12,13,14,15,
    16,17,18,19,20
};

const uint8_t mediumBAB[MAX_CHARACTER_LEVEL] =
{
    0,1,2,3,3,
    4,5,6,6,7,
    8,9,9,10,11,
    12,12,13,14,15
};

const uint8_t lowBAB[MAX_CHARACTER_LEVEL] =
{
    0,1,1,2,2,
    3,3,4,4,5,
    5,6,6,7,7,
    8,8,9,9,10
};

const uint8_t fighterHP[MAX_CHARACTER_LEVEL] =
{
    10,15,21,26,32,
    37,43,48,54,59,
    65,70,76,81,87,
    92,98,103,109,114
};

const uint8_t rogueHP[MAX_CHARACTER_LEVEL] =
{
    6,9,13,16,20,
    23,27,30,34,37,
    41,44,48,51,55,
    58,62,65,69,72
};

const uint8_t wizardHP[MAX_CHARACTER_LEVEL] =
{
    4,6,9,11,14,
    16,19,21,24,26,
    29,31,34,36,39,
    41,44,46,49,51
};

const uint8_t clericHP[MAX_CHARACTER_LEVEL] =
{
    8,12,17,21,26,
    30,35,39,44,48,
    53,57,62,66,71,
    75,80,84,89,93
};

uint8_t boundedLevel(uint8_t level)
{
    if (level < 1)
        return 1;
    return level > MAX_CHARACTER_LEVEL ? MAX_CHARACTER_LEVEL : level;
}
}

int calculateAbilityModifier(int score)
{
    return score >= 10 ? (score - 10) / 2 : (score - 11) / 2;
}

int calculateBaseAttackBonus(CharacterClass characterClass, uint8_t level)
{
    level = boundedLevel(level);
    switch (characterClass)
    {
        case CLASS_FIGHTER: return highBAB[level - 1];
        case CLASS_ROGUE:
        case CLASS_CLERIC: return mediumBAB[level - 1];
        case CLASS_WIZARD: return lowBAB[level - 1];
    }
    return 0;
}

int calculateBaseHitPoints(CharacterClass characterClass, uint8_t level)
{
    level = boundedLevel(level);
    switch (characterClass)
    {
        case CLASS_FIGHTER: return fighterHP[level - 1];
        case CLASS_ROGUE: return rogueHP[level - 1];
        case CLASS_WIZARD: return wizardHP[level - 1];
        case CLASS_CLERIC: return clericHP[level - 1];
    }
    return 0;
}

int calculateBaseSave(CharacterClass characterClass,
                      SaveType saveType,
                      uint8_t level)
{
    if (saveType == SAVE_NONE)
        return 0;

    bool goodSave = false;
    switch (characterClass)
    {
        case CLASS_FIGHTER:
            goodSave = saveType == SAVE_FORTITUDE;
            break;
        case CLASS_ROGUE:
            goodSave = saveType == SAVE_REFLEX;
            break;
        case CLASS_WIZARD:
            goodSave = saveType == SAVE_WILL;
            break;
        case CLASS_CLERIC:
            goodSave = saveType == SAVE_FORTITUDE || saveType == SAVE_WILL;
            break;
    }
    return goodSave ? 2 + level / 2 : level / 3;
}

int calculateBoundedSave(CharacterClass characterClass,
                         SaveType saveType,
                         uint8_t level)
{
    return calculateBaseSave(characterClass, saveType, boundedLevel(level));
}

bool hasFighterFeatureAtLevel(CharacterClass characterClass,
                              uint8_t level,
                              FighterFeature feature)
{
    if (characterClass != CLASS_FIGHTER)
        return false;

    for (uint8_t i = 0; i < fighterFeatureProgressionCount; ++i)
    {
        if (fighterFeatureProgression[i].feature == feature)
            return level >= fighterFeatureProgression[i].level;
    }
    return false;
}

bool isFighterTrainedWeaponGroup(CharacterClass characterClass,
                                 WeaponGroup trainedGroup,
                                 WeaponGroup weaponGroup)
{
    return characterClass == CLASS_FIGHTER &&
           trainedGroup != WEAPON_GROUP_NONE &&
           trainedGroup == weaponGroup;
}

int calculateFighterWeaponTrainingBonus(CharacterClass characterClass,
                                         uint8_t level)
{
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_WEAPON_TRAINING_4)) return 4;
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_WEAPON_TRAINING_3)) return 3;
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_WEAPON_TRAINING_2)) return 2;
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_WEAPON_TRAINING_1)) return 1;
    return 0;
}

int calculateFighterWeaponAttackBonus(CharacterClass characterClass,
                                       uint8_t level,
                                       WeaponGroup trainedGroup,
                                       WeaponGroup weaponGroup)
{
    if (!isFighterTrainedWeaponGroup(
            characterClass, trainedGroup, weaponGroup)) return 0;

    int bonus = calculateFighterWeaponTrainingBonus(characterClass, level);
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_WEAPON_FOCUS)) ++bonus;
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_GREATER_WEAPON_FOCUS)) ++bonus;
    return bonus;
}

int calculateFighterWeaponDamageBonus(CharacterClass characterClass,
                                       uint8_t level,
                                       WeaponGroup trainedGroup,
                                       WeaponGroup weaponGroup)
{
    if (!isFighterTrainedWeaponGroup(
            characterClass, trainedGroup, weaponGroup)) return 0;

    int bonus = calculateFighterWeaponTrainingBonus(characterClass, level);
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_WEAPON_SPECIALIZATION)) bonus += 2;
    if (hasFighterFeatureAtLevel(
            characterClass, level,
            FIGHTER_GREATER_WEAPON_SPECIALIZATION)) bonus += 2;
    return bonus;
}

int calculateFighterBonusMaxHP(CharacterClass characterClass, uint8_t level)
{
    int bonus = 0;
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_TOUGHNESS)) bonus += level;
    if (hasFighterFeatureAtLevel(
            characterClass, level, FIGHTER_GREATER_TOUGHNESS)) bonus += level;
    return bonus;
}

uint8_t calculateCriticalThreatMinimum(uint8_t baseThreatMinimum,
                                       bool improvedCritical,
                                       bool keen)
{
    if (!improvedCritical && !keen)
        return baseThreatMinimum;

    const int naturalThreatCount = 21 - baseThreatMinimum;
    int doubledMinimum = 21 - naturalThreatCount * 2;
    if (doubledMinimum < 2)
        doubledMinimum = 2;
    return static_cast<uint8_t>(doubledMinimum);
}

bool calculateAutomaticCriticalConfirmation(CharacterClass characterClass,
                                             uint8_t level,
                                             bool trainedWeapon)
{
    return trainedWeapon && hasFighterFeatureAtLevel(
        characterClass, level, FIGHTER_WEAPON_MASTERY);
}

int calculatePowerAttackPenalty(CharacterClass characterClass,
                                uint8_t level,
                                int baseAttackBonus)
{
    if (characterClass != CLASS_FIGHTER || level == 0 ||
        baseAttackBonus < 1)
    {
        return 0;
    }
    return -(1 + baseAttackBonus / 4);
}

int calculatePowerAttackDamageBonus(int powerAttackPenalty,
                                    WeaponType weaponType,
                                    uint32_t weaponProperties)
{
    if (weaponType != WEAPON_MELEE || powerAttackPenalty >= 0)
        return 0;
    const int multiplier =
        (weaponProperties & WEAPON_PROP_TWO_HANDED) != 0 ? 3 : 2;
    return -powerAttackPenalty * multiplier;
}

int calculateArmorClass(int dexterityModifier,
                        int armorBonus,
                        int shieldBonus,
                        int naturalArmor,
                        int deflectionBonus,
                        int dodgeBonus,
                        int sizeModifier)
{
    return 10 + armorBonus + shieldBonus + dexterityModifier +
           naturalArmor + deflectionBonus + dodgeBonus + sizeModifier;
}

int calculateTouchArmorClass(int dexterityModifier,
                             int deflectionBonus,
                             int dodgeBonus,
                             int sizeModifier,
                             int conditionModifier)
{
    return 10 + dexterityModifier + deflectionBonus + dodgeBonus +
           sizeModifier + conditionModifier;
}

int calculateAttackBonus(int baseAttackBonus,
                         int abilityModifier,
                         int itemBonus,
                         int classBonus,
                         int conditionModifier)
{
    return baseAttackBonus + abilityModifier + itemBonus + classBonus +
           conditionModifier;
}

int calculateSavingThrowBonus(int baseSave,
                              int abilityModifier,
                              int conditionModifier)
{
    return baseSave + abilityModifier + conditionModifier;
}

int calculateItemWeaponAttackBonus(bool validWeapon,
                                   const ItemInstance& item)
{
    if (!validWeapon)
        return 0;
    if (item.enhancementBonus > 0)
        return item.enhancementBonus;
    return hasWeaponProperty(item, WEAPON_PROPERTY_MASTERWORK) ? 1 : 0;
}

int calculateItemWeaponDamageBonus(bool validWeapon,
                                   const ItemInstance& item)
{
    return validWeapon && item.enhancementBonus > 0
        ? item.enhancementBonus : 0;
}

uint8_t calculateIterativeAttackCount(int baseAttackBonus)
{
    if (baseAttackBonus < 1)
        return 1;
    const int count = 1 + (baseAttackBonus - 1) / 5;
    return static_cast<uint8_t>(count > 4 ? 4 : count);
}

int calculateIterativeAttackBAB(int baseAttackBonus, uint8_t attackIndex)
{
    return baseAttackBonus - static_cast<int>(attackIndex) * 5;
}

int calculateSequenceAttackBAB(int baseAttackBonus,
                               uint8_t attackIndex,
                               uint8_t bonusAttackCount)
{
    return attackIndex < bonusAttackCount
        ? baseAttackBonus
        : calculateIterativeAttackBAB(
            baseAttackBonus, attackIndex - bonusAttackCount);
}

AttackRollResult resolveAttackRoll(int attackRoll,
                                   int totalAttackBonus,
                                   int targetArmorClass,
                                   uint8_t threatMinimum,
                                   bool automaticConfirmation,
                                   int confirmationRoll)
{
    AttackRollResult result;
    result.attackTotal = attackRoll + totalAttackBonus;
    result.hit = attackRoll == 20 ||
        (attackRoll != 1 && result.attackTotal >= targetArmorClass);
    result.criticalThreat = result.hit && attackRoll >= threatMinimum;
    if (!result.criticalThreat)
        return result;

    if (automaticConfirmation)
    {
        result.criticalConfirmed = true;
        return result;
    }

    if (confirmationRoll < 1 || confirmationRoll > 20)
        return result;

    result.confirmationTotal = confirmationRoll + totalAttackBonus;
    result.criticalConfirmed = confirmationRoll == 20 ||
        (confirmationRoll != 1 &&
         result.confirmationTotal >= targetArmorClass);
    return result;
}

int calculatePhysicalWeaponDamage(const PhysicalDamageInput& input)
{
    int damage = input.rolledWeaponDamage + input.abilityModifier +
                 input.itemBonus + input.classBonus + input.conditionBonus +
                 input.powerAttackBonus;
    if (damage < 1)
        damage = 1;
    if (input.criticalConfirmed)
        damage *= input.criticalMultiplier;
    return damage;
}

SeededCombatRandom::SeededCombatRandom(uint32_t seed)
    : state_(seed == 0 ? 0x6D2B79F5UL : seed)
{
}

uint32_t SeededCombatRandom::next()
{
    uint32_t value = state_;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state_ = value;
    return value;
}

int SeededCombatRandom::rollDie(int sides)
{
    return sides > 0 ? static_cast<int>(next() % sides) + 1 : 0;
}
