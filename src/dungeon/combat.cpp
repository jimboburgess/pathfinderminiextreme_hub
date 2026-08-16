#include "combat.h"

#include <algorithm>
#include <cstring>
#include <stdlib.h>

#include "activemap.h"
#include "abilityresolver.h"
#include "loot.h"
#include "monsterscripts.h"
#include "audio/audio.h"
#include "data/dice.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "data/progression.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "input/menu.h"

Combat combat;

static Entity* getPlayerCombatant()
{
    return getActiveMapPlayer();
}

static void requestCombatTileRedraw()
{
    // Closing the opaque menu schedules a full map redraw. Do not replace it
    // with a tile redraw before the menu-covered background has been restored.
    if (backgroundNeedsRedraw || redrawType == REDRAW_FULL)
    {
        needsRedraw = true;
        return;
    }

    redrawType = REDRAW_TILE;
    needsRedraw = true;
}

static void markPlayerFacingCursorDirty()
{
    Entity* player = getPlayerCombatant();

    if (player != nullptr)
    {
        markTileDirty(
            player->x + directionOffsets[moveDirection].dx,
            player->y + directionOffsets[moveDirection].dy);
    }
}

static void markAttackCursorDirty();
static void markInspectionCursorDirty();

static int gridDistanceToTile(const Entity* entity, int tileX, int tileY)
{
    return std::max(abs(entity->x - tileX), abs(entity->y - tileY));
}

static bool isChannelEnergyCreature(const Entity& entity)
{
    return entity.type == ENTITY_PLAYER ||
           entity.type == ENTITY_MONSTER ||
           entity.type == ENTITY_NPC;
}

static bool isValidChannelEnergyTarget(
    const Entity& cleric,
    const Entity& target)
{
    return target.active &&
           isChannelEnergyCreature(target) &&
           target.character.team == cleric.character.team &&
           target.character.state == STATE_ALIVE &&
           target.character.health.currentHP <
               target.character.health.maxHP &&
           getEntityGridDistance(cleric, target) <= 6;
}

static SoundEffect getAttackSound(
    const Entity* attacker,
    const Weapon* weapon)
{
    if (weapon != nullptr && weapon->type == WEAPON_RANGED)
        return SoundEffect::BOW_FIRE;

    if (attacker != nullptr && attacker->type == ENTITY_MONSTER &&
        (attacker->monsterID == MONSTER_GOBLIN_SCIMITAR ||
         attacker->monsterID == MONSTER_GOBLIN_ARCHER))
    {
        return SoundEffect::GOBLIN_ATTACK;
    }

    return SoundEffect::ATTACK;
}

// Returns the nearest square of a target that the attacker can see.  This is
// important for large creatures: a tree may hide their anchor square while
// another square of their footprint is still visible and can be attacked.
static bool getVisibleTargetTile(
    const Entity* attacker,
    const Entity* target,
    int& targetX,
    int& targetY)
{
    if (attacker == nullptr || target == nullptr)
        return false;

    bool foundVisibleTile = false;
    int closestDistance = 0;

    for (uint8_t offsetY = 0;
         offsetY < getEntityTileHeight(*target);
         offsetY++)
    {
        for (uint8_t offsetX = 0;
             offsetX < getEntityTileWidth(*target);
             offsetX++)
        {
            int candidateX = target->x + offsetX;
            int candidateY = target->y + offsetY;

            if (!hasLineOfSight(
                    attacker->x,
                    attacker->y,
                    candidateX,
                    candidateY))
            {
                continue;
            }

            int distance = gridDistanceToTile(
                attacker, candidateX, candidateY);

            if (!foundVisibleTile || distance < closestDistance)
            {
                targetX = candidateX;
                targetY = candidateY;
                closestDistance = distance;
                foundVisibleTile = true;
            }
        }
    }

    return foundVisibleTile;
}

static bool isValidRangedTarget(const Entity* player, const Entity* target)
{
    if (target == nullptr || target == player || !target->active ||
        target->character.state != STATE_ALIVE)
    {
        return false;
    }

    int targetX = 0;
    int targetY = 0;

    return getVisibleTargetTile(player, target, targetX, targetY);
}

static bool isPlayerSideCharacter(const Entity& entity)
{
    return entity.active &&
           entity.type == ENTITY_PLAYER &&
           entity.character.team == TEAM_PLAYER;
}

static bool isMonsterCombatant(const Entity& entity)
{
    return entity.active &&
           entity.type == ENTITY_MONSTER &&
           entity.character.team == TEAM_MONSTER;
}

static bool areHostile(const Entity& attacker, const Entity& target)
{
    return attacker.character.team != TEAM_NEUTRAL &&
           target.character.team != TEAM_NEUTRAL &&
           attacker.character.team != target.character.team;
}

static bool isValidAbilitySelectionTarget(
    const Entity* caster,
    const Entity* target)
{
    return caster != nullptr && target != nullptr && target != caster &&
           target->active &&
           (target->type == ENTITY_PLAYER ||
            target->type == ENTITY_MONSTER ||
            target->type == ENTITY_NPC) &&
           target->character.state == STATE_ALIVE &&
           areHostile(*caster, *target);
}

static bool selectNextEntityTarget(
    Entity* player,
    bool forward,
    bool abilityTargeting)
{
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (player == nullptr || entities == nullptr || entityCount == 0)
        return false;

    int index = combat.selectedTargetIndex;

    for (uint8_t checked = 0; checked < entityCount; checked++)
    {
        index += forward ? 1 : -1;

        if (index < 0)
            index = entityCount - 1;
        else if (index >= entityCount)
            index = 0;

        bool valid = abilityTargeting
            ? isValidAbilitySelectionTarget(player, &entities[index])
            : isValidRangedTarget(player, &entities[index]);

        if (valid)
        {
            combat.selectedTargetIndex = index;
            return true;
        }
    }

    combat.selectedTargetIndex = -1;
    return false;
}

bool canSneakAttack(const Entity& attacker, const Entity& target)
{
    return attacker.active &&
           target.active &&
           attacker.character.state == STATE_ALIVE &&
           target.character.state == STATE_ALIVE &&
           attacker.character.characterClass == CLASS_ROGUE &&
           areHostile(attacker, target) &&
           hasCondition(target.character, CONDITION_FLAT_FOOTED);
}

static int rollSneakAttackDamage(
    const Entity& attacker,
    const Entity& target)
{
    if (!canSneakAttack(attacker, target))
        return 0;

    uint8_t dice = getSneakAttackDice(attacker.character);

    if (dice == 0)
        return 0;

    int damage = rollDice(dice, 6);

    Serial.print("Sneak Attack: +");
    Serial.print(dice);
    Serial.print("d6 = ");
    Serial.println(damage);

    return damage;
}

static void applyFlatFootedToCombatants()
{
    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        if (entity == nullptr || entity->character.state != STATE_ALIVE)
            continue;

        if (addCondition(entity->character, CONDITION_FLAT_FOOTED, 0, 0))
        {
            Serial.print(getEntityName(entity));
            Serial.println(" is FLAT-FOOTED");
        }
    }
}

static void removeFlatFooted(Entity* entity)
{
    if (entity != nullptr &&
        removeCondition(entity->character, CONDITION_FLAT_FOOTED))
    {
        Serial.print(getEntityName(entity));
        Serial.println(" is no longer FLAT-FOOTED");
    }
}

static void clearCombatFlatFootedConditions()
{
    for (uint8_t i = 0; i < combat.combatantCount; i++)
        removeFlatFooted(combat.initiativeOrder[i]);
}

static bool areAllCombatMonstersDefeated()
{
    bool hasMonster = false;

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        if (entity == nullptr || !isMonsterCombatant(*entity))
            continue;

        hasMonster = true;

        if (entity->character.state == STATE_ALIVE)
            return false;
    }

    return hasMonster;
}

