#include "party_runtime.h"
#include "controller_slots.h"
#include "runtime.h"
#include "native_party.h"
#include "character_saves.h"
#include "Preferences.h"
#include <cstdio>
#include "data/game.h"
#include "session_bank.h"
#include <array>
namespace pfme { namespace runtime {
struct PartyRuntime::Impl {
    std::array<GameSession*,5> sessions{};std::array<std::unique_ptr<ControllerSlots>,5> slots{};
    unsigned count=0;uint32_t seed;bool initialized=false;
    Impl(GameSession& value,uint32_t initialSeed):count(1),seed(initialSeed){sessions[0]=&value;slots[0]=std::make_unique<ControllerSlots>(value);}
    Impl(SessionBank& bank,uint32_t initialSeed):count(bank.banks()),seed(initialSeed){for(unsigned i=0;i<count;++i){sessions[i]=&bank.bank(i);slots[i]=std::make_unique<ControllerSlots>(*sessions[i]);}}
    int locate(const PlayerHandle& handle,SessionPlayer& player){for(unsigned i=0;i<count;++i)if(sessions[i]->player(handle,player)==SessionError::Ok){player.id+=i*MAX_MULTIPLAYER_PLAYERS;return int(i);}return -1;}
    bool resolve(const PlayerHandle& handle,uint64_t serial,SessionPlayer& player,ControllerState*& state) {
        const int i=locate(handle,player);return i>=0&&slots[i]->access(handle,serial,state)==ControllerError::Ok;
    }
};
PartyRuntime::PartyRuntime(GameSession& session,uint32_t seed):impl_(new Impl(session,seed)){}
PartyRuntime::PartyRuntime(SessionBank& bank,uint32_t seed):impl_(new Impl(bank,seed)){}
PartyRuntime::~PartyRuntime(){
    if(impl_->initialized)for(uint8_t id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)
        if(!flushAccountCharacter(id))std::fputs("{\"event\":\"character_save_failed\"}\n",stderr);
}
bool PartyRuntime::start(const PlayerHandle& handle,uint64_t serial,const std::string& verifiedPrincipal) {
    ControllerState* state=nullptr;SessionPlayer player;
    const int bank=impl_->locate(handle,player);
    if(bank<0 || impl_->slots[bank]->attach(handle,serial,state)!=ControllerError::Ok)return false;
    if(!impl_->initialized) {initializeControllers(impl_->seed,true);impl_->initialized=true;}
    if((!verifiedPrincipal.empty()&&!CharacterFiles::bind(player.id,verifiedPrincipal)) ||
       (verifiedPrincipal.empty()&&CharacterFiles::required()))return false;
    if(!bindController(player.id,*state))return false;
    nativePartyCharacterName(player.id,::player.name[0]?::player.name:player.name);setControllerConnected(player.id,true);return true;
}
bool PartyRuntime::input(const PlayerHandle& handle,uint64_t serial,const std::string& action,bool down) {
    SessionPlayer player;ControllerState* state=nullptr;
    return impl_->resolve(handle,serial,player,state) && controllerInput(player.id,action,down);
}
const uint16_t* PartyRuntime::pixels(const PlayerHandle& handle,uint64_t serial) {
    SessionPlayer player;ControllerState* state=nullptr;
    if(!impl_->resolve(handle,serial,player,state) || !selectController(player.id))return nullptr;
    return controllerPixels(player.id);
}

bool PartyRuntime::audio(const PlayerHandle& handle,uint64_t serial,uint64_t since,bool fresh,AudioBatch& out) {
    SessionPlayer player;ControllerState* state=nullptr;
    if(!impl_->resolve(handle,serial,player,state))return false;
    out=readAudioStream(player.id,since,fresh);return true;
}
void PartyRuntime::detach(const PlayerHandle& handle,uint64_t serial) {
    SessionPlayer player;ControllerState* state=nullptr;
    if(!impl_->resolve(handle,serial,player,state))return;
    if(!flushAccountCharacter(player.id))std::fputs("{\"event\":\"character_save_failed\"}\n",stderr);
    setControllerConnected(player.id,false);impl_->slots[player.id/MAX_MULTIPLAYER_PLAYERS]->detach(handle,serial);
}
bool PartyRuntime::leave(const PlayerHandle& handle,uint64_t serial) {
    SessionPlayer player;ControllerState* state=nullptr;
    if(!impl_->resolve(handle,serial,player,state) || !unbindController(player.id))return false;
    return impl_->slots[player.id/MAX_MULTIPLAYER_PLAYERS]->leave(handle,serial)==ControllerError::Ok;
}
bool PartyRuntime::status(const PlayerHandle& handle,uint64_t serial,PartySnapshot& snapshot){
 SessionPlayer player;ControllerState* state=nullptr;
 if(!impl_->resolve(handle,serial,player,state)||!selectController(player.id))return false;
 refreshNativePartyMenu();return nativePartySnapshot(player.id,snapshot);
}
void PartyRuntime::tick(uint32_t milliseconds) {if(impl_->initialized){advance(milliseconds);autosaveAccountCharacters(milliseconds);}}
bool PartyRuntime::expire(const PlayerHandle& handle){
 SessionPlayer player;const int bank=impl_->locate(handle,player);
 if(bank<0||player.connected||!unbindController(player.id))return false;
 return impl_->slots[bank]->expire(handle)==ControllerError::Ok;
}
}}
