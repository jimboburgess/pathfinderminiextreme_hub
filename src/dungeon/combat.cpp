#include "combat.h"

#include <algorithm>
#include <stdlib.h>

#include "activemap.h"
#include "loot.h"
#include "monsterscripts.h"
#include "audio/audio.h"
#include "data/dice.h"
#include "data/entityspawn.h"
#include "data/game.h"
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

static int gridDistanceBetweenFootprints(
    const Entity* first,
    const Entity* second)
{
    int firstRight = first->x + getEntityTileWidth(*first) - 1;
    int firstBottom = first->y + getEntityTileHeight(*first) - 1;
    int secondRight = second->x + getEntityTileWidth(*second) - 1;
    int secondBottom = second->y + getEntityTileHeight(*second) - 1;

    int horizontalDistance = 0;
    int verticalDistance = 0;

    if (firstRight < second->x)
        horizontalDistance = second->x - firstRight;
    else if (secondRight < first->x)
        horizontalDistance = first->x - secondRight;

    if (firstBottom < second->y)
        verticalDistance = second->y - firstBottom;
    else if (secondBottom < first->y)
        verticalDistance = first->y - secondBottom;

    return std::max(horizontalDistance, verticalDistance);
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

static uint32_t getDefeatedMonsterExperience()
{
    uint32_t totalExperience = 0;

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        if (entity == nullptr || !isMonsterCombatant(*entity) ||
            entity->character.state == STATE_ALIVE)
        {
            continue;
        }

        const Monster* monster = entity->monster;

        if (monster == nullptr)
            monster = getMonster(entity->monsterID);

        if (monster != nullptr)
            totalExperience += getExperienceAward(monster->challengeRating);
    }

    return totalExperience;
}

static uint32_t awardCombatExperience(uint8_t& playerCount)
{
    playerCount = 0;

    if (combat.experienceAwarded || !areAllCombatMonstersDefeated())
        return 0;

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        if (entity != nullptr && isPlayerSideCharacter(*entity))
            playerCount++;
    }

    if (playerCount == 0)
        return 0;

    // The exact Pathfinder award method totals defeated creatures' CR XP,
    // then divides that encounter total equally among participating PCs.
    uint32_t experiencePerCharacter =
        getDefeatedMonsterExperience() / playerCount;

    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* entity = combat.initiativeOrder[i];

        if (entity != nullptr && isPlayerSideCharacter(*entity))
            entity->character.xp += experiencePerCharacter;
    }

    combat.experienceAwarded = true;
    return experiencePerCharacter;
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
            if (combat.monsterAttackHit && target != nullptr &&
                target->character.state == STATE_ALIVE)
            {
                target->character.health.currentHP -= combat.monsterPendingDamage;

                char message[64];

                if (target->character.health.currentHP <= 0)
                {
                    target->character.health.currentHP = 0;
                    target->character.state = STATE_DEAD;
                    generateCorpseLoot(*target);
                    combat.monsterDefeatedPlayer = true;

                    snprintf(message, sizeof(message),
                             "Player takes %d damage and falls!",
                             combat.monsterPendingDamage);
                }
                else
                {
                    snprintf(message, sizeof(message),
                             "Player takes %d damage!",
                             combat.monsterPendingDamage);
                }

                setGameMessage(message);
                // A target can cover more than one map tile (for example,
                // the giant spider).  Redraw its entire footprint so damage
                // and the dead/loot marker cannot leave stale sprite pixels.
                markEntityFootprintDirty(*target);
            }

            combat.monsterAttackPhase = MONSTER_ATTACK_COMPLETE;
            combat.monsterAttackTime = millis();
            requestCombatTileRedraw();
            break;
        }

        case MONSTER_ATTACK_COMPLETE:
            combat.monsterAttackPhase = MONSTER_ATTACK_NONE;
            combat.attackingMonster = nullptr;
            combat.monsterAttackTarget = nullptr;
            combat.monsterPendingDamage = 0;

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

    if (entities == nullptr || playerEntity == nullptr)
        return;

    for (uint8_t i = 0; i < entityCount; i++)
    {
        Entity& monster = entities[i];

        if (!isMonsterCombatant(monster))
            continue;

        int distance = gridDistanceBetweenFootprints(playerEntity, &monster);

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

    combat.phase = COMBAT_INITIATIVE;
    combat.phaseStartTime = millis();
    combat.initiativeMessageShown = false;

    combat.currentTurnIndex = 0;
    combat.combatRound = 1;
    combat.experienceAwarded = false;

    backgroundNeedsRedraw = true;

    redrawType = REDRAW_FULL;
    needsRedraw = true;

}