static uint8_t getParticipatingPlayerCount()
{
    uint8_t playerCount = 0;

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        if (entity != nullptr && isPlayerSideCharacter(*entity))
            playerCount++;
    }

    return playerCount;
}

struct DefeatResult
{
    uint32_t experiencePerCharacter = 0;
    uint8_t levelReached = 0;
};

static DefeatResult finalizeDefeat(Entity& defeated)
{
    DefeatResult result;

    // This is the one-time ALIVE -> DEAD transition for combat damage and
    // turn-start condition damage. A corpse, inactive entity, or already
    // defeated combatant cannot pass through it again.
    if (!defeated.active || defeated.character.state != STATE_ALIVE)
        return result;

    defeated.character.health.currentHP = 0;
    defeated.character.state = STATE_DEAD;
    generateCorpseLoot(defeated);

    // Only a hostile static monster definition carries a combat XP award.
    if (!isMonsterCombatant(defeated))
        return result;

    const Monster* monster = defeated.monster;

    if (monster == nullptr)
        monster = getMonster(defeated.monsterID);

    uint8_t playerCount = getParticipatingPlayerCount();

    if (monster == nullptr || playerCount == 0)
        return result;

    uint32_t monsterExperience =
        getExperienceAward(monster->challengeRating);

    if (monsterExperience > UINT32_MAX - combat.defeatedMonsterExperience)
        combat.defeatedMonsterExperience = UINT32_MAX;
    else
        combat.defeatedMonsterExperience += monsterExperience;

    // Preserve the existing encounter-total party split while still making
    // each death transition the award event. Using the cumulative share also
    // carries integer-division remainders into later kills.
    uint32_t cumulativeShare =
        combat.defeatedMonsterExperience / playerCount;

    if (cumulativeShare > combat.experienceGained)
    {
        result.experiencePerCharacter =
            cumulativeShare - combat.experienceGained;
    }

    if (result.experiencePerCharacter == 0)
        return result;

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        if (entity == nullptr || !isPlayerSideCharacter(*entity))
            continue;

        uint8_t levelsGained = awardExperience(
            entity->character, result.experiencePerCharacter);

        if (levelsGained > 0)
            result.levelReached = entity->character.level;
    }

    combat.experienceGained = cumulativeShare;

    if (result.levelReached > 0)
        playSound(SoundEffect::LEVEL_UP);

    return result;
}

CombatDamageResult applyCombatDamage(Entity& target, int damage)
{
    CombatDamageResult result;

    if (damage <= 0 || !target.active ||
        target.character.state != STATE_ALIVE)
    {
        return result;
    }

    target.character.health.currentHP -= damage;
    result.applied = true;

    if (target.character.health.currentHP <= 0)
    {
        DefeatResult defeatResult = finalizeDefeat(target);
        result.defeated = target.character.state == STATE_DEAD;
        result.levelReached = defeatResult.levelReached;
    }

    return result;
}

static void appendLevelUpFeedback(
    char* message,
    size_t messageSize,
    const DefeatResult& result)
{
    if (result.levelReached == 0 || messageSize == 0)
        return;

    size_t messageLength = strlen(message);

    if (messageLength >= messageSize)
        return;

    snprintf(message + messageLength,
             messageSize - messageLength,
             " LEVEL UP! Level %u.",
             static_cast<unsigned int>(result.levelReached));
}

void presentAbilityResolution(
    Entity& caster,
    Entity& target,
    AbilityID abilityID,
    const AbilityResolution& resolution)
{
    if (resolution.result != ABILITY_RESULT_SUCCESS)
    {
        setGameMessage(getAbilityResultMessage(resolution.result));
        playSound(SoundEffect::SPELL_FAIL);
        requestCombatTileRedraw();
        return;
    }

    const char* abilityName = getAbilityName(abilityID);
    char message[128];

    if (resolution.damage > 0)
    {
        snprintf(message, sizeof(message),
                 "%s hits %s for %d%s",
                 abilityName,
                 getEntityName(&target),
                 resolution.damage,
                 resolution.targetDefeated ? " and defeats it!" : "!");
    }
    else
    {
        snprintf(message, sizeof(message),
                 "%s heals %s for %d HP!",
                 abilityName,
                 getEntityName(&target),
                 resolution.healing);
    }

    if (resolution.levelReached > 0)
    {
        DefeatResult defeatResult;
        defeatResult.levelReached = resolution.levelReached;
        appendLevelUpFeedback(message, sizeof(message), defeatResult);
        // finalizeDefeat() already played the one-shot level sound.  Leave it
        // active instead of replacing it with the ordinary impact sound.
    }
    else if (resolution.damage > 0)
    {
        // finalizeDefeat() already selected the one-shot death sound.
        if (!resolution.targetDefeated)
            playSound(SoundEffect::SPELL_HIT);
    }
    else
        playSound(SoundEffect::SPELL_HEAL);

    setGameMessage(message);
    markEntityFootprintDirty(target);
    requestCombatTileRedraw();

    if (!combat.active)
        return;

    combat.abilityResolutionPending = true;
    combat.abilityCaster = &caster;
    combat.abilityEndedCombat = resolution.targetDefeated &&
        (target.type == ENTITY_PLAYER || areAllCombatMonstersDefeated());
    combat.abilityResultTime = millis();

    if (caster.type == ENTITY_PLAYER)
        combat.waitingForPlayer = false;
}

static bool isInspectableEntity(const Entity& entity)
{
    if (!entity.active)
        return false;

    return entity.type == ENTITY_PLAYER ||
           entity.type == ENTITY_MONSTER ||
           entity.type == ENTITY_NPC;
}

static const char* getInspectionHealthText(const Entity& entity)
{
    int currentHP = entity.character.health.currentHP;
    int maxHP = entity.character.health.maxHP;

    if (currentHP < 0)
        return "dying";

    if (currentHP == 0)
        return "disabled and staggered";

    if (currentHP == 1)
        return "near to death";

    if (maxHP <= 0 || currentHP >= maxHP)
        return "full";

    int percent = currentHP * 100 / maxHP;

    if (percent >= 75)
        return "mostly well";

    if (percent >= 50)
        return "injured";

    return "very hurt";
}

static void showInspection(const Entity* entity)
{
    if (entity == nullptr)
    {
        setGameMessage("No creatures to inspect.");
        requestCombatTileRedraw();
        return;
    }

    char message[64];
    snprintf(message, sizeof(message), "%s: %s",
             getEntityName(entity),
             getInspectionHealthText(*entity));
    setGameMessage(message);
    requestCombatTileRedraw();
}

static void finishPlayerAttack()
{
    combat.attackType = COMBAT_ATTACK_NONE;
    combat.selectedTargetIndex = -1;
    combat.attackDamagePending = false;
    combat.attackResolutionPending = false;
    combat.pendingAttackTarget = nullptr;
    combat.pendingDamage = 0;
    combat.pendingSneakAttack = false;
    combat.pendingSneakAttackDamage = 0;
    combat.pendingPowerAttack = false;
    combat.pendingPowerAttackDamage = 0;

    markPlayerFacingCursorDirty();
    requestCombatTileRedraw();

    if (areAllCombatMonstersDefeated())
    {
        combat.waitingForPlayer = false;
        combat.phase = COMBAT_END;
        return;
    }

    Entity* player = getPlayerCombatant();

    if (player == nullptr)
        return;

    player->turn.standardActionUsed = true;
    combat.waitingForPlayer = true;
    checkEndPlayerTurn();
}

static const MonsterPoisonData* getMonsterPoisonData(
    const Entity* monster)
{
    if (monster == nullptr || monster->monster == nullptr ||
        !monsterHasSpecialAbility(*monster->monster, ABILITY_POISON))
    {
        return nullptr;
    }

    const MonsterPoisonData& poison = monster->monster->poison;

    return poison.saveDC > 0 && poison.rounds > 0 ? &poison : nullptr;
}

