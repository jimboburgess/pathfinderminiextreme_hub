#include "combat.h"

#include "activemap.h"
#include "mapeffects.h"
#include "characters/conditions.h"
#include "graphics/display.h"

void abortCombat()
{
    // Initiative can still contain valid participants even if a defensive
    // caller has already changed maps. Reset those entities before clearing
    // every pointer in Combat.
    for (uint8_t i = 0; i < combat.combatantCount; i++)
    {
        Entity* combatant = combat.initiativeOrder[i];

        if (combatant == nullptr)
            continue;

        // Flat-Footed is applied by startCombat() as encounter bookkeeping.
        // Remove only it; poison, Sleep, Blindness, Prone, buffs, and all
        // other Character conditions remain intact.
        removeCondition(
            combatant->character,
            CONDITION_FLAT_FOOTED);
        combatant->turn = TurnState{};
    }

    uint8_t entityCount = 0;
    Entity* entities = getActiveMapEntities(entityCount);

    // Cover active-map entities that were not in the initiative list.
    for (uint8_t i = 0; entities != nullptr && i < entityCount; i++)
        entities[i].turn = TurnState{};

    // Persistent ground effects are encounter-duration runtime state. They
    // must not remain indefinitely after their combat is abandoned.
    clearMapEffects();

    // Value initialization clears every phase/timer, all initiative and
    // targeting pointers, pending damage, monster AI state, cursor selection,
    // and inspection state without invoking victory or XP/loot handling.
    combat = Combat{};

    backgroundNeedsRedraw = true;
    redrawType = REDRAW_FULL;
    needsRedraw = true;
}