void resetActions(Entity* entity)
{
    if (entity == nullptr)
        return;

    entity->turn.moveActionUsed = false;
    entity->turn.standardActionUsed = false;
    entity->turn.fullDefense = false;
    entity->turn.fiveFootStepUsed = false;
    entity->turn.delayTurn = false;
    entity->turn.turnActive = true;
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

                Entity* current = getCurrentCombatant();

                if (current->type == ENTITY_PLAYER)
                    setGameMessage("Player Turn");
                else
                    setGameMessage("Monster Turn");
            }

            break;

       case COMBAT_TURN:
       {
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

                       char message[64];

                       if (target->character.health.currentHP <= 0)
                       {
                           target->character.health.currentHP = 0;
                           target->character.state = STATE_DEAD;
                           generateCorpseLoot(*target);

                           snprintf(message, sizeof(message),
                                    "%s takes %d damage and dies!",
                                    getEntityName(target), combat.pendingDamage);
                       }
                       else
                       {
                           snprintf(message, sizeof(message),
                                    "%s takes %d damage!",
                                    getEntityName(target), combat.pendingDamage);
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
    uint8_t playerCount = 0;
    uint32_t experiencePerCharacter = awardCombatExperience(playerCount);

    if (experiencePerCharacter > 0)
    {
        char message[64];

        if (playerCount == 1)
        {
            snprintf(message, sizeof(message),
                     "Victory! You gain %lu XP.",
                     static_cast<unsigned long>(experiencePerCharacter));
        }
        else
        {
            snprintf(message, sizeof(message),
                     "Victory! Party gains %lu XP each.",
                     static_cast<unsigned long>(experiencePerCharacter));
        }

        setGameMessage(message);
    }

    combat.active = false;
    combat.phase = COMBAT_NONE;
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
        player->turn.standardActionUsed)
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
        Entity* player = getPlayerCombatant();
        uint8_t entityCount = 0;
        Entity* entities = getActiveMapEntities(entityCount);

        if (player == nullptr || entities == nullptr || entityCount == 0)
            return;

        int start = combat.selectedTargetIndex;

        for (uint8_t checked = 0; checked < entityCount; checked++)
        {
            start += forward ? 1 : -1;

            if (start < 0)
                start = entityCount - 1;
            else if (start >= entityCount)
                start = 0;

            if (isValidRangedTarget(player, &entities[start]))
            {
                combat.selectedTargetIndex = start;
                break;
            }
        }
    }

    markAttackCursorDirty();
    requestCombatTileRedraw();
}

void confirmPlayerAttack()
{
    Entity* player = getCurrentCombatant();
    Entity* target = getSelectedAttackTarget();

    if (player == nullptr || target == nullptr)
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
    int attackBonus =
        (combat.attackType == COMBAT_ATTACK_MELEE)
            ? getMeleeAttackBonus(player->character)
            : getRangedAttackBonus(player->character);
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
    else
    {
        snprintf(message, sizeof(message), "%s! %d + %d = %d",
                 hit ? "Hit" : "Miss", dieRoll, attackBonus, total);
    }

    setGameMessage(message);

    combat.pendingAttackTarget = target;
    combat.pendingDamage = 0;

    if (hit)
    {
        combat.pendingDamage = rollDice(weapon->damageDice, weapon->damageSides);

        if (combat.attackType == COMBAT_ATTACK_MELEE)
        {
            combat.pendingDamage += getAbilityModifier(
                player->character, ABILITY_STRENGTH);
        }

        combat.pendingDamage = std::max(1, combat.pendingDamage);
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
        player->turn.movementRemaining != player->character.speed)
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
        player->turn.moveActionUsed)
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

void beginMonsterAttack(
    Entity* monster,
    Entity* target,
    CombatAttackType attackType)
{
    if (monster == nullptr || target == nullptr ||
        monster->monster == nullptr ||
        isMonsterAttackResolving())
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
            gridDistanceBetweenFootprints(monster, target) * 5;
        int incrementsBeyondFirst =
            (distanceFeet - 1) / weapon->rangeIncrement;

        rangePenalty = -2 * incrementsBeyondFirst;
    }

    int dieRoll = rollDie(20);
    int total = dieRoll + monster->monster->baseAttack + abilityModifier +
                rangePenalty;

    combat.monsterAttackHit = (dieRoll == 20) ||
        (dieRoll != 1 && total >= getArmorClass(
            target->character,
            target->turn.fullDefense ? 4 : 0));
    combat.monsterPendingDamage = 0;

    if (combat.monsterAttackHit)
    {
        combat.monsterPendingDamage = rollDice(
            weapon->damageDice,
            weapon->damageSides);

        if (weapon->type == WEAPON_MELEE)
            combat.monsterPendingDamage += abilityModifier;

        combat.monsterPendingDamage = std::max(
            1, combat.monsterPendingDamage);
    }

    combat.attackingMonster = monster;
    combat.monsterAttackTarget = target;
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
