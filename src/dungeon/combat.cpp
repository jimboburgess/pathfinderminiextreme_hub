#include "combat.h"
#include "dungeon.h"
#include "forest.h"
#include "data/game.h"
#include "audio/audio.h"

Combat combat;


void checkForCombat()
{
    // TODO
}

void startCombat()
{
    combat.active = true;
    combat.phase = COMBAT_TURN;

    combat.turn = TURN_PLAYER;
    combat.movementRemaining = player.speed;
    combat.standardActionUsed = false;
}

void endPlayerTurn()
{
    playSound(SoundEffect::MENU_MOVE);
    combat.turn = TURN_MONSTERS;
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

        combat.turn = TURN_PLAYER;
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

            if (combat.turn == TURN_MONSTERS)
            {
                updateMonsterTurn();
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