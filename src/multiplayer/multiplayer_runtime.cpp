#include "multiplayer_runtime.h"

#include <Arduino.h>

#include <cstring>

#include "data/entities.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "graphics/messagelog.h"
#include "map/activemap.h"
#include "multiplayer_session.h"
#include "multiplayer_activity.h"

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
    const NetworkPlayerProfile profile = makeNetworkPlayerProfile(player);
    multiplayerSession.setLocalProfile(profile.displayName, profile.level,
                                       profile.characterClass);
}
}

NetworkPlayerProfile makeNetworkPlayerProfile(const Character& character)
{
    NetworkPlayerProfile profile = {};
    std::strncpy(profile.displayName, getCharacterName(character),
                 NETWORK_PLAYER_NAME_SIZE - 1);
    profile.level = character.level;
    profile.characterClass = static_cast<uint8_t>(character.characterClass);
    return profile;
}

void initializeMultiplayerRuntime()
{
    refreshLocalProfile();
    multiplayerSession.begin();
    initializeMultiplayerActivity();
}

void updateMultiplayerRuntime()
{
    refreshLocalProfile();
    multiplayerSession.update(millis(), currentAvailability());
    updateMultiplayerActivity(millis());

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
