#include "party_runtime.h"
#include "controller_slots.h"
#include "runtime.h"
namespace pfme { namespace runtime {
struct PartyRuntime::Impl {
    GameSession& session;ControllerSlots slots;uint32_t seed;bool initialized=false;
    Impl(GameSession& value,uint32_t initialSeed):session(value),slots(value),seed(initialSeed){}
    bool resolve(const PlayerHandle& handle,uint64_t serial,SessionPlayer& player,ControllerState*& state) {
        return session.player(handle,player)==SessionError::Ok &&
            slots.access(handle,serial,state)==ControllerError::Ok;
    }
};
PartyRuntime::PartyRuntime(GameSession& session,uint32_t seed):impl_(new Impl(session,seed)){}
PartyRuntime::~PartyRuntime()=default;
bool PartyRuntime::start(const PlayerHandle& handle,uint64_t serial) {
    ControllerState* state=nullptr;SessionPlayer player;
    if(impl_->session.player(handle,player)!=SessionError::Ok ||
       impl_->slots.attach(handle,serial,state)!=ControllerError::Ok)return false;
    if(!impl_->initialized) {initializeControllers(impl_->seed);impl_->initialized=true;}
    if(!bindController(player.id,*state))return false;
    setControllerConnected(player.id,true);return true;
}
bool PartyRuntime::input(const PlayerHandle& handle,uint64_t serial,const std::string& action,bool down) {
    SessionPlayer player;ControllerState* state=nullptr;
    return impl_->resolve(handle,serial,player,state) && controllerInput(player.id,action,down);
}
const uint16_t* PartyRuntime::pixels(const PlayerHandle& handle,uint64_t serial) {
    SessionPlayer player;ControllerState* state=nullptr;
    if(!impl_->resolve(handle,serial,player,state))return nullptr;
    return controllerPixels(player.id);
}
void PartyRuntime::detach(const PlayerHandle& handle,uint64_t serial) {
    SessionPlayer player;ControllerState* state=nullptr;
    if(!impl_->resolve(handle,serial,player,state))return;
    setControllerConnected(player.id,false);impl_->slots.detach(handle,serial);
}
bool PartyRuntime::leave(const PlayerHandle& handle,uint64_t serial) {
    SessionPlayer player;ControllerState* state=nullptr;
    if(!impl_->resolve(handle,serial,player,state) || !unbindController(player.id))return false;
    return impl_->slots.leave(handle,serial)==ControllerError::Ok;
}
void PartyRuntime::tick(uint32_t milliseconds) {if(impl_->initialized)advance(milliseconds);}
}}
