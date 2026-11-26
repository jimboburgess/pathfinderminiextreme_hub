#ifndef PATHFINDERMINIEXTREME_025_TOWNSCENE_H
#define PATHFINDERMINIEXTREME_025_TOWNSCENE_H

#include <stdint.h>

#include "data/game.h"

constexpr int16_t TOWN_SCENE_HEIGHT = 200;
constexpr uint16_t TOWN_SELECTION_COLOR = 0xFFE0;

void drawTownScene();
void invalidateTownScene();
void clearTownSceneSelectionHighlight(TownOption option);

#endif // PATHFINDERMINIEXTREME_025_TOWNSCENE_H
