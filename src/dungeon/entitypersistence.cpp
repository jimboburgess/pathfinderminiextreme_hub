#include "dungeon/entitypersistence.h"

#include <new>

#include "data/entityspawn.h"
#include "dungeon/dungeon.h"
#include "graphics/npcsprites.h"
#include "graphics/tiles.h"

namespace
{
void clearPersistentEntity(PersistentEntity& entity)
{
    entity.~PersistentEntity();
    new (&entity) PersistentEntity{};
}

bool validPersistentCoordinates(const Entity& entity)
{
    return entity.x < ROOM_WIDTH && entity.y < ROOM_HEIGHT;
}

bool hasLoot(const LootData& loot)
{
    return loot.generated || loot.itemCount != 0 || loot.gold != 0;
}

bool hasConditions(const ConditionData& conditions)
{
    return conditions.count != 0 || conditions.timedDamageCount != 0 ||
           conditions.energyResistanceCount != 0 ||
           conditions.energyProtectionCount != 0 ||
           conditions.poison.type != POISON_NONE;
}

bool hasInventoryOutsideCount(const InventoryData& inventory)
{
    if (inventory.itemCount > MAX_INVENTORY) return true;
    for (uint8_t i = inventory.itemCount; i < MAX_INVENTORY; ++i)
        if (inventory.slots[i].item.itemID != ITEM_NONE ||
            inventory.slots[i].quantity != 0) return true;
    return false;
}

bool sameAbilityScores(const AbilityScores& left, const AbilityScores& right)
{
    return left.strength == right.strength &&
           left.dexterity == right.dexterity &&
           left.constitution == right.constitution &&
           left.intelligence == right.intelligence &&
           left.wisdom == right.wisdom && left.charisma == right.charisma;
}

bool sameEquipment(const EquipmentData& left, const EquipmentData& right)
{
    for (uint8_t i = 0; i < NUM_EQUIPMENT_SLOTS; ++i)
        if (left.equipped[i] != right.equipped[i]) return false;
    return true;
}

bool knownAbilitySlotsAreEmpty(const MagicData& magic)
{
    for (uint8_t i = 0; i < MAX_KNOWN_ABILITIES; ++i)
        if (magic.knownAbilities[i] != ABILITY_NONE) return false;
    return true;
}

bool hasNonPersistentCharacterPayload(const Character& character)
{
    if (character.name.length() != 0 || character.level != 0 ||
        character.xp != 0 || character.speed != 0 ||
        character.initiative != 0 ||
        character.health.currentHP != 0 ||
        character.health.maxHP != 0 || hasConditions(character.conditions) ||
        character.magic.currentMP != 0 || character.magic.maxMP != 0 ||
        character.magic.knownAbilityCount != 0 ||
        character.magic.arcaneCaster || character.magic.divineCaster ||
        character.magic.learning.active ||
        character.magic.learning.ability != ABILITY_NONE ||
        character.magic.learning.restsRemaining != 0 ||
        character.classAbilities.channelEnergyCurrent != 0 ||
        character.classAbilities.channelEnergyMax != 0 ||
        character.inventory.itemCount != 0 ||
        character.inventory.gold != 0)
        return true;

    const AbilityScores emptyScores{};
    const EquipmentData emptyEquipment{};
    if (!sameAbilityScores(character.abilities, emptyScores) ||
        !sameEquipment(character.equipment, emptyEquipment) ||
        hasInventoryOutsideCount(character.inventory))
        return true;

    return !knownAbilitySlotsAreEmpty(character.magic);
}

bool monsterStaticStateIsRepresentable(const Entity& source)
{
    Entity expected{};
    if (!initializeMonsterDefinitionState(expected, source.monsterID))
        return false;
    const Character& actual = source.character;
    const Character& base = expected.character;
    return actual.team == base.team &&
           actual.characterClass == base.characterClass &&
           actual.creatureType == base.creatureType &&
           sameAbilityScores(actual.abilities, base.abilities) &&
           actual.speed == base.speed && actual.level == base.level &&
           actual.trainedWeaponGroup == base.trainedWeaponGroup &&
           actual.magic.maxMP == base.magic.maxMP &&
           sameEquipment(actual.equipment, base.equipment) &&
           actual.name.length() == 0 && actual.xp == 0 &&
           actual.magic.knownAbilityCount == 0 &&
           knownAbilitySlotsAreEmpty(actual.magic) &&
           !actual.magic.arcaneCaster && !actual.magic.divineCaster &&
           !actual.magic.learning.active &&
           actual.magic.learning.ability == ABILITY_NONE &&
           actual.magic.learning.restsRemaining == 0 &&
           actual.classAbilities.channelEnergyCurrent == 0 &&
           actual.classAbilities.channelEnergyMax == 0;
}

bool npcStaticStateIsRepresentable(const Entity& source)
{
    Entity expected{};
    return initializeNPCDefinitionState(expected, source.npcID) &&
           source.character.team == expected.character.team &&
           source.character.state == expected.character.state;
}

bool simpleEntityPayloadIsEmpty(const Entity& source)
{
    return !hasLoot(source.loot) && !source.locked && !source.opened &&
           !hasNonPersistentCharacterPayload(source.character) &&
           source.monsterID == MONSTER_NONE && source.monster == nullptr &&
           source.npcID == NPC_NONE;
}

void restoreCommon(const PersistentEntity& source, Entity& destination)
{
    destination = Entity{};
    destination.type = source.type;
    destination.x = source.x;
    destination.y = source.y;
    destination.active =
        (source.flags & PERSISTENT_ENTITY_ACTIVE) != 0;
}

bool packMonster(const Entity& source, PersistentEntity& destination)
{
    if (!monsterStaticStateIsRepresentable(source) ||
        source.npcID != NPC_NONE ||
        source.character.inventory.itemCount >
            MAX_PERSISTENT_MONSTER_ITEMS ||
        source.character.inventory.gold != 0 ||
        hasInventoryOutsideCount(source.character.inventory))
        return false;

    new (&destination.payload.monster) PersistentMonsterState{};
    PersistentMonsterState& result = destination.payload.monster;
    result.monsterID = source.monsterID;
    result.state = source.character.state;
    result.currentHP = source.character.health.currentHP;
    result.maxHP = source.character.health.maxHP;
    result.currentMP = source.character.magic.currentMP;
    result.conditions = source.character.conditions;
    result.itemCount = source.character.inventory.itemCount;
    for (uint8_t i = 0; i < result.itemCount; ++i)
        result.items[i] = source.character.inventory.slots[i];
    result.loot = source.loot;
    result.lastKnownX = source.lastKnownX;
    result.lastKnownY = source.lastKnownY;
    result.idleDirection = source.idleDirection;
    result.idleStepsRemaining = source.idleStepsRemaining;
    result.nextIdleActionTime = source.nextIdleActionTime;
    if (source.awareOfPlayer)
        destination.flags |= PERSISTENT_MONSTER_AWARE;
    if (source.revealedToPlayer)
        destination.flags |= PERSISTENT_MONSTER_REVEALED;
    if (source.hasLastKnownPosition)
        destination.flags |= PERSISTENT_MONSTER_HAS_LAST_KNOWN;
    return true;
}

bool inflateMonster(const PersistentEntity& source, Entity& destination)
{
    const PersistentMonsterState& state = source.payload.monster;
    if (state.itemCount > MAX_PERSISTENT_MONSTER_ITEMS ||
        !initializeMonsterDefinitionState(destination, state.monsterID))
        return false;
    destination.character.state = state.state;
    destination.character.health.currentHP = state.currentHP;
    destination.character.health.maxHP = state.maxHP;
    destination.character.magic.currentMP = state.currentMP;
    destination.character.conditions = state.conditions;
    destination.character.inventory = InventoryData{};
    destination.character.inventory.itemCount = state.itemCount;
    for (uint8_t i = 0; i < state.itemCount; ++i)
        destination.character.inventory.slots[i] = state.items[i];
    destination.loot = state.loot;
    destination.awareOfPlayer =
        (source.flags & PERSISTENT_MONSTER_AWARE) != 0;
    destination.revealedToPlayer =
        (source.flags & PERSISTENT_MONSTER_REVEALED) != 0;
    destination.hasLastKnownPosition =
        (source.flags & PERSISTENT_MONSTER_HAS_LAST_KNOWN) != 0;
    destination.lastKnownX = state.lastKnownX;
    destination.lastKnownY = state.lastKnownY;
    destination.idleDirection = state.idleDirection;
    destination.idleStepsRemaining = state.idleStepsRemaining;
    destination.nextIdleActionTime = state.nextIdleActionTime;
    return true;
}
}

