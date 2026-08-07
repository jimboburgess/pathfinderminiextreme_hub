#include "combat.h"
#include "dungeon.h"
#include "forest.h"
#include "data/game.h"
#include "audio/audio.h"
#include "data/dice.h"
#include "data/entityspawn.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "input/menu.h"

Combat combat;

static Entity* getPlayerCombatant()
{
    return getPlayerEntity(forestEntities, forestEntityCount);
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

static int gridDistance(const Entity* a, const Entity* b)
{
    return std::max(abs(a->x - b->x), abs(a->y - b->y));
}

static bool isValidRangedTarget(const Entity* player, const Entity* target)
{
    return target != nullptr &&
           target != player &&
           target->active &&
           target->character.state == STATE_ALIVE &&
           hasLineOfSight(player->x, player->y, target->x, target->y);
}

static Entity* getMapEntities(uint8_t& entityCount)
{
    switch (gameState)
    {
        case GAME_FOREST:
            entityCount = forestEntityCount;
            return forestEntities;

        case GAME_DUNGEON:
            entityCount = dungeon.entityCount;
            return dungeon.entities;

        default:
            entityCount = 0;
            return nullptr;
    }
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
                markTileDirty(target->x, target->y);
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


bool blocksSight(TileType tile)
{
    switch (tile)
    {
        case TILE_TREE:
            return true;

        default:
            return false;
    }
}

bool hasLineOfSight(int x1, int y1, int x2, int y2)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;

    int err = dx - dy;

    while (true)
    {
        // Skip the starting tile and ending tile.
        if (!(x1 == x2 && y1 == y2))
        {
            if (!(x1 == getForestPlayerX() &&
                  y1 == getForestPlayerY()))
            {
                if (blocksSight(getForestTile(x1, y1)))
                {
                    return false;
                }
            }
        }

        if (x1 == x2 && y1 == y2)
            break;

        int e2 = err * 2;

        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }

    return true;
}

void findCombatants()
{
    combat.combatantCount = 0;

    //--------------------------------------------------
    // Add the player.
    //--------------------------------------------------

    Entity* playerEntity = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    if (playerEntity != nullptr)
    {
        combat.initiativeOrder[
            combat.combatantCount++] = playerEntity;
    }

    //--------------------------------------------------
    // Add every living monster on the map.
    //--------------------------------------------------

    for (uint8_t i = 0; i < forestEntityCount; i++)
    {
        Entity& entity = forestEntities[i];

        if (!entity.active)
            continue;

        if (entity.type != ENTITY_MONSTER)
            continue;

        if (entity.character.state != STATE_ALIVE)
            continue;

        combat.initiativeOrder[
            combat.combatantCount++] = &entity;
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

    Entity* playerEntity = getPlayerEntity(
        forestEntities,
        forestEntityCount);

    if (playerEntity == nullptr)
        return;

    for (uint8_t i = 0; i < forestEntityCount; i++)
    {
        Entity& monster = forestEntities[i];

        if (!monster.active)
            continue;

        if (monster.type != ENTITY_MONSTER)
            continue;

        if (monster.character.state != STATE_ALIVE)
            continue;

        int dx = abs(monster.x - playerEntity->x);
        int dy = abs(monster.y - playerEntity->y);

        Serial.print("Monster at ");
        Serial.print(monster.x);
        Serial.print(", ");
        Serial.print(monster.y);
        Serial.print("  Distance = ");
        Serial.println(std::max(dx, dy));

        int distance = std::max(dx, dy);

        if (distance > COMBAT_DETECTION_RANGE)
            continue;

        if (!hasLineOfSight(
                playerEntity->x,
                playerEntity->y,
                monster.x,
                monster.y))
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
                isAdjacent(monster, chooseTarget(monster)))
            {
                monster->turn.monsterState = MONSTER_ACTION;
            }

            break;

        case MONSTER_ACTION:

            runMonsterScript(monster);

            monster->turn.monsterState = isMonsterAttackResolving()
                ? MONSTER_ATTACK
                : MONSTER_END;
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

                       markTileDirty(target->x, target->y);
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
    combat.active = false;
    combat.phase = COMBAT_NONE;
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

    if (player == nullptr || !isPlayerTargetingAttack())
        return nullptr;

    if (combat.attackType == COMBAT_ATTACK_MELEE)
    {
        int targetX = player->x + directionOffsets[moveDirection].dx;
        int targetY = player->y + directionOffsets[moveDirection].dy;

        if (targetX < 0 || targetX >= FOREST_WIDTH ||
            targetY < 0 || targetY >= FOREST_HEIGHT)
        {
            return nullptr;
        }

        Entity* target = getEntityAt(
            forestEntities,
            forestEntityCount,
            targetX,
            targetY);

        return target != nullptr &&
               target->character.state == STATE_ALIVE
                   ? target
                   : nullptr;
    }

    if (combat.selectedTargetIndex < 0 ||
        combat.selectedTargetIndex >= forestEntityCount)
    {
        return nullptr;
    }

    Entity* target = &forestEntities[combat.selectedTargetIndex];

    return isValidRangedTarget(player, target) ? target : nullptr;
}

static void markAttackCursorDirty()
{
    Entity* target = getSelectedAttackTarget();

    if (target != nullptr)
    {
        markTileDirty(target->x, target->y);
        return;
    }

    if (combat.attackType == COMBAT_ATTACK_MELEE)
    {
        markPlayerFacingCursorDirty();
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

        if (player == nullptr)
            return;

        int start = combat.selectedTargetIndex;

        for (uint8_t checked = 0; checked < forestEntityCount; checked++)
        {
            start += forward ? 1 : -1;

            if (start < 0)
                start = forestEntityCount - 1;
            else if (start >= forestEntityCount)
                start = 0;

            if (isValidRangedTarget(player, &forestEntities[start]))
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

    int rangePenalty = 0;

    if (combat.attackType == COMBAT_ATTACK_RANGED &&
        weapon->rangeIncrement > 0)
    {
        int targetDistanceFeet = gridDistance(player, target) * 5;
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

    markTileDirty(target->x, target->y);
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
    Entity* entities = getMapEntities(entityCount);

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
    Entity* entities = getMapEntities(entityCount);

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

void beginMonsterAttack(Entity* monster, Entity* target)
{
    if (monster == nullptr || target == nullptr ||
        monster->monster == nullptr ||
        isMonsterAttackResolving())
    {
        return;
    }

    const Weapon* weapon = getEquippedMeleeWeapon(monster->character);

    if (weapon == nullptr)
        return;

    int abilityModifier = getAbilityModifier(
        monster->character,
        weapon->type == WEAPON_RANGED
            ? ABILITY_DEXTERITY
            : ABILITY_STRENGTH);
    int dieRoll = rollDie(20);
    int total = dieRoll + monster->monster->baseAttack + abilityModifier;

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
             getEquippedItemName(monster->character, SLOT_MELEE_WEAPON));
    setGameMessage(message);
    requestCombatTileRedraw();
}

bool isMonsterAttackResolving()
{
    return combat.monsterAttackPhase != MONSTER_ATTACK_NONE;
}
