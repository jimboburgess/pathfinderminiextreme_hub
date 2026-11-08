#include "character_saves.h"
#include "runtime.h"
#include "native_party.h"
#include "native_rooms.h"
#include "Preferences.h"
#include "data/game.h"
#include "data/savegame.h"
#include "dungeon/combat.h"
#include "town/town.h"
#include "graphics/charcreationscreen.h"
#include "input/menu.h"
#include <cstdio>
namespace pfme { namespace runtime {
namespace {
uint32_t sinceSave=0;
const char* canSwitch(bool saving){
    const auto id=selectedController();
    if(!isValidPlayerID(id)||CharacterFiles::owner(id).empty())return "Account storage unavailable.";
    if(combat.active||nativeActor(id)||isTownRestActive()||pendingNativeTrade(id))return "Finish combat/trade. Return to Town.";
    if(gameState!=GAME_TOWN && (saving||gameState!=GAME_START))return "Return to Stay Home first.";
    return nullptr;
}
}
bool accountCharactersEnabled(){return nativeRoomsActive()&&(!CharacterFiles::owner(selectedController()).empty()||CharacterFiles::required());}
void resetAccountCharacters(){CharacterFiles::reset();sinceSave=0;}
bool flushAccountCharacter(uint8_t id){
    const int slot=CharacterFiles::active(id);
    if(slot<0)return true; // Newly created characters acquire a slot on first Save.
    const auto previous=selectedController();
    if(!selectController(id))return false;
    const auto* actor=nativeActor(id);
    const bool good=saveGameToSlot(actor?actor->character:player,static_cast<uint8_t>(slot));
    if(previous!=id&&isValidPlayerID(previous))selectController(previous);
    return good;
}
const char* saveAccountCharacter(uint8_t slot){
    if(const auto* error=canSwitch(true))return error;
    const auto id=selectedController();
    if(slot>=SAVE_SLOT_COUNT)return "Invalid character slot.";
    const int active=CharacterFiles::active(id);
    if(active>=0&&active!=slot)return "Update this character in its own slot.";
    if(!saveGameToSlot(player,slot))return "Unable to save character.";
    CharacterFiles::activate(id,slot);nativePartyCharacterName(id,player.name);return nullptr;
}
const char* loadAccountCharacter(uint8_t slot,Character& loaded){
    if(const auto* error=canSwitch(false))return error;
    if(slot>=SAVE_SLOT_COUNT)return "Invalid character slot.";
    const auto id=selectedController();
    if(gameState==GAME_TOWN&&CharacterFiles::active(id)<0)return "Save this new character first.";
    // Flush before reading even the same slot: Load cannot rewind traded items.
    if(!flushAccountCharacter(id))return "Current character could not be saved.";
    if(!loadGameFromSlot(loaded,slot))return "Empty or corrupt save slot.";
    CharacterFiles::activate(id,slot);
    nativePartyCharacterName(id,loaded.name);
    return nullptr;
}
const char* newAccountCharacter(){
    if(const auto* error=canSwitch(true))return error;
    const auto id=selectedController();
    if(CharacterFiles::active(id)<0)return "Save this new character first.";
    if(!flushAccountCharacter(id))return "Current character could not be saved.";
    CharacterFiles::deactivate(id);closeMenu();closeTownHome();
    gameState=GAME_CHARACTER_CREATION;enterCharacterCreation();return nullptr;
}
void autosaveAccountCharacters(uint32_t elapsed){
    sinceSave+=elapsed;if(sinceSave<30000)return;sinceSave=0;
    for(uint8_t id=0;id<MAX_MULTIPLAYER_PLAYERS;++id)
        if(!flushAccountCharacter(id))std::fputs("{\"event\":\"character_save_failed\"}\n",stderr);
}
}}
