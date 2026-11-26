#include "town.h"

#include "characters/items.h"
#include "characters/conditions.h"

#include "characters/characters.h"
#include "data/game.h"
#include "graphics/messagelog.h"
#include "input/buttons.h"

#include <stdio.h>

constexpr unsigned long REST_SLEEP_TIME_MS = 2800;
constexpr unsigned long REST_RESULT_TIME_MS = 1800;

static bool townHomeOpen = false;
static TownHomeOption townHomeSelection = TOWN_HOME_REST;
static bool resting = false;
static bool restResultShown = false;
static unsigned long restStageTime = 0;

namespace
{
const TownLocationDefinition townLocations[TOWN_OPTION_COUNT] =
{
    {TOWN_FOREST, "Enter the Forest",
     "Venture into the surrounding wilderness.", true,
     TOWN_SPRITE_FOREST, 1, 18, 43, 69, TOWN_ACTION_FOREST},
    {TOWN_DUNGEON, "Explore Dungeon",
     "Descend into the depths in search of adventure.", true,
     TOWN_SPRITE_DUNGEON, 166, 145, 69, 50, TOWN_ACTION_DUNGEON},
    {TOWN_SHOP, "Shop",
     "Purchase equipment and supplies.", true,
     TOWN_SPRITE_SHOP, 5, 91, 61, 49, TOWN_ACTION_SHOP},
    {TOWN_BULLETIN_BOARD, "Bulletin Board",
     "Find adventurers and form a party.", true,
     TOWN_SPRITE_BULLETIN_BOARD, 70, 105, 27, 35,
     TOWN_ACTION_BULLETIN_BOARD},
    {TOWN_STAY_HOME, "Stay Home",
     "It's dangerous out there.", true,
     TOWN_SPRITE_HOME, 91, 37, 52, 49, TOWN_ACTION_STAY_HOME},
    {TOWN_BARN, "Barn",
     "The barn is currently closed.", false,
     TOWN_SPRITE_BARN, 45, 39, 44, 44, TOWN_ACTION_UNAVAILABLE},
    {TOWN_CASTLE, "Castle",
     "The castle gates are closed.", false,
     TOWN_SPRITE_CASTLE, 164, 10, 73, 76, TOWN_ACTION_UNAVAILABLE},
    {TOWN_MYSTERIOUS_ALLEY, "Mysterious Alley",
     "Something stirs in the shadows.", false,
     TOWN_SPRITE_MYSTERIOUS_ALLEY, 101, 91, 33, 47,
     TOWN_ACTION_UNAVAILABLE}
};

const TownOption townSelectionOrder[TOWN_OPTION_COUNT] =
{
    TOWN_STAY_HOME,
    TOWN_FOREST,
    TOWN_DUNGEON,
    TOWN_SHOP,
    TOWN_BULLETIN_BOARD,
    TOWN_BARN,
    TOWN_CASTLE,
    TOWN_MYSTERIOUS_ALLEY
};
}

const TownLocationDefinition& getTownLocation(TownOption option)
{
    const uint8_t index = static_cast<uint8_t>(option);
    return townLocations[index < TOWN_OPTION_COUNT ? index : TOWN_STAY_HOME];
}

TownOption rotateTownLocation(TownOption option, bool forward)
{
    uint8_t current = 0;
    for (; current < TOWN_OPTION_COUNT; ++current)
        if (townSelectionOrder[current] == option) break;
    if (current == TOWN_OPTION_COUNT) current = 0;
    current = forward
        ? static_cast<uint8_t>((current + 1) % TOWN_OPTION_COUNT)
        : static_cast<uint8_t>(
            (current + TOWN_OPTION_COUNT - 1) % TOWN_OPTION_COUNT);
    return townSelectionOrder[current];
}

void showTownLocationUnavailable(const TownLocationDefinition& location)
{
    char message[48];
    snprintf(message, sizeof(message),
             "%s is currently unavailable.", location.displayName);
    setGameMessage(message);
    needsRedraw = true;
}

