#pragma once
#include "dungeon/dungeon.h"
namespace pfme { namespace runtime {
// Selected only by the serialized native runtime, never by a client field.
PlayerID selectedController();
bool nativeRoomsActive();
bool travelNativeRoom(Dungeon&,RoomEntry);
void bindNativeRoom(Dungeon&,PlayerID);
bool bindNativeWorldRoom(Dungeon&,uint8_t);
Entity* nativeActor(PlayerID);
void saveNativeRoomView(Dungeon&);
bool leaveNativeRoom(Dungeon&);
uint8_t nativeActorRoom(PlayerID);
void retireNativeObserver(PlayerID);
uint8_t nativeEntityRoom(const Entity*);
bool hasLivingNativePlayers();
void prepareNativeCombatant(Entity*);
void tickNativeCombatRooms();
bool broadcastNativeMessage(const char*);
bool clearNativeMapEffects();
bool isNativeWorldEvent();
void scanNativeRoomAwareness();
}}
