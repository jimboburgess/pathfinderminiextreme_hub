#include "controller_slots.h"
#include <new>
namespace pfme { namespace runtime {
ControllerError ControllerSlots::resolve(const PlayerHandle& handle,Slot*& slot,SessionPlayer& player) {
    slot=nullptr;
    if(session_.player(handle,player)!=SessionError::Ok)return ControllerError::InvalidHandle;
    slot=&slots_[player.id];
    if(slot->state) {
        SessionPlayer owner;
        // An externally retired/reused numeric ID never inherits an old actor.
        if(session_.player(slot->handle,owner)!=SessionError::Ok)return ControllerError::StaleSlot;
    }
    return ControllerError::Ok;
}
ControllerError ControllerSlots::attach(const PlayerHandle& handle,uint64_t serial,ControllerState*& out) {
    out=nullptr;Slot* slot=nullptr;SessionPlayer player;
    const auto error=resolve(handle,slot,player);if(error!=ControllerError::Ok)return error;
    if(!serial)return ControllerError::InvalidHandle;
    if(!player.connected)return ControllerError::Disconnected;
    if(slot->lease&&slot->lease!=serial)return ControllerError::AlreadyConnected;
    if(!slot->state) {
        auto candidate=std::unique_ptr<ControllerState>(new(std::nothrow) ControllerState);
        if(!candidate||!candidate->surface.getBuffer())return ControllerError::NoMemory;
        slot->state=std::move(candidate);slot->handle=handle;
    }
    slot->lease=serial;out=slot->state.get();return ControllerError::Ok;
}
ControllerError ControllerSlots::access(const PlayerHandle& handle,uint64_t serial,ControllerState*& out) {
    out=nullptr;Slot* slot=nullptr;SessionPlayer player;
    const auto error=resolve(handle,slot,player);if(error!=ControllerError::Ok)return error;
    if(!slot->state||!serial||slot->lease!=serial)return ControllerError::InvalidHandle;
    if(!player.connected)return ControllerError::Disconnected;
    out=slot->state.get();return ControllerError::Ok;
}
ControllerError ControllerSlots::detach(const PlayerHandle& handle,uint64_t serial) {
    Slot* slot=nullptr;SessionPlayer player;const auto error=resolve(handle,slot,player);
    if(error!=ControllerError::Ok)return error;
    if(!slot->state||!serial||slot->lease!=serial)return ControllerError::InvalidHandle;
    slot->lease=0;
    // No held long press, partial detent or confirm survives a lost connection.
    slot->state->input=ButtonInputState{};
    slot->state->input.reset(true,true,true,true,true,0);
    return ControllerError::Ok;
}
ControllerError ControllerSlots::leave(const PlayerHandle& handle,uint64_t serial) {
    ControllerState* state=nullptr;const auto error=access(handle,serial,state);
    if(error!=ControllerError::Ok)return error;
    SessionPlayer player;session_.player(handle,player);
    if(session_.leave(handle)!=SessionError::Ok)return ControllerError::InvalidHandle;
    slots_[player.id]=Slot{};return ControllerError::Ok;
}
size_t ControllerSlots::occupied() const {size_t count=0;for(const auto& slot:slots_)if(slot.state)++count;return count;}
ControllerError ControllerSlots::expire(const PlayerHandle& handle){
 Slot* slot=nullptr;SessionPlayer player;auto error=resolve(handle,slot,player);if(error!=ControllerError::Ok)return error;
 if(slot->lease)return ControllerError::AlreadyConnected;
 if(session_.leave(handle)!=SessionError::Ok)return ControllerError::InvalidHandle;slots_[player.id]=Slot{};return ControllerError::Ok;
}
}}
