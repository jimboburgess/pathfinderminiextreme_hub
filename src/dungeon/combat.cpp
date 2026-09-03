#include "combat.h"

#include <algorithm>
#include <cstring>
#include <stdlib.h>

#include "map/activemap.h"
#include "abilityresolver.h"
#include "combatpolicy.h"
#include "loot.h"
#include "map/awareness.h"
#include "map/mapeffects.h"
#include "monsterscripts.h"
#include "audio/audio.h"
#include "data/dice.h"
#include "data/entityspawn.h"
#include "data/entitytraits.h"
#include "data/game.h"
#include "data/progression.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "input/menu.h"

Combat combat;

static const char* getEnergyTypeName(DamageType type)
{
    switch (type)
    {
        case DAMAGE_FIRE: return "Fire";
        case DAMAGE_COLD: return "Cold";
        case DAMAGE_ELECTRIC: return "Electricity";
        case DAMAGE_ACID: return "Acid";
        default: return "Energy";
    }
}

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
static uint8_t getLivingEnemyCombatantCount();
static void showCombatStartMessage();
static int rollCombatInitiative(Entity& entity);

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
           (target.character.state == STATE_ALIVE ||
            target.character.state == STATE_UNCONSCIOUS) &&
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
    if (caster == nullptr || target == nullptr ||
        !target->active ||
        (target->type != ENTITY_PLAYER &&
         target->type != ENTITY_MONSTER &&
         target->type != ENTITY_NPC) ||
        target->character.state != STATE_ALIVE)
    {
        return false;
    }

    const Ability* ability = getAbility(combat.selectedAbility);
    if (ability == nullptr)
        return false;

    const bool hostile =
        isAbilityEffectHostileToTarget(*ability, target->character);
    if ((hostile &&
         (target == caster || !areHostile(*caster, *target))) ||
        (!hostile && target->character.team != caster->character.team))
    {
        return false;
    }

    if (ability->delivery == DELIVERY_TOUCH && target != caster)
    {
        return getEntityGridDistance(*caster, *target) <= 1 &&
               hasLineOfSightBetweenFootprintsAt(
                   *caster, caster->x, caster->y, *target);
    }

    if (hostile)
    {
        return getEntityGridDistance(*caster, *target) <=
                   ability->rangeTiles &&
               hasLineOfSightBetweenFootprintsAt(
                   *caster, caster->x, caster->y, *target);
    }

    return true;
}

static bool isValidGroundAbilitySelection(
    const Entity* caster,
    const Ability* ability,
    int targetX,
    int targetY)
{
    return caster != nullptr && ability != nullptr &&
           isInsideActiveMap(targetX, targetY) &&
           getEntityGridDistanceToTile(
               *caster, targetX, targetY) <= ability->rangeTiles &&
           hasLineOfSightFromFootprintAt(
               *caster,
               caster->x,
               caster->y,
               targetX,
               targetY);
}

static bool selectNextGroundAbilityTarget(bool forward)
{
    Entity* caster = getPlayerCombatant();
    const Ability* ability = getAbility(combat.selectedAbility);
    int width = getActiveMapWidth();
    int height = getActiveMapHeight();

    if (caster == nullptr || ability == nullptr ||
        width <= 0 || height <= 0)
    {
        return false;
    }

    int currentIndex = caster->y * width + caster->x;

    if (isInsideActiveMap(
            combat.selectedAbilityX,
            combat.selectedAbilityY))
    {
        currentIndex = combat.selectedAbilityY * width +
                       combat.selectedAbilityX;
    }

    int tileCount = width * height;

    for (int step = 1; step <= tileCount; step++)
    {
        int offset = forward ? step : -step;
        int index = (currentIndex + offset) % tileCount;

        if (index < 0)
            index += tileCount;

        int candidateX = index % width;
        int candidateY = index / width;

        if (!isValidGroundAbilitySelection(
                caster, ability, candidateX, candidateY))
        {
            continue;
        }

        combat.selectedAbilityX = static_cast<int8_t>(candidateX);
        combat.selectedAbilityY = static_cast<int8_t>(candidateY);
        return true;
    }

    return false;
}