static void resolveMonsterPoison(Entity* monster, Entity* target)
{
    const MonsterPoisonData* poison = getMonsterPoisonData(monster);

    if (poison == nullptr || target == nullptr ||
        target->character.state != STATE_ALIVE)
    {
        return;
    }

    int dieRoll = rollDie(20);
    int fortitude = getFortitudeSave(target->character);
    int total = dieRoll + fortitude;

    Serial.print(monster->monsterID == MONSTER_GIANT_SPIDER
        ? "Spider"
        : getEntityName(monster));
    Serial.print(" poison save: ");
    Serial.print(dieRoll);
    Serial.print(" + ");
    Serial.print(fortitude);
    Serial.print(" = ");
    Serial.print(total);
    Serial.print(" vs DC ");
    Serial.println(poison->saveDC);

    if (total >= poison->saveDC)
    {
        if (target->type == ENTITY_PLAYER)
        {
            setGameMessage("You resist the poison.");
        }
        else
        {
            char message[64];
            snprintf(message, sizeof(message), "%s resists the poison.",
                     getEntityName(target));
            setGameMessage(message);
        }

        return;
    }

    if (!addCondition(target->character,
                      CONDITION_POISONED,
                      0,
                      poison->rounds))
    {
        setGameMessage("Poison has no effect.");
        return;
    }

    if (target->type == ENTITY_PLAYER)
    {
        setGameMessage("You are poisoned.");
    }
    else
    {
        char message[64];
        snprintf(message, sizeof(message), "%s is poisoned.",
                 getEntityName(target));
        setGameMessage(message);
    }
}

static void updateMonsterAttack()
{
    if (millis() - combat.monsterAttackTime < COMBAT_MESSAGE_PAUSE_MS ||
        !isGameMessageComplete())
        return;

    Entity* monster = combat.attackingMonster;
    Entity* target = combat.monsterAttackTarget;

    switch (combat.monsterAttackPhase)
    {
        case MONSTER_ATTACK_ROLL_RESULT:
        {
            char message[64];

            snprintf(message, sizeof(message), "%s %s!",
                     getEntityName(monster),
                     combat.monsterAttackHit ? "hits" : "misses");
            setGameMessage(message);

            combat.monsterAttackPhase = MONSTER_ATTACK_DAMAGE_RESULT;
            combat.monsterAttackTime = millis();
            requestCombatTileRedraw();
            break;
        }

        case MONSTER_ATTACK_DAMAGE_RESULT:
        {
            bool poisonPending = false;

            if (combat.monsterAttackHit && target != nullptr &&
                target->character.state == STATE_ALIVE)
            {
                target->character.health.currentHP -= combat.monsterPendingDamage;

                char message[64];

                if (target->character.health.currentHP <= 0)
                {
                    DefeatResult defeatResult = finalizeDefeat(*target);
                    combat.monsterDefeatedPlayer = true;

                    if (combat.monsterSneakAttack)
                    {
                        snprintf(message, sizeof(message),
                                 "Player takes %d damage (Sneak Attack +%d) and falls!",
                                 combat.monsterPendingDamage,
                                 combat.monsterPendingSneakAttackDamage);
                    }
                    else
                    {
                        snprintf(message, sizeof(message),
                                 "Player takes %d damage and falls!",
                                 combat.monsterPendingDamage);
                    }

                    appendLevelUpFeedback(
                        message, sizeof(message), defeatResult);
                }
                else
                {
                    if (combat.monsterSneakAttack)
                    {
                        snprintf(message, sizeof(message),
                                 "Player takes %d damage (Sneak Attack +%d)!",
                                 combat.monsterPendingDamage,
                                 combat.monsterPendingSneakAttackDamage);
                    }
                    else
                    {
                        snprintf(message, sizeof(message),
                                 "Player takes %d damage!",
                                 combat.monsterPendingDamage);
                    }
                }

                setGameMessage(message);
                // A target can cover more than one map tile (for example,
                // the giant spider).  Redraw its entire footprint so damage
                // and the dead/loot marker cannot leave stale sprite pixels.
                markEntityFootprintDirty(*target);

                poisonPending =
                    target->character.state == STATE_ALIVE &&
                    combat.monsterAttackType == COMBAT_ATTACK_MELEE &&
                    getMonsterPoisonData(monster) != nullptr;
            }

            combat.monsterAttackPhase = poisonPending
                ? MONSTER_ATTACK_POISON_RESULT
                : MONSTER_ATTACK_COMPLETE;
            combat.monsterAttackTime = millis();
            requestCombatTileRedraw();
            break;
        }

        case MONSTER_ATTACK_POISON_RESULT:
            resolveMonsterPoison(monster, target);
            combat.monsterAttackPhase = MONSTER_ATTACK_COMPLETE;
            combat.monsterAttackTime = millis();
            requestCombatTileRedraw();
            break;

        case MONSTER_ATTACK_COMPLETE:
            combat.monsterAttackPhase = MONSTER_ATTACK_NONE;
            combat.attackingMonster = nullptr;
            combat.monsterAttackTarget = nullptr;
            combat.monsterPendingDamage = 0;
            combat.monsterSneakAttack = false;
            combat.monsterPendingSneakAttackDamage = 0;
            combat.monsterAttackType = COMBAT_ATTACK_NONE;

            if (combat.monsterDefeatedPlayer)
                combat.phase = COMBAT_END;

            break;

        default:
            break;
    }
}


void findCombatants()
{
    combat.combatantCount = 0;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities == nullptr)
        return;

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& entity = entities[i];

        if (!entity.active)
            continue;

        if (entity.character.state != STATE_ALIVE)
            continue;

        if (!isPlayerSideCharacter(entity) &&
            !isMonsterCombatant(entity))
        {
            continue;
        }

        if (combat.combatantCount >= MAX_DUNGEON_CHARACTERS)
            break;

        combat.initiativeOrder[combat.combatantCount++] = &entity;
    }

    //--------------------------------------------------
    // Debug
    //--------------------------------------------------

    Serial.println("Combatants:");

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        Serial.print(i);
        Serial.print(": ");

        if (entity->type == ENTITY_PLAYER)
            Serial.println("Player");
        else
            Serial.println(getEntityName(entity));
    }

    Serial.println();
}

void checkForCombat()
{
    Serial.println("checkForCombat()");
    if (combat.active)
        return;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    Entity* playerEntity = getActiveMapPlayer();

    // A defeated player remains an active map entity so their final state can
    // still be drawn, but they cannot trigger a new encounter.
    if (entities == nullptr || playerEntity == nullptr ||
        playerEntity->character.state != STATE_ALIVE)
    {
        return;
    }

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& monster = entities[i];

        // Dead monsters stay active on the map until their corpse loot is
        // collected. They are valid interaction targets, not combat targets.
        if (!isMonsterCombatant(monster) ||
            monster.character.state != STATE_ALIVE)
        {
            continue;
        }

        int distance = getEntityGridDistance(*playerEntity, monster);

        Serial.print("Monster at ");
        Serial.print(monster.x);
        Serial.print(", ");
        Serial.print(monster.y);
        Serial.print("  Distance = ");
        Serial.println(distance);

        if (distance > COMBAT_DETECTION_RANGE)
            continue;

        if (!hasLineOfSightBetweenFootprintsAt(
                *playerEntity,
                playerEntity->x,
                playerEntity->y,
                monster))
        {
            continue;
        }

        setGameMessage("You hear something nearby.");

        startCombat();

        return;
    }
}

void rollInitiative()
{
    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        entity->character.initiative =
            rollDie(20) +
            getAbilityModifier(entity->character.abilities.dexterity);
    }
}

void sortInitiative()
{
    for (uint8_t i = 0; i < combat.combatantCount - 1; i++)
    {
        for (uint8_t j = i + 1; j < combat.combatantCount; j++)
        {
            if (combat.initiativeOrder[j]->character.initiative >
                combat.initiativeOrder[i]->character.initiative)
            {
                Entity* temp = combat.initiativeOrder[i];
                combat.initiativeOrder[i] = combat.initiativeOrder[j];
                combat.initiativeOrder[j] = temp;
            }
        }
    }
}

