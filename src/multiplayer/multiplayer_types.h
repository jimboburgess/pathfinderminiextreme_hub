#ifndef PATHFINDERMINIEXTREME_025_MULTIPLAYER_TYPES_H
#define PATHFINDERMINIEXTREME_025_MULTIPLAYER_TYPES_H

#include <stdint.h>

using PlayerID = uint8_t;

constexpr uint8_t MAX_MULTIPLAYER_PLAYERS = 4;
constexpr PlayerID HOST_PLAYER_ID = 0;
constexpr PlayerID SINGLE_PLAYER_ID = HOST_PLAYER_ID;
constexpr PlayerID INVALID_PLAYER_ID = UINT8_MAX;

inline bool isValidPlayerID(PlayerID playerID)
{
    return playerID < MAX_MULTIPLAYER_PLAYERS;
}

#endif