static bool selectInitialGroundAbilityTarget(
    const Entity& caster,
    const Ability& ability)
{
    // Start far enough forward that the complete area is easy to read, while
    // gracefully backing toward the caster in cramped rooms or short ranges.
    constexpr int PREFERRED_GROUND_TARGET_DISTANCE = 3;
    int startDistance = std::min<int>(
        PREFERRED_GROUND_TARGET_DISTANCE,
        ability.rangeTiles);

    const DirectionOffset& offset =
        directionOffsets[combat.selectedAbilityDirection];

    for (int distance = startDistance; distance >= 1; distance--)
    {
        int targetX = caster.x + offset.dx * distance;
        int targetY = caster.y + offset.dy * distance;

        if (!isValidGroundAbilitySelection(
                &caster, &ability, targetX, targetY))
        {
            continue;
        }

        combat.selectedAbilityX = static_cast<int8_t>(targetX);
        combat.selectedAbilityY = static_cast<int8_t>(targetY);
        return true;
    }

    // A blocked facing direction should not make the spell unusable. Choose
    // another legal map tile only as a deterministic fallback; normal cursor
    // movement remains spatial after targeting begins.
    combat.selectedAbilityX = static_cast<int8_t>(caster.x);
    combat.selectedAbilityY = static_cast<int8_t>(caster.y);
    return selectNextGroundAbilityTarget(true);
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

        if (valid && !abilityTargeting && combat.openingAttackTargeting)
        {
            valid = entities[index].revealedToPlayer &&
                    areHostile(*player, entities[index]);
        }

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
    return qualifiesForSneakAttack(attacker, target);
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

        if (entity == nullptr || !isHostileMonsterForCombat(*entity))
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

    // Player defeat is recoverable. End the encounter immediately without
    // running victory/reward handling or converting the player into a corpse.
    // Monsters continue through the normal permanent-death path below.
    if (defeated.type == ENTITY_PLAYER)
    {
        defeated.character.state = STATE_UNCONSCIOUS;
        markEntityFootprintDirty(defeated);
        abortCombat();
        return result;
    }

    defeated.character.state = STATE_DEAD;
    generateCorpseLoot(defeated);

    // Only a hostile static monster definition carries a combat XP award.
    if (!isHostileMonsterForCombat(defeated))
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

void applyEnvironmentalDamage(Entity& target, int damage)
{
    if (!target.active || target.character.state != STATE_ALIVE || damage <= 0)
        return;

    damageCharacter(target.character, damage);
    if (target.character.health.currentHP <= 0)
        finalizeDefeat(target);
    markEntityFootprintDirty(target);
}

CombatDamageResult applyCombatDamage(Entity& target, int damage,
                                     DamageType damageType)
{
    CombatDamageResult result;

    if (damage <= 0 || !target.active ||
        target.character.state != STATE_ALIVE)
    {
        return result;
    }

    damage = applyEnergyMitigation(
        target.character, static_cast<uint8_t>(damageType), damage);
    result.damageApplied = damage;
    if (damage == 0)
    {
        result.applied = true;
        return result;
    }

    damageCharacter(target.character, damage);
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

    if (resolution.attackRoll.required && !resolution.attackRoll.hit)
    {
        snprintf(message, sizeof(message),
                 "%s misses %s!",
                 abilityName,
                 getEntityName(&target));
    }
    else if (resolution.savingThrow.result == SAVE_RESULT_SUCCESS &&
             resolution.damage == 0)
    {
        snprintf(message, sizeof(message),
                 "%s resists %s!",
                 getEntityName(&target),
                 abilityName);
    }
    else if (resolution.conditionApplied != CONDITION_NONE)
    {
        snprintf(message, sizeof(message),
                 "%s affects %s for %d rounds!",
                 abilityName,
                 getEntityName(&target),
                 resolution.conditionDuration);
    }
    else if (resolution.resistanceAmount > 0)
    {
        snprintf(message, sizeof(message), "%s resistance %d",
                 getEnergyTypeName(resolution.resistanceType),
                 resolution.resistanceAmount);
    }
    else if (resolution.protectionAmount > 0)
    {
        snprintf(message, sizeof(message), "%s protection %d",
                 getEnergyTypeName(resolution.protectionType),
                 resolution.protectionAmount);
    }
    else if (resolution.damage > 0)
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

    if (resolution.attackRoll.required && !resolution.attackRoll.hit)
        playSound(SoundEffect::MISS);
    else if (resolution.levelReached > 0)
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
    else if (resolution.savingThrow.result == SAVE_RESULT_SUCCESS ||
             resolution.conditionApplied != CONDITION_NONE ||
             resolution.resistanceAmount > 0 ||
             resolution.protectionAmount > 0)
    {
        playSound(SoundEffect::SPELL_CAST);
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

void presentGroundAbilityResolution(
    Entity& caster,
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
    char message[96];

    if (abilityID == ABILITY_WEB && resolution.targetsAffected > 0)
    {
        snprintf(message, sizeof(message), "Caught in the web!");
    }

    else if (resolution.targetsAffected > 0 &&
        resolution.targetsResisted > 0)
    {
        snprintf(message, sizeof(message),
                 "%s covers the area: %u affected, %u resist!",
                 abilityName,
                 static_cast<unsigned int>(resolution.targetsAffected),
                 static_cast<unsigned int>(resolution.targetsResisted));
    }
    else if (resolution.targetsAffected > 0)
    {
        snprintf(message, sizeof(message),
                 "%s covers the area; %u affected!",
                 abilityName,
                 static_cast<unsigned int>(resolution.targetsAffected));
    }
    else if (resolution.targetsResisted > 0)
    {
        snprintf(message, sizeof(message),
                 "%s covers the area; %u resist!",
                 abilityName,
                 static_cast<unsigned int>(resolution.targetsResisted));
    }
    else
    {
        snprintf(message, sizeof(message),
                 "%s covers the area!", abilityName);
    }

    setGameMessage(message);
    playSound(SoundEffect::SPELL_CAST);
    requestCombatTileRedraw();

    if (!combat.active)
        return;

    combat.abilityResolutionPending = true;
    combat.abilityCaster = &caster;
    combat.abilityEndedCombat = false;
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

static bool anotherMonsterDetectsAfterOpeningAttack(
    const Entity* openingTarget)
{
    Entity* player = getPlayerCombatant();
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    if (player == nullptr || entities == nullptr)
        return false;

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& monster = entities[i];

        if (&monster == openingTarget || !isLivingHostileForCombat(monster))
            continue;

        if (monster.awareOfPlayer ||
            tryMonsterDetectPlayer(monster, *player))
        {
            return true;
        }
    }

    return false;
}

static void beginInitiativeAfterOpeningAttack()
{
    // The provisional roster existed only so the established attack resolver
    // could run. Rebuild now so a dead opener is removed and a forest monster
    // that detected the player after the strike is included.
    clearCombatFlatFootedConditions();
    findCombatants();
    rollInitiative();
    sortInitiative();
    applyFlatFootedToCombatants();

    combat.phase = COMBAT_INITIATIVE;
    combat.phaseStartTime = millis();
    combat.initiativeMessageShown = false;
    combat.currentTurnIndex = 0;
    combat.combatRound = 1;
    combat.waitingForPlayer = false;
    showCombatStartMessage();
}

static void finishPlayerAttack()
{
    Entity* resolvedTarget = combat.pendingAttackTarget;
    const bool wasOpeningAttack = combat.openingAttackInProgress;
    const bool wasAmbush = combat.openingAttackWasAmbush;

    if (!wasOpeningAttack && combat.iterativeAttackActive &&
        resolvedTarget != nullptr &&
        shouldContinueIterativeAttack(
            combat.iterativeAttackIndex,
            combat.iterativeAttackCount,
            resolvedTarget->active,
            resolvedTarget->character.state == STATE_ALIVE))
    {
        combat.attackDamagePending = false;
        combat.attackResolutionPending = false;
        combat.pendingAttackTarget = nullptr;
        combat.pendingDamage = 0;
        combat.pendingSneakAttack = false;
        combat.pendingSneakAttackDamage = 0;
        combat.pendingPowerAttack = false;
        combat.pendingPowerAttackDamage = 0;
        combat.iterativeAttackIndex++;
        combat.waitingForPlayer = true;
        confirmPlayerAttack();
        return;
    }

    const bool completedFullAttack = combat.iterativeAttackActive &&
        combat.iterativeAttackCount > 1;

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
    combat.openingAttackInProgress = false;
    combat.openingAttackWasAmbush = false;
    combat.iterativeAttackActive = false;
    combat.iterativeAttackIndex = 0;
    combat.iterativeAttackCount = 1;

    markPlayerFacingCursorDirty();
    requestCombatTileRedraw();

    if (wasOpeningAttack)
    {
        const bool targetDefeated = resolvedTarget == nullptr ||
            resolvedTarget->character.state != STATE_ALIVE;

        const bool anotherMonsterDetected =
            wasAmbush && targetDefeated &&
            anotherMonsterDetectsAfterOpeningAttack(resolvedTarget);

        if (!shouldContinueCombatAfterOpeningAttack(
                wasAmbush, targetDefeated, anotherMonsterDetected))
        {
            // A quiet stealth kill does not establish a room-wide encounter.
            // endCombat() preserves the normal XP/death/loot feedback while
            // clearing provisional initiative conditions.
            endCombat();
            resetAwarenessTimer();
            return;
        }

        beginInitiativeAfterOpeningAttack();

        if (getLivingEnemyCombatantCount() == 0)
        {
            endCombat();
            resetAwarenessTimer();
        }

        return;
    }

    if (areAllCombatMonstersDefeated())
    {
        combat.waitingForPlayer = false;
        combat.phase = COMBAT_END;
        return;
    }

    Entity* player = getPlayerCombatant();

    if (player == nullptr)
        return;

    if (completedFullAttack)
    {
        player->turn.moveActionUsed = true;
        player->turn.movementRemaining = 0;
    }

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
                damageCharacter(
                    target->character, combat.monsterPendingDamage);

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
        {
            const CombatAttackType completedAttackType =
                combat.monsterAttackType;
            const bool continueFullAttack =
                combat.monsterIterativeAttackActive &&
                monster != nullptr && monster->active &&
                monster->character.state == STATE_ALIVE &&
                target != nullptr &&
                shouldContinueIterativeAttack(
                    combat.monsterIterativeAttackIndex,
                    combat.monsterIterativeAttackCount,
                    target->active,
                    target->character.state == STATE_ALIVE);

            combat.monsterAttackPhase = MONSTER_ATTACK_NONE;
            combat.attackingMonster = nullptr;
            combat.monsterAttackTarget = nullptr;
            combat.monsterPendingDamage = 0;
            combat.monsterSneakAttack = false;
            combat.monsterPendingSneakAttackDamage = 0;
            combat.monsterAttackType = COMBAT_ATTACK_NONE;

            if (combat.monsterDefeatedPlayer)
            {
                combat.phase = COMBAT_END;
            }
            else if (continueFullAttack)
            {
                combat.monsterIterativeAttackIndex++;
                beginMonsterAttack(monster, target, completedAttackType);
            }
            else
            {
                combat.monsterIterativeAttackActive = false;
                combat.monsterIterativeAttackIndex = 0;
                combat.monsterIterativeAttackCount = 1;
            }

            break;
        }

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

    Entity* player = getActiveMapPlayer();

    // The active entity array scopes dungeon combat to the current retained
    // room and forest combat to the current forest map. Detection starts an
    // encounter, but it never limits its initiative membership.
    combat.combatantCount = buildCombatRoster(
        entities,
        entityCount,
        player,
        combat.initiativeOrder,
        MAX_COMBATANTS);

    for (uint8_t i = 0; i < combat.combatantCount; i++)
        combat.initiativeOrder[i]->reinforcementJoinedRound = 0;

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

bool isCombatParticipant(const Entity& entity)
{
    if (!combat.active)
        return false;

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        if (combat.initiativeOrder[i] == &entity)
            return true;
    }

    return false;
}

bool addCombatReinforcement(Entity& monster)
{
    if (!combat.active || combat.combatantCount >= MAX_COMBATANTS ||
        !isLivingHostileForCombat(monster) || isCombatParticipant(monster))
    {
        return false;
    }

    monster.character.initiative = rollCombatInitiative(monster);
    monster.reinforcementJoinedRound = combat.combatRound;
    combat.initiativeOrder[combat.combatantCount++] = &monster;
    sortInitiative();
    return true;
}

static uint8_t getLivingEnemyCombatantCount()
{
    return countLivingHostilesInCombatRoster(
        combat.initiativeOrder, combat.combatantCount);
}

static void showCombatStartMessage()
{
    const uint8_t enemyCount = getLivingEnemyCombatantCount();
    char message[48];

    snprintf(message, sizeof(message),
             "Combat begins! %u %s.",
             static_cast<unsigned int>(enemyCount),
             enemyCount == 1 ? "enemy" : "enemies");
    setGameMessage(message);
}

void checkForCombat()
{
    // Normal awareness is interval-driven so movement is not the trigger.
}

void rollInitiative()
{
    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        entity->character.initiative = rollCombatInitiative(*entity);
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

static int rollCombatInitiative(Entity& entity)
{
    return rollDie(20) +
        getAbilityModifier(entity.character.abilities.dexterity);
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
    Entity* player = getActiveMapPlayer();
    if (player == nullptr || player->character.health.currentHP <= 0 ||
        player->character.state != STATE_ALIVE)
    {
        return;
    }

    clearMapEffects();
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
    combat.selectedAbilityX = -1;
    combat.selectedAbilityY = -1;
    combat.selectedAbilityDirection = moveDirection;
    combat.selectedAbilityDamageType = DAMAGE_NONE;
    combat.abilityResolutionPending = false;
    combat.abilityCaster = nullptr;
    combat.abilityEndedCombat = false;
    combat.abilityResultTime = 0;
    combat.turnStartConditionPhase = TURN_START_CONDITION_NONE;
    combat.turnStartPoisonExpired = false;
    combat.turnStartConditionDefeated = false;
    combat.turnStartActionPrevented = false;
    combat.openingAttackInProgress = false;
    combat.openingAttackWasAmbush = false;

    combat.iterativeAttackActive = false;
    combat.iterativeAttackIndex = 0;
    combat.iterativeAttackCount = 1;
    combat.monsterIterativeAttackActive = false;
    combat.monsterIterativeAttackIndex = 0;
    combat.monsterIterativeAttackCount = 1;

    showCombatStartMessage();

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
    ConditionTurnResult& result)
{
    combat.turnStartPoisonExpired = result.poisonExpired;
    combat.turnStartConditionDefeated = false;

    for (uint8_t i = 0; i < result.timedDamageCount &&
                        entity->character.state == STATE_ALIVE; i++)
    {
        const TimedDamageEffect& effect = result.timedDamage[i];
        const int damage = rollDice(effect.diceCount, effect.diceSides);
        const AreaFlashTile flashTile = {
            static_cast<int8_t>(entity->x), static_cast<int8_t>(entity->y)};
        playAreaDamageFlash(static_cast<DamageType>(effect.damageType),
                            &flashTile, 1);
        CombatDamageResult damageResult = applyCombatDamage(
            *entity, damage, static_cast<DamageType>(effect.damageType));
        if (damageResult.applied)
        {
            result.damage += damageResult.damageApplied;
            result.damageType = effect.damageType;
        }
    }

    if (result.damage > 0)
    {
        char message[128];

        if (entity->character.health.currentHP <= 0)
        {
            DefeatResult defeatResult = finalizeDefeat(*entity);
            combat.turnStartConditionDefeated = true;

            snprintf(message, sizeof(message),
                     "%s takes %d damage and dies!",
                     getEntityName(entity), result.damage);
            appendLevelUpFeedback(
                message, sizeof(message), defeatResult);
        }
        else if (entity->type == ENTITY_PLAYER)
        {
            snprintf(message, sizeof(message),
                     "You take %d damage.", result.damage);
        }
        else
        {
            snprintf(message, sizeof(message),
                     "%s takes %d damage.",
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

    if (result.poisonStageAdvanced > 0)
    {
        setGameMessage("The numbness is spreading.");
        combat.turnStartConditionPhase = TURN_START_CONDITION_EXPIRY_MESSAGE;
        needsRedraw = true;
        return;
    }

    if (result.poisonRecovered)
    {
        setGameMessage("The feeling slowly begins to return.");
        combat.turnStartConditionPhase = TURN_START_CONDITION_EXPIRY_MESSAGE;
        needsRedraw = true;
        return;
    }

    const char* actionStatus = entity->type == ENTITY_PLAYER
        ? getActionAffectingConditionMessage(entity->character)
        : nullptr;
    if (actionStatus != nullptr)
    {
        setGameMessage(actionStatus);
        combat.turnStartConditionPhase =
            TURN_START_CONDITION_ACTION_STATUS_MESSAGE;
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

    if (combat.turnStartConditionPhase !=
        TURN_START_CONDITION_ACTION_STATUS_MESSAGE)
    {
        const char* actionStatus = entity != nullptr &&
            entity->type == ENTITY_PLAYER
                ? getActionAffectingConditionMessage(entity->character)
                : nullptr;
        if (actionStatus != nullptr)
        {
            setGameMessage(actionStatus);
            combat.turnStartConditionPhase =
                TURN_START_CONDITION_ACTION_STATUS_MESSAGE;
            needsRedraw = true;
            return true;
        }
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
    if (entity->character.state == STATE_ALIVE)
    {
        const MapEffectTriggerResult hazardResult =
            handleStartingTurnMapEffects(*entity);
        if (hazardResult.targetDefeated ||
            entity->character.state != STATE_ALIVE)
        {
            result.actionPrevented = true;
        }
    }
    combat.turnStartActionPrevented = result.actionPrevented;
    beginTurnStartConditionMessages(entity, result);
}

void announceTurn(Entity* entity)
{
    entity->turn.movementRemaining =
        hasCondition(entity->character, CONDITION_WEBBED)
            ? 0
            : getEffectiveSpeed(entity->character);
    entity->turn.bonusAttacksRemaining =
        getActiveConditionModifiers(entity->character).bonusAttacks;

    entity->turn.standardActionUsed = false;
    entity->turn.monsterState = MONSTER_START;
    Serial.println(entity->character.speed);
}

void runMonsterTurn(Entity* monster)
{
    switch (monster->turn.monsterState)
    {
        case MONSTER_START:

            if (!shouldMonsterRunCombatAI(*monster))
            {
                Entity* player = getPlayerCombatant();

                if (player == nullptr ||
                    !tryMonsterDetectPlayer(*monster, *player))
                {
                    // This creature is in initiative because the room is in
                    // combat, but it still has no perceived target. It gets
                    // one normal detection chance and otherwise yields the
                    // turn without invoking movement or attack AI.
                    monster->turn.movementRemaining = 0;
                    monster->turn.standardActionUsed = true;
                    monster->turn.monsterState = MONSTER_END;
                    break;
                }

                char message[64];
                snprintf(message, sizeof(message), "%s notices you!",
                         getEntityName(monster));
                setGameMessage(message);
                needsRedraw = true;
            }

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
            combat.combatRound++;
            tickMapEffects();
            checkReinforcementAwareness();
            combat.currentTurnIndex = 0;
        }

        Entity* next = combat.initiativeOrder[combat.currentTurnIndex];

        if (next != nullptr && next->active &&
            next->character.state == STATE_ALIVE &&
            reinforcementMayAct(*next, combat.combatRound))
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
    if (entity == nullptr ||
        (!combat.turnStartActionPrevented &&
         canCharacterAct(entity->character)))
    {
        return false;
    }

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
                        damageCharacter(
                            target->character, combat.pendingDamage);

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
    clearMapEffects();
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

    if (gameState == GAME_DUNGEON)
        updateCurrentDungeonRoomCompletion(dungeon);

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
    combat.waitingForPlayer = false;
    combat.endPlayerTurnAfterMessage = false;
    combat.openingAttackTargeting = false;
    combat.openingAttackInProgress = false;
    combat.openingAttackWasAmbush = false;
    combat.iterativeAttackActive = false;
    combat.iterativeAttackIndex = 0;
    combat.iterativeAttackCount = 1;
    combat.monsterIterativeAttackActive = false;
    combat.monsterIterativeAttackIndex = 0;
    combat.monsterIterativeAttackCount = 1;
    combat.selectedAbility = ABILITY_NONE;
    combat.selectedAbilityX = -1;
    combat.selectedAbilityY = -1;
    combat.selectedAbilityDirection = moveDirection;
    combat.selectedAbilityDamageType = DAMAGE_NONE;
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

    if (attackType == COMBAT_ATTACK_RANGED && !canSee(*player))
    {
        setGameMessage("You cannot see a ranged target.");
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

bool canPlayerAttackOutsideCombat(CombatAttackType attackType)
{
    Entity* player = getActiveMapPlayer();
    if (combat.active || player == nullptr || !canCharacterAct(player->character))
        return false;
    const Weapon* weapon = attackType == COMBAT_ATTACK_MELEE
        ? getEquippedMeleeWeapon(player->character)
        : getEquippedRangedWeapon(player->character);
    if (weapon == nullptr)
        return false;
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    for (uint8_t i = 0; entities != nullptr && i < count; i++)
    {
        const bool validGeometry = attackType == COMBAT_ATTACK_MELEE
            ? getEntityGridDistance(*player, entities[i]) <= 1
            : isValidRangedTarget(player, &entities[i]);

        if (isLivingHostileForCombat(entities[i]) &&
            entities[i].revealedToPlayer &&
            areHostile(*player, entities[i]) && validGeometry)
        {
            return true;
        }
    }
    return false;
}

void beginOutOfCombatAttack(CombatAttackType attackType)
{
    if (!canPlayerAttackOutsideCombat(attackType))
        return;
    combat.openingAttackTargeting = true;
    combat.attackType = attackType;
    combat.selectedTargetIndex = -1;
    if (attackType == COMBAT_ATTACK_RANGED)
        rotateAttackTarget(true);
    setGameMessage(attackType == COMBAT_ATTACK_MELEE
        ? "Choose direction: A attack B back"
        : "Choose target: A attack B back");
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
    Entity* player = combat.openingAttackTargeting
        ? getActiveMapPlayer()
        : getPlayerCombatant();
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

        return target != nullptr && target->revealedToPlayer &&
               areHostile(*player, *target) &&
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

    return target->revealedToPlayer && areHostile(*player, *target) &&
           isValidRangedTarget(player, target) ? target : nullptr;
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
    Entity* player = combat.openingAttackTargeting
        ? getActiveMapPlayer()
        : getCurrentCombatant();
    Entity* target = getSelectedAttackTarget();

    if (player == nullptr || target == nullptr ||
        !canCharacterAct(player->character))
    {
        setGameMessage("No target selected.");
        needsRedraw = true;
        return;
    }

    if (combat.openingAttackTargeting)
    {
        const bool targetWasAware = target->awareOfPlayer;
        const bool targetWasAlreadyFlatFooted =
            hasCondition(target->character, CONDITION_FLAT_FOOTED);
        target->awareOfPlayer = true;
        target->revealedToPlayer = true;
        startCombat();
        combat.openingAttackInProgress = true;
        combat.openingAttackWasAmbush = !targetWasAware;
        for (uint8_t i = 0; i < combat.combatantCount; i++)
            if (combat.initiativeOrder[i] == player)
                combat.currentTurnIndex = i;
        combat.phase = COMBAT_TURN;
        combat.waitingForPlayer = true;
        combat.openingAttackTargeting = false;
        if (!targetWasAware)
            addCondition(target->character, CONDITION_FLAT_FOOTED, 1, 1);
        else if (!targetWasAlreadyFlatFooted)
            removeFlatFooted(target);
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

    if (!combat.iterativeAttackActive)
    {
        const EquipmentSlot slot = combat.attackType == COMBAT_ATTACK_MELEE
            ? SLOT_MELEE_WEAPON
            : SLOT_RANGED_WEAPON;
        const ItemID weaponID = player->character.equipment.equipped[slot].itemID;
        const int bab = getBaseAttackBonus(
            player->character.characterClass, player->character.level);

        combat.iterativeAttackActive = true;
        combat.iterativeAttackIndex = 0;
        const bool fullAttack =
            !combat.openingAttackInProgress &&
            !isNaturalWeaponItem(weaponID) && canMakeFullAttack(*player);
        const uint8_t bonusAttacks = fullAttack
            ? player->turn.bonusAttacksRemaining : 0;
        combat.iterativeAttackCount = fullAttack
            ? getIterativeAttackCount(bab) + bonusAttacks : 1;
        if (fullAttack)
            player->turn.bonusAttacksRemaining = 0;
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
    const int bab = getBaseAttackBonus(
        player->character.characterClass, player->character.level);
    const int iterativeBAB = getSequenceAttackBAB(
        bab, combat.iterativeAttackIndex,
        combat.iterativeAttackCount > getIterativeAttackCount(bab)
            ? combat.iterativeAttackCount - getIterativeAttackCount(bab) : 0);
    const int iterativePenalty = iterativeBAB - bab;
    int attackBonus = normalAttackBonus + powerAttackPenalty +
        iterativePenalty;
    int total = dieRoll + attackBonus + rangePenalty;
    const int targetArmorClass = getArmorClass(
        target->character, target->turn.fullDefense ? 4 : 0);
    bool hit = (dieRoll == 20) ||
               (dieRoll != 1 && total >= targetArmorClass);
    bool criticalConfirmed = false;

    if (hit && dieRoll >= getWeaponCriticalThreatMinimum(
            player->character, *weapon))
    {
        if (fighterAutomaticallyConfirmsCritical(player->character, *weapon))
        {
            criticalConfirmed = true;
        }
        else
        {
            const int confirmationRoll = rollDie(20);
            const int confirmationTotal = confirmationRoll + attackBonus +
                                          rangePenalty;
            criticalConfirmed = confirmationRoll == 20 ||
                (confirmationRoll != 1 &&
                 confirmationTotal >= targetArmorClass);
        }
    }

    char message[64];

    snprintf(message, sizeof(message), "Attack %+d: %s! (%d)",
             iterativeBAB,
             criticalConfirmed ? "Critical" : (hit ? "Hit" : "Miss"),
             total);

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

        combat.pendingDamage += getFighterWeaponDamageBonus(
            player->character, *weapon);
        combat.pendingDamage += getActiveConditionModifiers(
            player->character).damageBonus;

        combat.pendingDamage = std::max(1, combat.pendingDamage);

        if (criticalConfirmed)
            combat.pendingDamage *= weapon->criticalMultiplier;

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
    combat.iterativeAttackActive = false;
    combat.iterativeAttackIndex = 0;
    combat.iterativeAttackCount = 1;
    combat.openingAttackTargeting = false;
    combat.selectedTargetIndex = -1;
    markPlayerFacingCursorDirty();
    requestCombatTileRedraw();
    openMenu(&mainMenu);
}

static void markAbilityCursorDirty()
{
    if (isPlayerTargetingDirectionalAbility())
    {
        Entity* caster = getPlayerCombatant();

        if (caster != nullptr)
        {
            for (int y = 0; y < getActiveMapHeight(); y++)
            {
                for (int x = 0; x < getActiveMapWidth(); x++)
                {
                    if (isTileInDirectionalAbilityArea(
                            *caster,
                            combat.selectedAbility,
                            combat.selectedAbilityDirection,
                            x,
                            y))
                    {
                        markTileDirty(x, y);
                    }
                }
            }
        }

        return;
    }

    if (isPlayerTargetingGroundAbility())
    {
        int targetX = 0;
        int targetY = 0;
        const Ability* ability = getAbility(combat.selectedAbility);

        if (ability != nullptr &&
            getSelectedAbilityGroundTarget(targetX, targetY))
        {
            for (int y = targetY - ability->areaRadiusTiles;
                 y <= targetY + ability->areaRadiusTiles;
                 y++)
            {
                for (int x = targetX - ability->areaRadiusTiles;
                     x <= targetX + ability->areaRadiusTiles;
                     x++)
                {
                    if (isInsideActiveMap(x, y))
                        markTileDirty(x, y);
                }
            }
        }

        return;
    }

    Entity* target = getSelectedAbilityTarget();

    if (target != nullptr)
        markEntityFootprintDirty(*target);
}

static bool consumeSelectedAbilityScroll(Entity& player);
static void clearSelectedAbilityScroll();

static void executePlayerAbility(
    Entity& player,
    Entity* target,
    AbilityID abilityID)
{
    AbilityResolution resolution = resolveAbility(
        player, target, abilityID,
        combat.selectedAbilityFromScroll
            ? AbilityCastSource::SCROLL : AbilityCastSource::NORMAL,
        combat.selectedAbilityDamageType);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
    {
        setGameMessage(getAbilityResultMessage(resolution.result));
        playSound(SoundEffect::SPELL_FAIL);
        requestCombatTileRedraw();
        return;
    }

    if (!consumeSelectedAbilityScroll(player))
    {
        setGameMessage("Scroll is no longer available.");
        return;
    }
    clearSelectedAbilityScroll();

    presentAbilityResolution(
        player,
        target != nullptr ? *target : player,
        abilityID,
        resolution);
}

static bool isEligibleTurnUndeadTarget(
    const Entity& cleric,
    const Entity& target,
    const Ability& ability)
{
    return target.active && target.type == ENTITY_MONSTER &&
           target.character.state == STATE_ALIVE &&
           areHostile(cleric, target) && isUndeadCreature(target) &&
           getEntityGridDistance(cleric, target) <= ability.rangeTiles &&
           hasLineOfSightBetweenFootprintsAt(
               cleric, cleric.x, cleric.y, target);
}

static void appendTurnedName(
    char* names,
    size_t namesSize,
    const char* name,
    uint8_t count)
{
    if (names[0] != '\0')
        strncat(names, ", ", namesSize - strlen(names) - 1);

    strncat(names, name, namesSize - strlen(names) - 1);

    if (count > 1)
    {
        char suffix[8];
        snprintf(suffix, sizeof(suffix), " x%u",
                 static_cast<unsigned int>(count));
        strncat(names, suffix, namesSize - strlen(names) - 1);
    }
}

static void executeTurnUndead(Entity& cleric)
{
    const Ability* ability = getAbility(ABILITY_TURN_UNDEAD);
    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);
    int turned = 0;
    const char* turnedNames[MAX_ENTITIES] = {};
    uint8_t turnedCounts[MAX_ENTITIES] = {};
    uint8_t turnedKinds = 0;

    if (ability == nullptr || !combat.active ||
        getCurrentCombatant() != &cleric ||
        !combat.waitingForPlayer ||
        !knowsAbility(cleric.character, ABILITY_TURN_UNDEAD) ||
        !canCharacterAct(cleric.character) || cleric.turn.standardActionUsed)
    {
        setGameMessage("Turn Undead unavailable.");
        playSound(SoundEffect::SPELL_FAIL);
        return;
    }

    if (cleric.character.magic.currentMP < ability->mpCost ||
        entities == nullptr)
    {
        setGameMessage(entities == nullptr ? "No undead were turned" :
                       "Not enough MP.");
        playSound(SoundEffect::SPELL_FAIL);
        return;
    }

    bool hasEligibleTarget = false;
    for (uint8_t i = 0; i < entityCount; i++)
    {
        if (isEligibleTurnUndeadTarget(cleric, entities[i], *ability))
        {
            hasEligibleTarget = true;
            break;
        }
    }

    if (!hasEligibleTarget)
    {
        setGameMessage("No undead were turned");
        playSound(SoundEffect::SPELL_FAIL);
        return;
    }

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& target = entities[i];
        if (!isEligibleTurnUndeadTarget(cleric, target, *ability))
            continue;

        AbilitySavingThrow save = resolveAbilitySavingThrow(
            cleric, target, *ability);
        if (save.result == SAVE_RESULT_FAILURE)
        {
            // Turn Undead defeats the monster through the same one-time path
            // as weapon and spell damage.  This creates an ordinary lootable
            // corpse and keeps XP, room completion, and combat completion in
            // sync with every other defeated monster.
            finalizeDefeat(target);
            target.turn.movementRemaining = 0;
            target.turn.standardActionUsed = true;
            markEntityFootprintDirty(target);
            const char* name = getEntityName(&target);
            uint8_t kind = 0;
            for (; kind < turnedKinds; kind++)
                if (strcmp(turnedNames[kind], name) == 0)
                    break;
            if (kind == turnedKinds && turnedKinds < MAX_ENTITIES)
                turnedNames[turnedKinds++] = name;
            turnedCounts[kind]++;
            turned++;
        }
    }

    cleric.character.magic.currentMP -= ability->mpCost;
    cleric.turn.standardActionUsed = true;
    if (turned == 0)
        setGameMessage("No undead were turned");
    else
    {
        char names[72] = {};
        for (uint8_t i = 0; i < turnedKinds; i++)
            appendTurnedName(names, sizeof(names), turnedNames[i],
                             turnedCounts[i]);
        char message[112];
        snprintf(message, sizeof(message), "Turned %d undead: %s", turned, names);
        setGameMessage(message);
    }
    playSound(SoundEffect::SPELL_CAST);
    combat.abilityResolutionPending = true;
    combat.abilityCaster = &cleric;
    combat.abilityEndedCombat = areAllCombatMonstersDefeated();
    combat.abilityResultTime = millis();
    combat.waitingForPlayer = false;
    requestCombatTileRedraw();
}

void presentDirectionalAbilityResolution(
    Entity& caster,
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

    char message[96];
    snprintf(
        message,
        sizeof(message),
        "%s: %u affected, %u resist, %u immune.",
        getAbilityName(abilityID),
        static_cast<unsigned int>(resolution.targetsAffected),
        static_cast<unsigned int>(resolution.targetsResisted),
        static_cast<unsigned int>(resolution.targetsImmune));
    setGameMessage(message);
    playSound(SoundEffect::SPELL_CAST);
    requestCombatTileRedraw();

    if (!combat.active)
        return;

    combat.abilityResolutionPending = true;
    combat.abilityCaster = &caster;
    combat.abilityEndedCombat = false;
    combat.abilityResultTime = millis();

    if (caster.type == ENTITY_PLAYER)
        combat.waitingForPlayer = false;
}

void beginPlayerAbility(AbilityID abilityID)
{
    Entity* player = combat.active
        ? getCurrentCombatant() : getActiveMapPlayer();
    const Ability* ability = getAbility(abilityID);

    if (player == nullptr || player->type != ENTITY_PLAYER ||
        (combat.active && !combat.waitingForPlayer) ||
        (!combat.selectedAbilityFromScroll && !knowsAbility(player->character, abilityID) &&
         !(player->character.characterClass == CLASS_CLERIC &&
           ability != nullptr && ability->type == ABILITY_DIVINE &&
           ability->category == ABILITY_CATEGORY_SPELL &&
           ability->level <= getClericSpellAccessLevel(player->character))) ||
        !isAbilitySupported(abilityID) ||
        ability == nullptr)
    {
        setGameMessage("Ability unavailable.");
        playSound(SoundEffect::SPELL_FAIL);
        return;
    }

    if ((abilityID == ABILITY_RESIST_ENERGY ||
         abilityID == ABILITY_RESIST_ENERGY_ARCANE ||
         abilityID == ABILITY_PROTECTION_FROM_ENERGY ||
         abilityID == ABILITY_PROTECTION_FROM_ENERGY_ARCANE) &&
        combat.selectedAbilityDamageType == DAMAGE_NONE)
    {
        combat.selectedAbility = abilityID;
        openResistEnergyMenu();
        return;
    }

    if (ability->target == TARGET_SELF)
    {
        if (abilityID == ABILITY_TURN_UNDEAD)
        {
            executeTurnUndead(*player);
            return;
        }
        executePlayerAbility(*player, player, abilityID);
        return;
    }

    combat.selectedAbility = abilityID;
    combat.selectedTargetIndex = -1;
    combat.selectedAbilityDirection = moveDirection;

    if (isDirectionalAbility(abilityID))
    {
        AbilityResult validation = validateDirectionalAbility(
            *player, abilityID,
            combat.selectedAbilityFromScroll
                ? AbilityCastSource::SCROLL : AbilityCastSource::NORMAL);

        if (validation != ABILITY_RESULT_SUCCESS)
        {
            combat.selectedAbility = ABILITY_NONE;
            combat.selectedAbilityDamageType = DAMAGE_NONE;
            setGameMessage(getAbilityResultMessage(validation));
            playSound(SoundEffect::SPELL_FAIL);
            requestCombatTileRedraw();
            return;
        }

        combat.selectedAbilityX = -1;
        combat.selectedAbilityY = -1;
        markAbilityCursorDirty();
        setGameMessage("Choose a spell direction");
        requestCombatTileRedraw();
        return;
    }

    if (isGroundTargetAbility(abilityID))
    {
        if (!selectInitialGroundAbilityTarget(*player, *ability))
        {
            combat.selectedAbility = ABILITY_NONE;
            combat.selectedAbilityX = -1;
            combat.selectedAbilityY = -1;
            combat.selectedAbilityDamageType = DAMAGE_NONE;
            setGameMessage("No valid ground targets.");
            playSound(SoundEffect::SPELL_FAIL);
            requestCombatTileRedraw();
            return;
        }

        markAbilityCursorDirty();
        setGameMessage("Choose a spell target");
        requestCombatTileRedraw();
        return;
    }

    combat.selectedAbilityX = -1;
    combat.selectedAbilityY = -1;
    if (!selectNextEntityTarget(player, true, true))
    {
        combat.selectedAbility = ABILITY_NONE;
        combat.selectedAbilityDamageType = DAMAGE_NONE;
        setGameMessage("No valid targets.");
        playSound(SoundEffect::SPELL_FAIL);
        requestCombatTileRedraw();
        return;
    }

    markAbilityCursorDirty();
    setGameMessage("Choose target: A cast B back");
    requestCombatTileRedraw();
}

void beginPlayerScrollAbility(AbilityID abilityID, const ItemInstance& scroll)
{
    Entity* player = getCurrentCombatant();
    const Ability* ability = getAbility(abilityID);
    if (player == nullptr || ability == nullptr || !hasItem(player->character, scroll) ||
        (player->character.characterClass == CLASS_WIZARD && ability->type != ABILITY_ARCANE) ||
        (player->character.characterClass == CLASS_CLERIC && ability->type != ABILITY_DIVINE) ||
        (player->character.characterClass != CLASS_WIZARD &&
         player->character.characterClass != CLASS_CLERIC))
    {
        setGameMessage("Cannot cast that scroll.");
        playSound(SoundEffect::SPELL_FAIL);
        return;
    }

    combat.selectedAbilityFromScroll = true;
    combat.selectedAbilityScroll = scroll;
    beginPlayerAbility(abilityID);
    if (combat.selectedAbility == ABILITY_NONE && !combat.abilityResolutionPending)
    {
        clearSelectedAbilityScroll();
    }
}

void continuePlayerAbilityWithDamageType(DamageType damageType)
{
    const AbilityID abilityID = combat.selectedAbility;
    if (abilityID == ABILITY_NONE)
        return;

    combat.selectedAbilityDamageType = damageType;
    beginPlayerAbility(abilityID);
}

bool isPlayerTargetingAbility()
{
    return combat.selectedAbility != ABILITY_NONE &&
           !combat.abilityResolutionPending;
}

bool isPlayerTargetingGroundAbility()
{
    return isPlayerTargetingAbility() &&
           isGroundTargetAbility(combat.selectedAbility);
}

bool isPlayerTargetingDirectionalAbility()
{
    return isPlayerTargetingAbility() &&
           isDirectionalAbility(combat.selectedAbility);
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
        isPlayerTargetingGroundAbility() ||
        isPlayerTargetingDirectionalAbility() ||
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

bool getSelectedAbilityGroundTarget(int& x, int& y)
{
    if (!isPlayerTargetingGroundAbility() ||
        !isInsideActiveMap(
            combat.selectedAbilityX,
            combat.selectedAbilityY))
    {
        return false;
    }

    x = combat.selectedAbilityX;
    y = combat.selectedAbilityY;
    return true;
}

void rotateAbilityTarget(bool forward)
{
    if (!isPlayerTargetingAbility())
        return;

    markAbilityCursorDirty();

    if (isPlayerTargetingGroundAbility() ||
        isPlayerTargetingDirectionalAbility())
    {
        int direction = static_cast<int>(combat.selectedAbilityDirection);
        direction = (direction + (forward ? 1 : 7)) % 8;
        combat.selectedAbilityDirection = static_cast<Direction>(direction);
    }
    else
        selectNextEntityTarget(getPlayerCombatant(), forward, true);

    markAbilityCursorDirty();
    requestCombatTileRedraw();
}

bool moveGroundAbilityTarget()
{
    if (!isPlayerTargetingGroundAbility())
        return false;

    Entity* caster = getPlayerCombatant();
    const Ability* ability = getAbility(combat.selectedAbility);

    if (caster == nullptr || ability == nullptr)
        return false;

    const DirectionOffset& offset =
        directionOffsets[combat.selectedAbilityDirection];
    int targetX = combat.selectedAbilityX + offset.dx;
    int targetY = combat.selectedAbilityY + offset.dy;

    // Keep the preview on a castable center. This enforces map bounds, range,
    // and the same active-map LOS convention used by resolveAbilityAt().
    if (!isValidGroundAbilitySelection(
            caster, ability, targetX, targetY))
    {
        return false;
    }

    markAbilityCursorDirty();
    combat.selectedAbilityX = static_cast<int8_t>(targetX);
    combat.selectedAbilityY = static_cast<int8_t>(targetY);
    markAbilityCursorDirty();
    setGameMessage("Choose a spell target");
    requestCombatTileRedraw();
    return true;
}

static bool consumeSelectedAbilityScroll(Entity& player)
{
    return !combat.selectedAbilityFromScroll ||
           removeItem(player.character, combat.selectedAbilityScroll, 1);
}

static void clearSelectedAbilityScroll()
{
    combat.selectedAbilityFromScroll = false;
    combat.selectedAbilityScroll = { ITEM_NONE, 0, WEAPON_ENHANCEMENT_NONE };
    combat.selectedAbilityDamageType = DAMAGE_NONE;
}

void confirmPlayerAbility()
{
    Entity* player = combat.active
        ? getCurrentCombatant() : getActiveMapPlayer();

    if (player != nullptr && isPlayerTargetingDirectionalAbility())
    {
        AbilityID abilityID = combat.selectedAbility;
        AbilityResolution resolution = resolveAbilityInDirection(
            *player, combat.selectedAbilityDirection, abilityID,
            combat.selectedAbilityFromScroll
                ? AbilityCastSource::SCROLL : AbilityCastSource::NORMAL);

        if (resolution.result != ABILITY_RESULT_SUCCESS)
        {
            setGameMessage(getAbilityResultMessage(resolution.result));
            playSound(SoundEffect::SPELL_FAIL);
            requestCombatTileRedraw();
            return;
        }
        if (!consumeSelectedAbilityScroll(*player))
        {
            setGameMessage("Scroll is no longer available.");
            return;
        }

        markAbilityCursorDirty();
        combat.selectedAbility = ABILITY_NONE;
        combat.selectedAbilityX = -1;
        combat.selectedAbilityY = -1;
        combat.selectedAbilityDirection = moveDirection;
        combat.selectedTargetIndex = -1;
        clearSelectedAbilityScroll();
        markPlayerFacingCursorDirty();
        presentDirectionalAbilityResolution(
            *player, abilityID, resolution);
        return;
    }

    if (player != nullptr && isPlayerTargetingGroundAbility())
    {
        int targetX = 0;
        int targetY = 0;

        if (!getSelectedAbilityGroundTarget(targetX, targetY))
        {
            setGameMessage("Invalid target.");
            playSound(SoundEffect::SPELL_FAIL);
            requestCombatTileRedraw();
            return;
        }

        AbilityID abilityID = combat.selectedAbility;
        AbilityResolution resolution = resolveAbilityAt(
            *player, targetX, targetY, abilityID,
            combat.selectedAbilityFromScroll
                ? AbilityCastSource::SCROLL : AbilityCastSource::NORMAL);

        if (resolution.result != ABILITY_RESULT_SUCCESS)
        {
            setGameMessage(getAbilityResultMessage(resolution.result));
            playSound(SoundEffect::SPELL_FAIL);
            requestCombatTileRedraw();
            return;
        }
        if (!consumeSelectedAbilityScroll(*player))
        {
            setGameMessage("Scroll is no longer available.");
            return;
        }

        markAbilityCursorDirty();
        combat.selectedAbility = ABILITY_NONE;
        combat.selectedAbilityX = -1;
        combat.selectedAbilityY = -1;
        combat.selectedAbilityDirection = moveDirection;
        combat.selectedTargetIndex = -1;
        clearSelectedAbilityScroll();
        markPlayerFacingCursorDirty();
        presentGroundAbilityResolution(*player, abilityID, resolution);
        return;
    }

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
        *player, target, abilityID,
        combat.selectedAbilityFromScroll
            ? AbilityCastSource::SCROLL : AbilityCastSource::NORMAL);

    if (resolution.result != ABILITY_RESULT_SUCCESS)
    {
        setGameMessage(getAbilityResultMessage(resolution.result));
        playSound(SoundEffect::SPELL_FAIL);
        requestCombatTileRedraw();
        return;
    }
    if (!consumeSelectedAbilityScroll(*player))
    {
        setGameMessage("Scroll is no longer available.");
        return;
    }

    markAbilityCursorDirty();
    combat.selectedAbility = ABILITY_NONE;
    combat.selectedAbilityX = -1;
    combat.selectedAbilityY = -1;
    combat.selectedAbilityDirection = moveDirection;
    combat.selectedTargetIndex = -1;
    clearSelectedAbilityScroll();
    markPlayerFacingCursorDirty();

    presentAbilityResolution(
        *player, *target, abilityID, resolution);
}

void cancelPlayerAbility()
{
    if (!isPlayerTargetingAbility())
        return;

    bool wasSpatialTargeting = isPlayerTargetingGroundAbility() ||
                               isPlayerTargetingDirectionalAbility();
    markAbilityCursorDirty();
    combat.selectedAbility = ABILITY_NONE;
    combat.selectedAbilityX = -1;
    combat.selectedAbilityY = -1;
    combat.selectedAbilityDirection = moveDirection;
    combat.selectedTargetIndex = -1;
    clearSelectedAbilityScroll();
    markPlayerFacingCursorDirty();
    requestCombatTileRedraw();

    if (wasSpatialTargeting)
        setGameMessage("Spell targeting canceled.");
    else
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
        player->turn.movementRemaining != getEffectiveSpeed(player->character) ||
        !canCharacterAct(player->character))
    {
        setGameMessage("Double Move unavailable.");
        return;
    }

    player->turn.movementRemaining = getEffectiveSpeed(player->character) * 2;
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
        !hasFighterFeature(fighter.character, FIGHTER_POWER_ATTACK) ||
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

    const ItemID weaponID = getEquippedItem(monster->character, weaponSlot);

    if (!combat.monsterIterativeAttackActive)
    {
        combat.monsterIterativeAttackActive = true;
        combat.monsterIterativeAttackIndex = 0;
        const bool fullAttack = !isNaturalWeaponItem(weaponID) &&
            canMakeFullAttack(*monster);
        const uint8_t bonusAttacks = fullAttack
            ? monster->turn.bonusAttacksRemaining : 0;
        combat.monsterIterativeAttackCount = fullAttack
            ? getIterativeAttackCount(monster->monster->baseAttack) + bonusAttacks
            : 1;
        if (fullAttack)
            monster->turn.bonusAttacksRemaining = 0;
    }

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
    const int baseIteratives = getIterativeAttackCount(monster->monster->baseAttack);
    const int iterativeBAB = getSequenceAttackBAB(
        monster->monster->baseAttack, combat.monsterIterativeAttackIndex,
        combat.monsterIterativeAttackCount > baseIteratives
            ? combat.monsterIterativeAttackCount - baseIteratives : 0);
    int total = dieRoll + iterativeBAB + abilityModifier +
                getConditionAttackModifier(monster->character) +
                rangePenalty;

    const int targetArmorClass = getArmorClass(
        target->character, target->turn.fullDefense ? 4 : 0);

    combat.monsterAttackHit = (dieRoll == 20) ||
        (dieRoll != 1 && total >= targetArmorClass);
    bool criticalConfirmed = false;

    if (combat.monsterAttackHit && dieRoll >= weapon->criticalThreat)
    {
        const int confirmationRoll = rollDie(20);
        const int confirmationTotal = confirmationRoll + iterativeBAB +
            abilityModifier + getConditionAttackModifier(monster->character) +
            rangePenalty;
        criticalConfirmed = confirmationRoll == 20 ||
            (confirmationRoll != 1 &&
             confirmationTotal >= targetArmorClass);
    }
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

        combat.monsterPendingDamage += getActiveConditionModifiers(
            monster->character).damageBonus;

        combat.monsterPendingDamage = std::max(
            1, combat.monsterPendingDamage);

        if (criticalConfirmed)
            combat.monsterPendingDamage *= weapon->criticalMultiplier;

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
    snprintf(message, sizeof(message), "%s attack %+d with %s%s",
             getEntityName(monster),
             iterativeBAB,
             getEquippedItemName(
                  monster->character,
                  weaponSlot),
             criticalConfirmed ? " (critical)." : ".");
    setGameMessage(message);
    requestCombatTileRedraw();
}

bool isMonsterAttackResolving()
{
    return combat.monsterAttackPhase != MONSTER_ATTACK_NONE;
}
