#include "town.h"

#include "characters/characters.h"
#include "data/game.h"
#include "graphics/messagelog.h"
#include "input/buttons.h"

constexpr unsigned long REST_SLEEP_TIME_MS = 2800;
constexpr unsigned long REST_RESULT_TIME_MS = 1800;

static bool townHomeOpen = false;
static TownHomeOption townHomeSelection = TOWN_HOME_REST;
static bool resting = false;
static bool restResultShown = false;
static unsigned long restStageTime = 0;

void openTownHome()
{
    townHomeOpen = true;
    townHomeSelection = TOWN_HOME_REST;
    suppressEncoderSelectUntilRelease();
    needsRedraw = true;
}

void closeTownHome()
{
    townHomeOpen = false;
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

        int healing = player.level * getAbilityModifier(
            player, ABILITY_CONSTITUTION);
        healing = max(0, healing);

        int regained = healCharacter(player, healing);
        restoreClassAbilityUses(player);

        char message[48];
        snprintf(message, sizeof(message),
                 "Rest regained %d HP.", regained);
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