Entity* getCurrentCombatant()
{
    if (!combat.active)
        return nullptr;

    if (combat.currentTurnIndex >= combat.combatantCount)
        return nullptr;

    return combat.initiativeOrder[combat.currentTurnIndex];
}

bool isPlayerTurn()
{
    Entity* current = getCurrentCombatant();

    if (current == nullptr)
        return false;

    return (current->type == ENTITY_PLAYER);
}

void startCombat()
{
    combat.active = true;
    playSound(SoundEffect::DUNGEON_THEME);

    findCombatants();
    rollInitiative();
    sortInitiative();
    applyFlatFootedToCombatants();

    combat.phase = COMBAT_INITIATIVE;
    combat.phaseStartTime = millis();
    combat.initiativeMessageShown = false;

    combat.currentTurnIndex = 0;
    combat.combatRound = 1;
    combat.defeatedMonsterExperience = 0;
    combat.experienceGained = 0;
    combat.endPlayerTurnAfterMessage = false;
    combat.selectedAbility = ABILITY_NONE;
    combat.abilityResolutionPending = false;
    combat.abilityCaster = nullptr;
    combat.abilityEndedCombat = false;
    combat.abilityResultTime = 0;
    combat.turnStartConditionPhase = TURN_START_CONDITION_NONE;
    combat.turnStartPoisonExpired = false;
    combat.turnStartConditionDefeated = false;

    backgroundNeedsRedraw = true;

    redrawType = REDRAW_FULL;
    needsRedraw = true;

}

static void finishTurnStart(Entity* entity)
{
    if (entity == nullptr)
        return;

    bool conditionDefeated = combat.turnStartConditionDefeated ||
                             entity->character.state != STATE_ALIVE;

    combat.turnStartConditionPhase = TURN_START_CONDITION_NONE;
    combat.turnStartPoisonExpired = false;
    combat.turnStartConditionDefeated = false;

    if (conditionDefeated)
    {
        combat.waitingForPlayer = false;

        if (entity->type == ENTITY_PLAYER)
        {
            combat.monsterDefeatedPlayer = true;
            combat.phase = COMBAT_END;
        }
        else if (areAllCombatMonstersDefeated())
        {
            combat.phase = COMBAT_END;
        }
        else
        {
            nextTurn();
        }

        return;
    }

    announceTurn(entity);

    if (entity->type == ENTITY_PLAYER)
    {
        setGameMessage("Player Turn A:Open Menu Move:Encoder");
        combat.waitingForPlayer = true;
    }
    else
    {
        setGameMessage("Monster Turn");
        combat.waitingForPlayer = false;
    }

    needsRedraw = true;
}

static void beginTurnStartConditionMessages(
    Entity* entity,
    const ConditionTurnResult& result)
{
    combat.turnStartPoisonExpired = result.poisonExpired;
    combat.turnStartConditionDefeated = false;

    if (result.damage > 0)
    {
        char message[128];

        if (entity->character.health.currentHP <= 0)
        {
            DefeatResult defeatResult = finalizeDefeat(*entity);
            combat.turnStartConditionDefeated = true;

            snprintf(message, sizeof(message),
                     "%s takes %d poison damage and dies!",
                     getEntityName(entity), result.damage);
            appendLevelUpFeedback(
                message, sizeof(message), defeatResult);
        }
        else if (entity->type == ENTITY_PLAYER)
        {
            snprintf(message, sizeof(message),
                     "You take %d poison damage.", result.damage);
        }
        else
        {
            snprintf(message, sizeof(message),
                     "%s takes %d poison damage.",
                     getEntityName(entity), result.damage);
        }

        setGameMessage(message);
        combat.turnStartConditionPhase =
            TURN_START_CONDITION_DAMAGE_MESSAGE;
        markEntityFootprintDirty(*entity);
        requestCombatTileRedraw();
        return;
    }

    if (result.poisonExpired)
    {
        setGameMessage("Poison wears off.");
        combat.turnStartConditionPhase =
            TURN_START_CONDITION_EXPIRY_MESSAGE;
        needsRedraw = true;
        return;
    }

    finishTurnStart(entity);
}

static bool updateTurnStartConditionMessages(Entity* entity)
{
    if (combat.turnStartConditionPhase == TURN_START_CONDITION_NONE)
        return false;

    if (!isGameMessageComplete())
        return true;

    if (combat.turnStartConditionPhase ==
        TURN_START_CONDITION_DAMAGE_MESSAGE &&
        combat.turnStartPoisonExpired)
    {
        setGameMessage("Poison wears off.");
        combat.turnStartConditionPhase =
            TURN_START_CONDITION_EXPIRY_MESSAGE;
        return true;
    }

    finishTurnStart(entity);
    return true;
}

void resetActions(Entity* entity)
{
    if (entity == nullptr)
        return;

    // This condition is intentionally untimed: its first start-of-turn
    // removal marks that this combatant has acted in the current combat.
    removeFlatFooted(entity);

    entity->turn.moveActionUsed = false;
    entity->turn.standardActionUsed = false;
    entity->turn.fullDefense = false;
    entity->turn.fiveFootStepUsed = false;
    entity->turn.delayTurn = false;
    entity->turn.powerAttackActive = false;
    entity->turn.turnActive = true;
    combat.waitingForPlayer = false;

    ConditionTurnResult result =
        processConditionsAtTurnStart(entity->character);
    beginTurnStartConditionMessages(entity, result);
}

void announceTurn(Entity* entity)
{
    entity->turn.movementRemaining =
    entity->character.speed;

    entity->turn.standardActionUsed = false;
    entity->turn.monsterState = MONSTER_START;
    Serial.println(entity->character.speed);
}

void runMonsterTurn(Entity* monster)
{
    switch (monster->turn.monsterState)
    {
        case MONSTER_START:

            monster->turn.monsterState = MONSTER_MOVE;
            break;

        case MONSTER_MOVE:

            performMovementPhase(monster);

            if (monster->turn.movementRemaining == 0 ||
                isMonsterReadyForAction(monster))
            {
                monster->turn.monsterState = MONSTER_ACTION;
            }

            break;

        case MONSTER_ACTION:

            runMonsterScript(monster);

            if (isMonsterAttackResolving())
            {
                monster->turn.monsterState = MONSTER_ATTACK;
            }
            else if (combat.abilityResolutionPending)
            {
                // The shared ability result remains visible before the turn
                // engine advances from this caster.
                monster->turn.monsterState = MONSTER_END;
            }
            else
            {
                // A ranged monster may finish its movement without a clear
                // shot, and any monster can be trapped with no legal move.
                // Consume the action and advance immediately rather than
                // leaving combat waiting on a monster with nothing to do.
                monster->turn.standardActionUsed = true;
                monster->turn.movementRemaining = 0;
                nextTurn();
            }
            break;

        case MONSTER_ATTACK:

            if (!isMonsterAttackResolving())
                monster->turn.monsterState = MONSTER_END;

            break;

        case MONSTER_END:

            nextTurn();
            break;
    }
}

void runPlayerTurn(Entity* player)
{
     // Nothing else.
    // Wait for button input.
}

void checkEndPlayerTurn()
{
    Entity* player = getCurrentCombatant();

    if (player == nullptr)
        return;

    if (player->type != ENTITY_PLAYER)
        return;

    if (player->turn.moveActionUsed &&
        player->turn.standardActionUsed)
    {
        endPlayerTurn();
    }
}


void nextTurn()
{
    Serial.println("NEXT TURN");

    for (uint8_t attempts = 0; attempts < combat.combatantCount; attempts++)
    {
        combat.currentTurnIndex++;

        if (combat.currentTurnIndex >= combat.combatantCount)
        {
            combat.currentTurnIndex = 0;
            combat.combatRound++;
        }

        Entity* next = combat.initiativeOrder[combat.currentTurnIndex];

        if (next != nullptr && next->active &&
            next->character.state == STATE_ALIVE)
        {
            break;
        }
    }

    combat.nextMonsterStep = millis();

    resetActions(getCurrentCombatant());
}