bool packPersistentEntity(const Entity& source, PersistentEntity& destination)
{
    clearPersistentEntity(destination);
    if (source.type == ENTITY_PLAYER || !validPersistentCoordinates(source))
        return false;

    destination.type = source.type;
    destination.x = source.x;
    destination.y = source.y;
    if (source.active) destination.flags |= PERSISTENT_ENTITY_ACTIVE;

    bool packed = false;
    switch (source.type)
    {
        case ENTITY_MONSTER:
            packed = packMonster(source, destination);
            break;
        case ENTITY_CHEST:
            if (hasNonPersistentCharacterPayload(source.character) ||
                source.monsterID != MONSTER_NONE || source.monster != nullptr ||
                source.npcID != NPC_NONE)
                break;
            new (&destination.payload.chest) PersistentChestState{};
            destination.payload.chest.loot = source.loot;
            destination.payload.chest.locked = source.locked;
            destination.payload.chest.opened = source.opened;
            packed = true;
            break;
        case ENTITY_NPC:
            if (getNPCDefinition(source.npcID) == nullptr ||
                !npcStaticStateIsRepresentable(source) ||
                hasLoot(source.loot) || source.locked || source.opened ||
                hasNonPersistentCharacterPayload(source.character) ||
                source.monsterID != MONSTER_NONE || source.monster != nullptr)
                break;
            new (&destination.payload.npc) PersistentNPCState{};
            destination.payload.npc.npcID = source.npcID;
            packed = true;
            break;
        case ENTITY_RIDDLE_CAT:
            if (source.character.team != TEAM_NEUTRAL ||
                source.character.state != STATE_ALIVE)
                break;
            packed = simpleEntityPayloadIsEmpty(source);
            break;
        case ENTITY_PUZZLE_KEY:
        case ENTITY_LOOT:
            packed = simpleEntityPayloadIsEmpty(source);
            break;
        case ENTITY_NONE:
            packed = !source.active && simpleEntityPayloadIsEmpty(source);
            break;
        default:
            break;
    }
    if (!packed) clearPersistentEntity(destination);
    return packed;
}

