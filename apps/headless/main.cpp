#include <cstdint>
#include <iostream>

#include "characters/combat_rules.h"
#include "core/engine_version.h"
#include "dungeon/dungeontopology.h"
#include "multiplayer/network_protocol.h"

namespace
{
const char* characterClassName(CharacterClass characterClass)
{
    switch (characterClass)
    {
        case CLASS_FIGHTER: return "fighter";
        case CLASS_ROGUE: return "rogue";
        case CLASS_WIZARD: return "wizard";
        case CLASS_CLERIC: return "cleric";
    }
    return "unknown";
}
}

int main()
{
    std::cout << "Pathfinder Mini EXTREME headless\n"
              << "engine_api_version=" << PFME_ENGINE_API_VERSION << '\n'
              << "multiplayer_protocol_version="
              << static_cast<unsigned>(MULTIPLAYER_PROTOCOL_VERSION) << '\n'
              << "dungeon_generation_version="
              << DUNGEON_GENERATION_VERSION << '\n'
              << "modules=dungeon_topology,combat_rules,network_protocol,"
                 "snapshot_codec,riddles\n";

    DungeonTopology dungeon;
    DungeonGenerationReport generationReport;
    SeededDungeonRandom dungeonRandom(12345);
    if (!generateDungeonLayout(dungeon, dungeonRandom, generationReport))
    {
        std::cerr << "status=error dungeon_generation_failed\n";
        return 1;
    }

    const DungeonTopologyValidationError validation =
        validateDungeonLayoutDetailed(dungeon);
    if (validation != DUNGEON_TOPOLOGY_VALID)
    {
        std::cerr << "status=error dungeon_validation_failed reason="
                  << dungeonTopologyValidationErrorName(validation) << '\n';
        return 1;
    }

    const CharacterClass characterClass = CLASS_FIGHTER;
    const uint8_t characterLevel = 5;
    const int strengthModifier = calculateAbilityModifier(18);
    const int baseAttackBonus =
        calculateBaseAttackBonus(characterClass, characterLevel);
    ItemInstance equippedWeapon = makeItemInstance(ITEM_LONGSWORD);
    equippedWeapon.enhancementBonus = 1;
    const int attackBonus = calculateAttackBonus(
        baseAttackBonus,
        strengthModifier,
        calculateItemWeaponAttackBonus(true, equippedWeapon),
        calculateFighterWeaponAttackBonus(
            characterClass,
            characterLevel,
            WEAPON_GROUP_BLADES,
            WEAPON_GROUP_BLADES),
        0);
    const AttackRollResult attack =
        resolveAttackRoll(12, attackBonus, 18, 20, false);

    PhysicalDamageInput damageInput;
    damageInput.rolledWeaponDamage = 4;
    damageInput.abilityModifier = strengthModifier;
    damageInput.itemBonus =
        calculateItemWeaponDamageBonus(true, equippedWeapon);
    damageInput.classBonus = calculateFighterWeaponDamageBonus(
        characterClass,
        characterLevel,
        WEAPON_GROUP_BLADES,
        WEAPON_GROUP_BLADES);
    damageInput.criticalMultiplier = 2;
    damageInput.criticalConfirmed = attack.criticalConfirmed;
    const int damage = attack.hit
        ? calculatePhysicalWeaponDamage(damageInput) : 0;

    std::cout << "dungeon_seed=12345 rooms="
              << static_cast<unsigned>(dungeon.roomCount)
              << " boss_room=" << static_cast<unsigned>(dungeon.bossRoom)
              << " boss_distance="
              << static_cast<unsigned>(getRoomDistanceFromEntrance(
                     dungeon, dungeon.bossRoom))
              << " riddle_room="
              << static_cast<unsigned>(dungeon.riddleRoom)
              << " attempts="
              << static_cast<unsigned>(generationReport.attempts) << '\n'
              << "character_model=portable_rule_inputs class="
              << characterClassName(characterClass)
              << " level=" << static_cast<unsigned>(characterLevel)
              << " full_character_available=false\n"
              << "combat_roll=12 attack_bonus=" << attackBonus
              << " attack_total=" << attack.attackTotal
              << " target_ac=18 hit=" << (attack.hit ? "true" : "false")
              << " damage=" << damage << '\n'
              << "status=ok\n";
    return 0;
}
