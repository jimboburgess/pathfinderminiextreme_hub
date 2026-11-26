#ifndef PATHFINDERMINIEXTREME_025_TOWN_H
#define PATHFINDERMINIEXTREME_025_TOWN_H

#include <stdint.h>

#include "data/game.h"

enum TownLocationSprite : uint8_t
{
    TOWN_SPRITE_FOREST,
    TOWN_SPRITE_DUNGEON,
    TOWN_SPRITE_SHOP,
    TOWN_SPRITE_BULLETIN_BOARD,
    TOWN_SPRITE_HOME,
    TOWN_SPRITE_BARN,
    TOWN_SPRITE_CASTLE,
    TOWN_SPRITE_MYSTERIOUS_ALLEY
};

enum TownLocationAction : uint8_t
{
    TOWN_ACTION_FOREST,
    TOWN_ACTION_DUNGEON,
    TOWN_ACTION_SHOP,
    TOWN_ACTION_BULLETIN_BOARD,
    TOWN_ACTION_STAY_HOME,
    TOWN_ACTION_UNAVAILABLE
};

struct TownLocationDefinition
{
    TownOption id;
    const char* displayName;
    const char* description;
    bool available;
    TownLocationSprite sprite;
    int16_t x;
    int16_t y;
    uint8_t width;
    uint8_t height;
    TownLocationAction action;
};

const TownLocationDefinition& getTownLocation(TownOption option);
TownOption rotateTownLocation(TownOption option, bool forward);
void showTownLocationUnavailable(const TownLocationDefinition& location);

enum TownHomeOption : uint8_t
{
    TOWN_HOME_REST,
    TOWN_HOME_SAVE_GAME,
#ifdef PFME_NATIVE_CONTROLLER_CONTEXT
    TOWN_HOME_LOAD_GAME,
#endif
    TOWN_HOME_CHARACTER_SHEET,
    TOWN_HOME_EQUIPMENT,
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
