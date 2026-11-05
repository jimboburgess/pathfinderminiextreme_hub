#ifndef PATHFINDERMINIEXTREME_025_MULTIPLAYER_RUNTIME_H
#define PATHFINDERMINIEXTREME_025_MULTIPLAYER_RUNTIME_H

#include "characters/characters.h"
#include "multiplayer/network_protocol.h"

void initializeMultiplayerRuntime();
void updateMultiplayerRuntime();
NetworkPlayerProfile makeNetworkPlayerProfile(const Character& character);

#endif
