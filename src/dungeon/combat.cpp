#include "combat.h"
#include "dungeon.h"
#include "forest.h"
#include "data/game.h"
#include "audio/audio.h"
#include "graphics/messagelog.h"

Combat combat;


void checkForCombat()
{
    // TODO
}

Character* getCurrentCombatant()
{
    if (combat.combatantCount == 0)
        return nullptr;

    return combat.turnOrder[combat.currentTurnIndex];
}

bool isPlayerTurn()
{
    return (getCurrentCombatant() == &player);
}

void startCombat()
{
    combat.active = true;
    combat.phase = COMBAT_TURN;

    combat.combatantCount = 1;
    combat.currentTurnIndex = 0;

    // For now, only the player is in the turn order.
    // We'll add monsters when we build initiative.
    combat.turnOrder[0] = &player;

    combat.combatRound = 1;

    combat.movementRemaining = player.speed;
    combat.standardActionUsed = false;

    setGameMessage("Combat Begins!");
}

void nextTurn()
{
    combat.currentTurnIndex++;

    if (combat.currentTurnIndex >= combat.combatantCount)
    {
        combat.currentTurnIndex = 0;
        combat.combatRound++;
    }

    combat.movementRemaining = getCurrentCombatant()->speed;
    combat.standardActionUsed = false;
}

void endPlayerTurn()
{
    playSound(SoundEffect::MENU_MOVE);
    Character* current = getCurrentCombatant();

    if (current == nullptr)
        return;

    if (current == &player)
    {
        // Wait for player input.
    }
    else
    {
        updateMonsterTurn();
    }
}

void updateMonsterTurn()
{
    for (uint8_t i = 0; i < forestEntityCount; i++)
    {
        Entity& entity = forestEntities[i];

        if (entity.type != ENTITY_ENEMY)
            continue;

        entity.x--;
    }

    playSound(SoundEffect::BUMP);   // or any very different sound

    if (isPlayerTurn())
    {
        // Wait for player input
    }
    else
    {
        // Monster AI
    }
        combat.movementRemaining = player.speed;
        combat.standardActionUsed = false;
}

void updateCombat()
{
    switch (combat.phase)
    {
        case COMBAT_NONE:
            break;

        case COMBAT_INITIATIVE:
            break;

        case COMBAT_TURN:

            if (!isPlayerTurn())
            {
                // Monster turns aren't implemented yet.
            }

            break;

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