void endPlayerTurn()
{
    combat.waitingForPlayer = false;
    playSound(SoundEffect::DEFEND);

    nextTurn();
}

static bool skipTurnForCondition(Entity* entity)
{
    if (entity == nullptr || canCharacterAct(entity->character))
        return false;

    if (entity->turn.turnActive)
    {
        entity->turn.turnActive = false;
        entity->turn.movementRemaining = 0;
        entity->turn.moveActionUsed = true;
        entity->turn.standardActionUsed = true;
        combat.waitingForPlayer = false;

        char message[64];
        snprintf(message, sizeof(message), "%s cannot act.",
                 getEntityName(entity));
        setGameMessage(message);
        combat.nextMonsterStep = millis() + COMBAT_MESSAGE_PAUSE_MS;
        needsRedraw = true;
    }

    return true;
}

void updateCombat()
{
   switch (combat.phase)
    {
        case COMBAT_NONE:
            break;

        case COMBAT_INITIATIVE:

            if (!combat.initiativeMessageShown &&
                millis() - combat.phaseStartTime >= 2000)
            {
                setGameMessage("Roll for Initiative!");

                combat.initiativeMessageShown = true;

                playSound(SoundEffect::MENU_SELECT);

                combat.phase = COMBAT_TURN;

                resetActions(getCurrentCombatant());

                if (combat.turnStartConditionPhase ==
                    TURN_START_CONDITION_NONE)
                {
                    Entity* current = getCurrentCombatant();

                    if (current->type == ENTITY_PLAYER)
                        setGameMessage("Player Turn");
                    else
                        setGameMessage("Monster Turn");
                }
            }

            break;

       case COMBAT_TURN:
       {
           if (combat.endPlayerTurnAfterMessage)
           {
               if (isGameMessageComplete())
               {
                   combat.endPlayerTurnAfterMessage = false;
                   checkEndPlayerTurn();
               }

               break;
           }

           if (combat.abilityResolutionPending)
           {
               if (millis() - combat.abilityResultTime >=
                       COMBAT_MESSAGE_PAUSE_MS &&
                   isGameMessageComplete())
               {
                   Entity* caster = combat.abilityCaster;
                   bool endedCombat = combat.abilityEndedCombat;

                   combat.abilityResolutionPending = false;
                   combat.abilityCaster = nullptr;
                   combat.abilityEndedCombat = false;

                   if (endedCombat)
                   {
                       combat.waitingForPlayer = false;
                       combat.phase = COMBAT_END;
                   }
                   else if (caster != nullptr &&
                            caster->type == ENTITY_PLAYER)
                   {
                       combat.waitingForPlayer = true;
                       checkEndPlayerTurn();
                   }
                   else
                   {
                       nextTurn();
                   }
               }

               break;
           }

           if (combat.attackResolutionPending)
           {
               if (millis() - combat.attackResultTime >=
                       COMBAT_MESSAGE_PAUSE_MS &&
                   isGameMessageComplete())
               {
                   Entity* target = combat.pendingAttackTarget;

                   if (combat.attackDamagePending && target != nullptr &&
                       target->active &&
                       target->character.state == STATE_ALIVE)
                   {
                       target->character.health.currentHP -= combat.pendingDamage;

                       char message[128];

                       if (target->character.health.currentHP <= 0)
                       {
                            DefeatResult defeatResult =
                                finalizeDefeat(*target);

                           if (combat.pendingPowerAttack)
                           {
                               snprintf(message, sizeof(message),
                                        "%s takes %d damage (Power Attack +%d) and dies!",
                                        getEntityName(target), combat.pendingDamage,
                                        combat.pendingPowerAttackDamage);
                           }
                           else if (combat.pendingSneakAttack)
                           {
                               snprintf(message, sizeof(message),
                                        "%s takes %d damage (Sneak Attack +%d) and dies!",
                                        getEntityName(target), combat.pendingDamage,
                                        combat.pendingSneakAttackDamage);
                           }
                            else
                            {
                                snprintf(message, sizeof(message),
                                         "%s takes %d damage and dies!",
                                         getEntityName(target),
                                         combat.pendingDamage);
                            }

                            appendLevelUpFeedback(
                                message, sizeof(message), defeatResult);
                       }
                       else
                       {
                           if (combat.pendingPowerAttack)
                           {
                               snprintf(message, sizeof(message),
                                        "%s takes %d damage (Power Attack +%d)!",
                                        getEntityName(target), combat.pendingDamage,
                                        combat.pendingPowerAttackDamage);
                           }
                           else if (combat.pendingSneakAttack)
                           {
                               snprintf(message, sizeof(message),
                                        "%s takes %d damage (Sneak Attack +%d)!",
                                        getEntityName(target), combat.pendingDamage,
                                        combat.pendingSneakAttackDamage);
                           }
                           else
                           {
                               snprintf(message, sizeof(message),
                                        "%s takes %d damage!",
                                        getEntityName(target), combat.pendingDamage);
                           }
                       }

                       setGameMessage(message);

                       combat.attackDamagePending = false;
                       combat.attackResultTime = millis();

                       // Large creatures need every occupied tile redrawn
                       // when their health/death state changes.
                       markEntityFootprintDirty(*target);
                       requestCombatTileRedraw();

                       break;
                   }

                   finishPlayerAttack();
               }

               break;
           }

           if (isMonsterAttackResolving())
           {
               updateMonsterAttack();
               break;
           }

           Entity* current = getCurrentCombatant();

           if (current == nullptr)
               break;

           if (updateTurnStartConditionMessages(current))
               break;

           if (skipTurnForCondition(current))
           {
               if (!current->turn.turnActive &&
                   millis() >= combat.nextMonsterStep &&
                   isGameMessageComplete())
               {
                   nextTurn();
               }

               break;
           }

           if (current->type == ENTITY_PLAYER)
           {
               if (combat.waitingForPlayer)
               {
                   runPlayerTurn(current);
               }
           }
           else
           {
               if (millis() >= combat.nextMonsterStep)
               {
                   combat.nextMonsterStep = millis() + 120;

                   runMonsterTurn(current);
               }
           }

           break;
       }

        case COMBAT_END:
            endCombat();
            break;
    }
}

void endCombat()
{
    clearCombatFlatFootedConditions();

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* combatant = combat.initiativeOrder[i];

        if (combatant != nullptr)
            combatant->turn.powerAttackActive = false;
    }

    uint8_t playerCount = getParticipatingPlayerCount();
    uint32_t experiencePerCharacter = combat.experienceGained;
    bool victory = areAllCombatMonstersDefeated();

    if (experiencePerCharacter > 0)
    {
        char message[64];

        if (victory && playerCount == 1)
        {
            snprintf(message, sizeof(message),
                     "Victory! You gain %lu XP.",
                     static_cast<unsigned long>(experiencePerCharacter));
        }
        else if (victory)
        {
            snprintf(message, sizeof(message),
                     "Victory! Party gains %lu XP each.",
                     static_cast<unsigned long>(experiencePerCharacter));
        }
        else if (playerCount == 1)
        {
            snprintf(message, sizeof(message),
                     "You gained %lu XP.",
                     static_cast<unsigned long>(experiencePerCharacter));
        }
        else
        {
            snprintf(message, sizeof(message),
                     "Party gained %lu XP each.",
                     static_cast<unsigned long>(experiencePerCharacter));
        }

        setGameMessage(message);
    }

    combat.active = false;
    combat.phase = COMBAT_NONE;
    combat.endPlayerTurnAfterMessage = false;
    combat.selectedAbility = ABILITY_NONE;
    combat.abilityResolutionPending = false;
    combat.abilityCaster = nullptr;
    combat.abilityEndedCombat = false;
    backgroundNeedsRedraw = true;
    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

