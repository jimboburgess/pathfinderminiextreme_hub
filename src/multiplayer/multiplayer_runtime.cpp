#include "multiplayer_runtime.h"

#include <Arduino.h>

#include "data/entities.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "graphics/messagelog.h"
#include "map/activemap.h"
#include "multiplayer_session.h"

namespace
{
MultiplayerAvailability currentAvailability()
{
    if (combat.active) return MultiplayerAvailability::IN_COMBAT;
    if (gameState == GAME_DUNGEON)
        return MultiplayerAvailability::IN_DUNGEON;
    if (multiplayerSession.isActive() || multiplayerSession.isJoining())
        return MultiplayerAvailability::IN_PARTY;
    return MultiplayerAvailability::AVAILABLE;
}

void refreshLocalProfile()
{
    multiplayerSession.setLocalProfile(
        player.name.c_str(),
        player.level,
        static_cast<uint8_t>(player.characterClass));
}
}

void initializeMultiplayerRuntime()
{
    refreshLocalProfile();
    multiplayerSession.begin();
}

void updateMultiplayerRuntime()
{
    refreshLocalProfile();
    multiplayerSession.update(millis(), currentAvailability());

    // Stage 1 still has one player entity, so the legacy lookup is safe here.
    // Stage 2 will replace it with an owner-aware local-player lookup once
    // remote player entities are instantiated on the active map.
    Entity* localPlayerEntity = getActiveMapPlayer();
    if (localPlayerEntity != nullptr)
        localPlayerEntity->ownerPlayerID =
            multiplayerSession.getLocalPlayerID();

    char notice[64] = {};
    if (multiplayerSession.consumeNotice(notice, sizeof(notice)))
    {
        setGameMessage(notice);
        needsRedraw = true;
    }
}
