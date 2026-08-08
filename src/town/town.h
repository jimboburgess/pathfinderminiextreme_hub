#ifndef PATHFINDERMINIEXTREME_025_TOWN_H
#define PATHFINDERMINIEXTREME_025_TOWN_H

#include <stdint.h>

enum TownHomeOption
{
    TOWN_HOME_REST,
    TOWN_HOME_SAVE_GAME,
    TOWN_HOME_BACK,
    TOWN_HOME_OPTION_COUNT
};

void openTownHome();
void closeTownHome();
bool isTownHomeOpen();

TownHomeOption getTownHomeSelection();
void rotateTownHomeSelection(bool forward);

void beginTownRest();
void updateTownRest();
bool isTownRestActive();

#endif // PATHFINDERMINIEXTREME_025_TOWN_H