bool isCombatActive()
{
    return combat.active;
}

void beginPlayerAttack(CombatAttackType attackType)
{
    Entity* player = getCurrentCombatant();

    if (player == nullptr ||
        player->type != ENTITY_PLAYER ||
        !combat.waitingForPlayer ||
        player->turn.standardActionUsed ||
        !canCharacterAct(player->character))
    {
        return;
    }

    const Weapon* weapon =
        (attackType == COMBAT_ATTACK_MELEE)
            ? getEquippedMeleeWeapon(player->character)
            : getEquippedRangedWeapon(player->character);

    if (weapon == nullptr)
    {
        setGameMessage("No weapon equipped.");
        return;
    }

    combat.attackType = attackType;
    combat.attackResolutionPending = false;

    if (attackType == COMBAT_ATTACK_RANGED)
    {
        combat.selectedTargetIndex = -1;
        rotateAttackTarget(true);
        setGameMessage("Choose target: A attack B back");
    }
    else
    {
        setGameMessage("Choose direction: A attack B back");
    }

    markAttackCursorDirty();
    requestCombatTileRedraw();
}

bool isPlayerTargetingAttack()
{
    return combat.attackType != COMBAT_ATTACK_NONE &&
           !combat.attackResolutionPending;
}

bool isPlayerAttackResolving()
{
    return combat.attackResolutionPending;
}

Entity* getSelectedAttackTarget()
{
    Entity* player = getPlayerCombatant();
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (player == nullptr || entities == nullptr ||
        !isPlayerTargetingAttack())
    return nullptr;

    if (combat.attackType == COMBAT_ATTACK_MELEE)
    {
        int targetX = player->x + directionOffsets[moveDirection].dx;
        int targetY = player->y + directionOffsets[moveDirection].dy;

        if (!isInsideActiveMap(targetX, targetY))
        {
            return nullptr;
        }

        Entity* target = getEntityAt(
            entities,
            entityCount,
            targetX,
            targetY);

        return target != nullptr &&
               target->character.state == STATE_ALIVE
                   ? target
                   : nullptr;
    }

    if (combat.selectedTargetIndex < 0 ||
        combat.selectedTargetIndex >= entityCount)
    {
        return nullptr;
    }

    Entity* target = &entities[combat.selectedTargetIndex];

    return isValidRangedTarget(player, target) ? target : nullptr;
}

static void markAttackCursorDirty()
{
    if (combat.attackType == COMBAT_ATTACK_MELEE)
    {
        // The melee cursor represents the square the player is facing,
        // including a non-anchor square of a large creature.
        markPlayerFacingCursorDirty();
        return;
    }

    Entity* target = getSelectedAttackTarget();

    if (target != nullptr)
    {
        markEntityFootprintDirty(*target);
    }
}

void rotateAttackTarget(bool forward)
{
    if (!isPlayerTargetingAttack())
        return;

    markAttackCursorDirty();

    if (combat.attackType == COMBAT_ATTACK_MELEE)
    {
        previousMoveDirection = moveDirection;

        if (forward)
            rotateDirectionCW();
        else
            rotateDirectionCCW();
    }
    else
    {
        selectNextEntityTarget(
            getPlayerCombatant(), forward, false);
    }

    markAttackCursorDirty();
    requestCombatTileRedraw();
}

void confirmPlayerAttack()
{
    Entity* player = getCurrentCombatant();
    Entity* target = getSelectedAttackTarget();

    if (player == nullptr || target == nullptr ||
        !canCharacterAct(player->character))
    {
        setGameMessage("No target selected.");
        needsRedraw = true;
        return;
    }

    const Weapon* weapon =
        (combat.attackType == COMBAT_ATTACK_MELEE)
            ? getEquippedMeleeWeapon(player->character)
            : getEquippedRangedWeapon(player->character);

    if (weapon == nullptr)
    {
        cancelPlayerAttack();
        setGameMessage("No weapon equipped.");
        return;
    }

    playSound(getAttackSound(player, weapon));

    // Clear the targeting cursor before it stops being a targeting action.
    // For melee this is the exact square chosen by the direction cursor.
    markAttackCursorDirty();

    int rangePenalty = 0;

    if (combat.attackType == COMBAT_ATTACK_RANGED &&
        weapon->rangeIncrement > 0)
    {
        int targetX = target->x;
        int targetY = target->y;

        getVisibleTargetTile(player, target, targetX, targetY);

        int targetDistanceFeet =
            gridDistanceToTile(player, targetX, targetY) * 5;
        int incrementsBeyondFirst =
            (targetDistanceFeet - 1) / weapon->rangeIncrement;

        rangePenalty = -2 * incrementsBeyondFirst;
    }

    int dieRoll = rollDie(20);
    int powerAttackPenalty = 0;

    if (combat.attackType == COMBAT_ATTACK_MELEE &&
        weapon->type == WEAPON_MELEE &&
        player->turn.powerAttackActive)
    {
        powerAttackPenalty = getPowerAttackPenalty(player->character);
    }

    int normalAttackBonus =
        (combat.attackType == COMBAT_ATTACK_MELEE)
            ? getMeleeAttackBonus(player->character)
            : getRangedAttackBonus(player->character);
    int attackBonus = normalAttackBonus + powerAttackPenalty;
    int total = dieRoll + attackBonus + rangePenalty;
    bool hit = (dieRoll == 20) ||
               (dieRoll != 1 && total >= getArmorClass(
                   target->character,
                   target->turn.fullDefense ? 4 : 0));

    char message[64];

    if (rangePenalty < 0)
    {
        snprintf(message, sizeof(message), "%s! %d + %d - %d = %d",
                 hit ? "Hit" : "Miss", dieRoll, attackBonus,
                 -rangePenalty, total);
    }
    else if (powerAttackPenalty < 0)
    {
        snprintf(message, sizeof(message), "%s! %d + %d - %d = %d",
                 hit ? "Hit" : "Miss", dieRoll, normalAttackBonus,
                 -powerAttackPenalty, total);
    }
    else
    {
        snprintf(message, sizeof(message), "%s! %d + %d = %d",
                 hit ? "Hit" : "Miss", dieRoll, attackBonus, total);
    }

    setGameMessage(message);

    combat.pendingAttackTarget = target;
    combat.pendingDamage = 0;
    combat.pendingSneakAttack = false;
    combat.pendingSneakAttackDamage = 0;
    combat.pendingPowerAttack = false;
    combat.pendingPowerAttackDamage = 0;

    if (powerAttackPenalty < 0)
    {
        Serial.print("Power Attack: attack ");
        Serial.println(powerAttackPenalty);
    }

    if (hit)
    {
        combat.pendingDamage = rollDice(weapon->damageDice, weapon->damageSides);

        if (combat.attackType == COMBAT_ATTACK_MELEE)
        {
            combat.pendingDamage += getAbilityModifier(
                player->character, ABILITY_STRENGTH);

            if (powerAttackPenalty < 0)
            {
                combat.pendingPowerAttackDamage =
                    getPowerAttackDamageBonus(player->character, *weapon);
                combat.pendingPowerAttack =
                    combat.pendingPowerAttackDamage > 0;
                combat.pendingDamage +=
                    combat.pendingPowerAttackDamage;

                if (combat.pendingPowerAttack)
                {
                    Serial.print("Power Attack: damage +");
                    Serial.println(combat.pendingPowerAttackDamage);
                }
            }
        }

        combat.pendingDamage = std::max(1, combat.pendingDamage);

        combat.pendingSneakAttackDamage =
            rollSneakAttackDamage(*player, *target);
        combat.pendingSneakAttack =
            combat.pendingSneakAttackDamage > 0;
        combat.pendingDamage += combat.pendingSneakAttackDamage;
    }

    combat.attackDamagePending = hit;
    combat.attackResolutionPending = true;
    combat.attackResultTime = millis();
    combat.waitingForPlayer = false;

    markEntityFootprintDirty(*target);
    requestCombatTileRedraw();
}

