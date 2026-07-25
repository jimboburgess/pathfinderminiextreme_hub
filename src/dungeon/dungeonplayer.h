//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_DUNGEONPLAYER_H
#define PATHFINDERMINIEXTREME_025_DUNGEONPLAYER_H

#include "dungeon.h"
#include "audio/audio.h"
#include "data/game.h"



void drawMoveCursor(const Dungeon &dungeon);

bool tryMovePlayer(Dungeon &dungeon);

bool tryMoveForestPlayer();

#endif //PATHFINDERMINIEXTREME_025_DUNGEONPLAYER_H