void openTownHome()
{
    townHomeOpen = true;
    townHomeSelection = TOWN_HOME_REST;
    suppressMenuInputUntilRelease();
    needsRedraw = true;
}

void closeTownHome()
{
    townHomeOpen = false;
    suppressMenuInputUntilRelease();
    needsRedraw = true;
}

bool isTownHomeOpen()
{
    return townHomeOpen;
}

TownHomeOption getTownHomeSelection()
{
    return townHomeSelection;
}

void rotateTownHomeSelection(bool forward)
{
    if (resting)
        return;

    if (forward)
    {
        townHomeSelection = static_cast<TownHomeOption>(
            (townHomeSelection + 1) % TOWN_HOME_OPTION_COUNT);
    }
    else
    {
        townHomeSelection = static_cast<TownHomeOption>(
            (townHomeSelection + TOWN_HOME_OPTION_COUNT - 1) %
            TOWN_HOME_OPTION_COUNT);
    }

    needsRedraw = true;
}

void beginTownRest()
{
    if (resting)
        return;

    resting = true;
    restResultShown = false;
    restStageTime = millis();
    setGameMessage("zzz. . . zzz. . . zzz. . .");
    needsRedraw = true;
}

void updateTownRest()
{
    if (!resting)
        return;

    if (!restResultShown)
    {
        if (millis() - restStageTime < REST_SLEEP_TIME_MS)
            return;

        // Cure both generic poison and its timed venom affliction before
        // calculating recovery, so poison penalties cannot reduce this rest.
        removeCondition(player, CONDITION_POISONED);
        removeCondition(player, CONDITION_SLOWING_VENOM);
        player.conditions.poison = PoisonAffliction{};
        const int healing = player.level <= 1 ? player.health.maxHP :
            max(0, int(player.level) + getAbilityModifier(player, ABILITY_CONSTITUTION));
        const int regained = healCharacter(player, healing);
        int mana = player.level <= 1 ? player.magic.maxMP : 0;
        if (player.level > 1 && player.characterClass == CLASS_CLERIC)
            mana = max(0, int(player.level) + getAbilityModifier(player, ABILITY_CHARISMA));
        else if (player.level > 1 && player.characterClass == CLASS_WIZARD)
            mana = max(0, int(player.level) + getAbilityModifier(player, ABILITY_INTELLIGENCE));
        const int regainedMP = restoreMana(player, mana);
        restoreClassAbilityUses(player);

        char message[48];
        const AbilityID studyAbility = player.magic.learning.ability;
        bool learningCompleted = false;
        if (advanceSpellLearning(player, learningCompleted))
        {
            const char* spellName = learningCompleted
                ? getAbilityName(studyAbility)
                : getAbilityName(player.magic.learning.ability);
            if (learningCompleted)
                snprintf(message, sizeof(message), "Learned %s!", spellName);
            else
                snprintf(message, sizeof(message), "Studying %s: %u left",
                         spellName,
                         static_cast<unsigned>(player.magic.learning.restsRemaining));
        }
        else
            snprintf(message, sizeof(message), "Rest: +%d HP, +%d MP.", regained, regainedMP);
        setGameMessage(message);

        restResultShown = true;
        restStageTime = millis();
        needsRedraw = true;
        return;
    }

    if (millis() - restStageTime >= REST_RESULT_TIME_MS &&
        isGameMessageComplete())
    {
        resting = false;
        needsRedraw = true;
    }
}

bool isTownRestActive()
{
    return resting;
}

#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
#include "context_fields.h"
PFME_CONTEXT_FIELD(townHomeOpen);
PFME_CONTEXT_FIELD(townHomeSelection);
PFME_CONTEXT_FIELD(resting);
PFME_CONTEXT_FIELD(restResultShown);
PFME_CONTEXT_FIELD(restStageTime);
#endif
