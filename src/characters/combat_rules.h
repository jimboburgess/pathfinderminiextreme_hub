#ifndef PATHFINDERMINIEXTREME_025_COMBAT_RULES_H
#define PATHFINDERMINIEXTREME_025_COMBAT_RULES_H

#include <stdint.h>

#include "character_types.h"
#include "items.h"

int calculateAbilityModifier(int score);
int calculateBaseAttackBonus(CharacterClass characterClass, uint8_t level);
int calculateBaseHitPoints(CharacterClass characterClass, uint8_t level);
int calculateBaseSave(CharacterClass characterClass,
                      SaveType saveType,
                      uint8_t level);
int calculateBoundedSave(CharacterClass characterClass,
                         SaveType saveType,
                         uint8_t level);

bool hasFighterFeatureAtLevel(CharacterClass characterClass,
                              uint8_t level,
                              FighterFeature feature);
bool isFighterTrainedWeaponGroup(CharacterClass characterClass,
                                 WeaponGroup trainedGroup,
                                 WeaponGroup weaponGroup);
int calculateFighterWeaponTrainingBonus(CharacterClass characterClass,
                                         uint8_t level);
int calculateFighterWeaponAttackBonus(CharacterClass characterClass,
                                       uint8_t level,
                                       WeaponGroup trainedGroup,
                                       WeaponGroup weaponGroup);
int calculateFighterWeaponDamageBonus(CharacterClass characterClass,
                                       uint8_t level,
                                       WeaponGroup trainedGroup,
                                       WeaponGroup weaponGroup);
int calculateFighterBonusMaxHP(CharacterClass characterClass, uint8_t level);

uint8_t calculateCriticalThreatMinimum(uint8_t baseThreatMinimum,
                                       bool improvedCritical,
                                       bool keen);
bool calculateAutomaticCriticalConfirmation(CharacterClass characterClass,
                                             uint8_t level,
                                             bool trainedWeapon);

int calculatePowerAttackPenalty(CharacterClass characterClass,
                                uint8_t level,
                                int baseAttackBonus);
int calculatePowerAttackDamageBonus(int powerAttackPenalty,
                                    WeaponType weaponType,
                                    uint32_t weaponProperties);

int calculateArmorClass(int dexterityModifier,
                        int armorBonus,
                        int shieldBonus,
                        int naturalArmor,
                        int deflectionBonus,
                        int dodgeBonus,
                        int sizeModifier);
int calculateTouchArmorClass(int dexterityModifier,
                             int deflectionBonus,
                             int dodgeBonus,
                             int sizeModifier,
                             int conditionModifier);
int calculateAttackBonus(int baseAttackBonus,
                         int abilityModifier,
                         int itemBonus,
                         int classBonus,
                         int conditionModifier);
int calculateSavingThrowBonus(int baseSave,
                              int abilityModifier,
                              int conditionModifier = 0);
int calculateItemWeaponAttackBonus(bool validWeapon,
                                   const ItemInstance& item);
int calculateItemWeaponDamageBonus(bool validWeapon,
                                   const ItemInstance& item);
uint8_t calculateIterativeAttackCount(int baseAttackBonus);
int calculateIterativeAttackBAB(int baseAttackBonus, uint8_t attackIndex);
int calculateSequenceAttackBAB(int baseAttackBonus,
                               uint8_t attackIndex,
                               uint8_t bonusAttackCount);

struct AttackRollResult
{
    int attackTotal = 0;
    int confirmationTotal = 0;
    bool hit = false;
    bool criticalThreat = false;
    bool criticalConfirmed = false;
};

AttackRollResult resolveAttackRoll(int attackRoll,
                                   int totalAttackBonus,
                                   int targetArmorClass,
                                   uint8_t threatMinimum,
                                   bool automaticConfirmation,
                                   int confirmationRoll = 0);

struct PhysicalDamageInput
{
    int rolledWeaponDamage = 0;
    int abilityModifier = 0;
    int itemBonus = 0;
    int classBonus = 0;
    int conditionBonus = 0;
    int powerAttackBonus = 0;
    uint8_t criticalMultiplier = 1;
    bool criticalConfirmed = false;
};

int calculatePhysicalWeaponDamage(const PhysicalDamageInput& input);

class SeededCombatRandom
{
public:
    explicit SeededCombatRandom(uint32_t seed);

    uint32_t next();
    int rollDie(int sides);

private:
    uint32_t state_;
};

#endif
