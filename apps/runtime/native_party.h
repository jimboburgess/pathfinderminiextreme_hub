#pragma once
#include "session/party_registry.h"
#include "input/menu.h"
struct Entity;
namespace pfme { namespace runtime {
constexpr unsigned NATIVE_WORLD_CAPACITY=MAX_MULTIPLAYER_PLAYERS;
constexpr uint8_t NATIVE_FOREST_ROOM=MAX_ROOMS;
bool socialPartiesEnabled();
unsigned nativeWorldIndex();
unsigned nativeControllerWorld(PlayerID);
void selectNativeWorld(unsigned);
void resetNativeWorld();
void switchNativeWorldRandom(unsigned,unsigned);
void resetNativeResidentRooms();
void resetNativeForest();
void assignNativeWorld(PlayerID,unsigned);
void initializeNativeParties(uint32_t);
void openNativePartyMenu();
bool activateNativePartyMenu();
bool cancelNativePartyMenu();
void refreshNativePartyMenu();
bool startNativePartyAdventure(bool forest);
void nativePartyReturnedToTown();
bool canResetNativePartyAdventure();
bool resetNativePartyAdventure();
void nativePartyConnection(PlayerID,bool);
bool retireNativePartyMember(PlayerID);
void resetNativePartyCharacter(PlayerID);
void nativePartyCharacterName(PlayerID,const char*);
void nativePartyMessage(const char*);
void selectNativeForestWorld(unsigned previous,unsigned next);
bool enterNativeForestController(bool fresh);
void adoptNativeForestActor(PlayerID,Entity*);
void invalidateNativeRoomView();
// Local test diagnostics; never accepted from clients or published via protocol.
bool nativePartySnapshot(PlayerID,PartySnapshot&);
}}