void cancelPlayerAttack()
{
    if (!isPlayerTargetingAttack())
        return;

    markAttackCursorDirty();
    combat.attackType = COMBAT_ATTACK_NONE;
    combat.selectedTargetIndex = -1;
    markPlayerFacingCursorDirty();
    requestCombatTileRedraw();
    openMenu(&mainMenu);
}

static void markAbilityCursorDirty()
{
    Entity* target = getSelectedAbilityTarget();

    if (target != nullptr)
        markEntityFootprintDirty(*target);
}

static void executePlayerAbility(
    Entity& player,
    Entity* target,
    AbilityID abilityID)
{
    AbilityResolution resolution = resolveAbility(
        player, target, abilityID);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
    {
        setGameMessage(getAbilityResultMessage(resolution.result));
        playSound(SoundEffect::SPELL_FAIL);
        requestCombatTileRedraw();
        return;
    }

    presentAbilityResolution(
        player,
        target != nullptr ? *target : player,
        abilityID,
        resolution);
}

void beginPlayerAbility(AbilityID abilityID)
{
    Entity* player = getCurrentCombatant();
    const Ability* ability = getAbility(abilityID);

    if (player == nullptr || player->type != ENTITY_PLAYER ||
        !combat.waitingForPlayer ||
        !knowsAbility(player->character, abilityID) ||
        !isAbilitySupported(abilityID) ||
        ability == nullptr)
    {
        setGameMessage("Ability unavailable.");
        playSound(SoundEffect::SPELL_FAIL);
        return;
    }

    if (ability->target == TARGET_SELF)
    {
        executePlayerAbility(*player, player, abilityID);
        return;
    }

    combat.selectedAbility = abilityID;
    combat.selectedTargetIndex = -1;
    if (!selectNextEntityTarget(player, true, true))
    {
        combat.selectedAbility = ABILITY_NONE;
        setGameMessage("No valid targets.");
        playSound(SoundEffect::SPELL_FAIL);
        requestCombatTileRedraw();
        return;
    }

    markAbilityCursorDirty();
    setGameMessage("Choose target: A cast B back");
    requestCombatTileRedraw();
}

bool isPlayerTargetingAbility()
{
    return combat.selectedAbility != ABILITY_NONE &&
           !combat.abilityResolutionPending;
}

bool isAbilityResolving()
{
    return combat.abilityResolutionPending;
}

Entity* getSelectedAbilityTarget()
{
    Entity* player = getPlayerCombatant();
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (player == nullptr || entities == nullptr ||
        !isPlayerTargetingAbility() ||
        combat.selectedTargetIndex < 0 ||
        combat.selectedTargetIndex >= entityCount)
    {
        return nullptr;
    }

    Entity* target = &entities[combat.selectedTargetIndex];

    return isValidAbilitySelectionTarget(player, target)
        ? target
        : nullptr;
}

void rotateAbilityTarget(bool forward)
{
    if (!isPlayerTargetingAbility())
        return;

    markAbilityCursorDirty();
    selectNextEntityTarget(getPlayerCombatant(), forward, true);
    markAbilityCursorDirty();
    requestCombatTileRedraw();
}

void confirmPlayerAbility()
{
    Entity* player = getCurrentCombatant();
    Entity* target = getSelectedAbilityTarget();

    if (player == nullptr || target == nullptr)
    {
        setGameMessage("Invalid target.");
        playSound(SoundEffect::SPELL_FAIL);
        requestCombatTileRedraw();
        return;
    }

    AbilityID abilityID = combat.selectedAbility;
    AbilityResolution resolution = resolveAbility(
        *player, target, abilityID);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
    {
        setGameMessage(getAbilityResultMessage(resolution.result));
        playSound(SoundEffect::SPELL_FAIL);
        requestCombatTileRedraw();
        return;
    }

    markAbilityCursorDirty();
    combat.selectedAbility = ABILITY_NONE;
    combat.selectedTargetIndex = -1;
    markPlayerFacingCursorDirty();

    presentAbilityResolution(
        *player, *target, abilityID, resolution);
}

void cancelPlayerAbility()
{
    if (!isPlayerTargetingAbility())
        return;

    markAbilityCursorDirty();
    combat.selectedAbility = ABILITY_NONE;
    combat.selectedTargetIndex = -1;
    markPlayerFacingCursorDirty();
    requestCombatTileRedraw();
    openMenu(&mainMenu);
}

void beginInspection()
{
    if (combat.active && (!isPlayerTurn() || !combat.waitingForPlayer))
        return;

    combat.inspecting = true;
    combat.inspectedEntityIndex = -1;
    rotateInspectedEntity(true);
    showInspection(getInspectedEntity());
}

bool isInspectingEntities()
{
    return combat.inspecting;
}

Entity* getInspectedEntity()
{
    if (!combat.inspecting)
        return nullptr;

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities == nullptr || combat.inspectedEntityIndex < 0 ||
        combat.inspectedEntityIndex >= entityCount)
    {
        return nullptr;
    }

    Entity* entity = &entities[combat.inspectedEntityIndex];

    return isInspectableEntity(*entity) ? entity : nullptr;
}

static void markInspectionCursorDirty()
{
    Entity* entity = getInspectedEntity();

    if (entity != nullptr)
        markTileDirty(entity->x, entity->y);
}

void rotateInspectedEntity(bool forward)
{
    if (!combat.inspecting)
        return;

    markInspectionCursorDirty();

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (entities == nullptr || entityCount == 0)
    {
        combat.inspectedEntityIndex = -1;
        showInspection(nullptr);
        return;
    }

    int index = combat.inspectedEntityIndex;

    for (uint8_t checked = 0; checked < entityCount; checked++)
    {
        index += forward ? 1 : -1;

        if (index < 0)
            index = entityCount - 1;
        else if (index >= entityCount)
            index = 0;

        if (isInspectableEntity(entities[index]))
        {
            combat.inspectedEntityIndex = index;
            markInspectionCursorDirty();
            showInspection(&entities[index]);
            return;
        }
    }

    combat.inspectedEntityIndex = -1;
    showInspection(nullptr);
}

void confirmInspection()
{
    showInspection(getInspectedEntity());
}

void cancelInspection()
{
    if (!combat.inspecting)
        return;

    markInspectionCursorDirty();
    combat.inspecting = false;
    combat.inspectedEntityIndex = -1;
    markPlayerFacingCursorDirty();
    requestCombatTileRedraw();
    openMenu(&mainMenu);
}

void beginDoubleMove()
{
    Entity* player = getCurrentCombatant();

    if (player == nullptr || player->type != ENTITY_PLAYER ||
        !combat.waitingForPlayer || player->turn.standardActionUsed ||
        player->turn.movementRemaining != player->character.speed ||
        !canCharacterAct(player->character))
    {
        setGameMessage("Double Move unavailable.");
        return;
    }

    player->turn.movementRemaining = player->character.speed * 2;
    player->turn.standardActionUsed = true;
    setGameMessage("Double Move: use encoder to move.");
    requestCombatTileRedraw();
}

void beginTotalDefense()
{
    Entity* player = getCurrentCombatant();

    if (player == nullptr || player->type != ENTITY_PLAYER ||
        !combat.waitingForPlayer || player->turn.standardActionUsed ||
        player->turn.moveActionUsed ||
        !canCharacterAct(player->character))
    {
        setGameMessage("Total Defense unavailable.");
        return;
    }

    player->turn.fullDefense = true;
    player->turn.moveActionUsed = true;
    player->turn.standardActionUsed = true;
    setGameMessage("Total Defense: +4 AC.");
    endPlayerTurn();
}

