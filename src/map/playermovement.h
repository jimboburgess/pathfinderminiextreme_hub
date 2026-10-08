//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_DUNGEONPLAYER_H
#define PATHFINDERMINIEXTREME_025_DUNGEONPLAYER_H

#include "data/game.h"
#include "multiplayer/multiplayer_types.h"

struct Dungeon;

bool tryMovePlayer(Dungeon &dungeon);
bool tryMovePlayerAuthoritative(
    Dungeon& dungeon,
    PlayerID ownerPlayerID,
    Direction direction,
    bool allowRoomTransition,
    bool& roomChanged);

bool canPlayerMoveTo(int x, int y);

#endif //PATHFINDERMINIEXTREME_025_DUNGEONPLAYER_H