bool inflatePersistentEntity(const PersistentEntity& source,
                             Entity& destination)
{
    destination = Entity{};
    if (source.x >= ROOM_WIDTH || source.y >= ROOM_HEIGHT ||
        source.type == ENTITY_PLAYER)
        return false;
    restoreCommon(source, destination);
    bool inflated = false;
    switch (source.type)
    {
        case ENTITY_MONSTER:
            inflated = inflateMonster(source, destination);
            break;
        case ENTITY_CHEST:
            destination.loot = source.payload.chest.loot;
            destination.locked = source.payload.chest.locked;
            destination.opened = source.payload.chest.opened;
            destination.sprite = destination.opened
                ? (destination.loot.itemCount > 0 || destination.loot.gold > 0
                    ? chestopenwith : chestopenwithout)
                : chestclosed;
            inflated = true;
            break;
        case ENTITY_NPC:
            inflated = initializeNPCDefinitionState(
                destination, source.payload.npc.npcID);
            break;
        case ENTITY_RIDDLE_CAT:
            destination.character.team = TEAM_NEUTRAL;
            destination.character.state = STATE_ALIVE;
            destination.sprite = bertramCat16x16;
            inflated = true;
            break;
        case ENTITY_PUZZLE_KEY:
        case ENTITY_LOOT:
        case ENTITY_NONE:
            inflated = true;
            break;
        default:
            break;
    }
    if (!inflated) destination = Entity{};
    return inflated;
}