bool canTogglePowerAttack(const Entity& fighter)
{
    if (!combat.active || !fighter.active ||
        fighter.type != ENTITY_PLAYER ||
        fighter.character.characterClass != CLASS_FIGHTER ||
        fighter.character.state != STATE_ALIVE ||
        getCurrentCombatant() != &fighter ||
        !combat.waitingForPlayer ||
        fighter.turn.standardActionUsed ||
        !canCharacterAct(fighter.character))
    {
        return false;
    }

    const Weapon* weapon = getEquippedMeleeWeapon(fighter.character);

    return weapon != nullptr && weapon->type == WEAPON_MELEE;
}

bool togglePowerAttack(Entity& fighter)
{
    if (!canTogglePowerAttack(fighter))
    {
        setGameMessage("Power Attack unavailable.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    fighter.turn.powerAttackActive =
        !fighter.turn.powerAttackActive;

    if (fighter.turn.powerAttackActive)
    {
        setGameMessage("Power Attack enabled.");
        Serial.println("Power Attack ON");
    }
    else
    {
        setGameMessage("Power Attack disabled.");
        Serial.println("Power Attack OFF");
    }

    return true;
}

bool canUseChannelEnergy(const Entity& cleric)
{
    if (!cleric.active ||
        cleric.character.characterClass != CLASS_CLERIC ||
        cleric.character.state != STATE_ALIVE ||
        cleric.character.classAbilities.channelEnergyCurrent == 0)
    {
        return false;
    }

    if (!combat.active)
        return true;

    Entity* combatant = getCurrentCombatant();

    return combatant == &cleric &&
           cleric.type == ENTITY_PLAYER &&
           combat.waitingForPlayer &&
           canCharacterAct(cleric.character) &&
           !cleric.turn.standardActionUsed;
}

bool useChannelEnergy(Entity& cleric)
{
    if (cleric.character.characterClass != CLASS_CLERIC ||
        !cleric.active || cleric.character.state != STATE_ALIVE)
    {
        setGameMessage("Channel Energy unavailable.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    if (cleric.character.classAbilities.channelEnergyCurrent == 0)
    {
        setGameMessage("No Channel Energy uses remaining.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    if (!canUseChannelEnergy(cleric))
    {
        setGameMessage("Channel Energy unavailable.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    bool hasWoundedTarget = false;

    for (uint8_t i = 0; entities != nullptr && i < entityCount; i++)
    {
        if (isValidChannelEnergyTarget(cleric, entities[i]))
        {
            hasWoundedTarget = true;
            break;
        }
    }

    if (!hasWoundedTarget)
    {
        setGameMessage("No one needs healing.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    uint8_t dice = getChannelEnergyDice(cleric.character);

    if (dice == 0)
    {
        setGameMessage("Channel Energy unavailable.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    int healing = rollDice(dice, 6);
    uint8_t previousUses =
        cleric.character.classAbilities.channelEnergyCurrent;

    cleric.character.classAbilities.channelEnergyCurrent--;

    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (isValidChannelEnergyTarget(cleric, entities[i]))
            healCharacter(entities[i].character, healing);
    }

    Serial.print("Channel Energy: ");
    Serial.print(dice);
    Serial.print("d6 = ");
    Serial.println(healing);
    Serial.print("Channel uses: ");
    Serial.print(previousUses);
    Serial.print(" -> ");
    Serial.println(
        cleric.character.classAbilities.channelEnergyCurrent);

    char message[64];
    snprintf(message, sizeof(message),
             "Channel Energy heals %d HP. Uses: %u.",
             healing,
             static_cast<unsigned int>(
                 cleric.character.classAbilities.channelEnergyCurrent));
    playSound(SoundEffect::SPELL_HEAL);
    setGameMessage(message);

    if (combat.active)
    {
        cleric.turn.standardActionUsed = true;

        if (cleric.turn.moveActionUsed)
        {
            // Keep the result visible before nextTurn() starts processing
            // the following combatant's conditions and turn message.
            combat.waitingForPlayer = false;
            combat.endPlayerTurnAfterMessage = true;
        }
        else
        {
            checkEndPlayerTurn();
        }
    }

    return true;
}

void beginMonsterAttack(
    Entity* monster,
    Entity* target,
    CombatAttackType attackType)
{
    if (monster == nullptr || target == nullptr ||
        monster->monster == nullptr ||
        isMonsterAttackResolving() ||
        !canCharacterAct(monster->character))
    {
        return;
    }

    EquipmentSlot weaponSlot = attackType == COMBAT_ATTACK_RANGED
        ? SLOT_RANGED_WEAPON
        : SLOT_MELEE_WEAPON;
    const Weapon* weapon = attackType == COMBAT_ATTACK_RANGED
        ? getEquippedRangedWeapon(monster->character)
        : getEquippedMeleeWeapon(monster->character);

    // Older in-memory encounters placed bows in the melee slot.  New
    // monsters use the correct ranged slot, but retaining this fallback
    // prevents an existing encounter from silently losing its attack.
    if (attackType == COMBAT_ATTACK_RANGED &&
        (weapon == nullptr || weapon->type != WEAPON_RANGED))
    {
        const Weapon* fallback = getEquippedMeleeWeapon(monster->character);

        if (fallback != nullptr && fallback->type == WEAPON_RANGED)
        {
            weapon = fallback;
            weaponSlot = SLOT_MELEE_WEAPON;
        }
    }

    if (weapon == nullptr ||
        (attackType == COMBAT_ATTACK_RANGED &&
         weapon->type != WEAPON_RANGED) ||
        (attackType != COMBAT_ATTACK_RANGED &&
         weapon->type != WEAPON_MELEE))
    return;

    playSound(getAttackSound(monster, weapon));

    int abilityModifier = getAbilityModifier(
        monster->character,
        weapon->type == WEAPON_RANGED
            ? ABILITY_DEXTERITY
            : ABILITY_STRENGTH);
    int rangePenalty = 0;

    if (weapon->type == WEAPON_RANGED && weapon->rangeIncrement > 0)
    {
        int distanceFeet =
            getEntityGridDistance(*monster, *target) * 5;
        int incrementsBeyondFirst =
            (distanceFeet - 1) / weapon->rangeIncrement;

        rangePenalty = -2 * incrementsBeyondFirst;
    }

    int dieRoll = rollDie(20);
    int total = dieRoll + monster->monster->baseAttack + abilityModifier +
                getConditionAttackModifier(monster->character) +
                rangePenalty;

    combat.monsterAttackHit = (dieRoll == 20) ||
        (dieRoll != 1 && total >= getArmorClass(
            target->character,
            target->turn.fullDefense ? 4 : 0));
    combat.monsterPendingDamage = 0;
    combat.monsterSneakAttack = false;
    combat.monsterPendingSneakAttackDamage = 0;

    if (combat.monsterAttackHit)
    {
        combat.monsterPendingDamage = rollDice(
            weapon->damageDice,
            weapon->damageSides);

        if (weapon->type == WEAPON_MELEE)
            combat.monsterPendingDamage += abilityModifier;

        combat.monsterPendingDamage = std::max(
            1, combat.monsterPendingDamage);

        combat.monsterPendingSneakAttackDamage =
            rollSneakAttackDamage(*monster, *target);
        combat.monsterSneakAttack =
            combat.monsterPendingSneakAttackDamage > 0;
        combat.monsterPendingDamage +=
            combat.monsterPendingSneakAttackDamage;
    }

    combat.attackingMonster = monster;
    combat.monsterAttackTarget = target;
    combat.monsterAttackType = attackType;
    combat.monsterDefeatedPlayer = false;
    combat.monsterAttackPhase = MONSTER_ATTACK_ROLL_RESULT;
    combat.monsterAttackTime = millis();
    monster->turn.standardActionUsed = true;

    char message[64];
    snprintf(message, sizeof(message), "%s makes a %s attack with %s.",
             getEntityName(monster),
             weapon->type == WEAPON_RANGED ? "ranged" : "melee",
             getEquippedItemName(
                 monster->character,
                 weaponSlot));
    setGameMessage(message);
    requestCombatTileRedraw();
}

bool isMonsterAttackResolving()
{
    return combat.monsterAttackPhase != MONSTER_ATTACK_NONE;
}
