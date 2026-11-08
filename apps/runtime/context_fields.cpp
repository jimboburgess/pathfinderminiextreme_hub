#include "context_fields.h"
#include <stdexcept>
#include "native_party.h"
namespace pfme { namespace runtime {
namespace {
ContextField*& fields() {static ContextField* first=nullptr;return first;}
RoomContextField*& roomFields() {static RoomContextField* first=nullptr;return first;}
unsigned boundRoom=NATIVE_ROOM_BANK_COUNT;
}
ContextField::ContextField() { next=fields();fields()=this; }
RoomContextField::RoomContextField() {next=roomFields();roomFields()=this;}
void switchRoomFields(unsigned next) {
    if(next>MAX_ROOMS)return;
    next+=nativeWorldIndex()*(MAX_ROOMS+1);
    if(next==boundRoom)return;
    if(boundRoom<NATIVE_ROOM_BANK_COUNT)for(auto* f=roomFields();f;f=f->next)f->save(boundRoom);
    for(auto* f=roomFields();f;f=f->next)f->load(next);
    boundRoom=next;
}
void resetNativeRoomFields(){
 const unsigned first=nativeWorldIndex()*(MAX_ROOMS+1);
 for(auto* f=roomFields();f;f=f->next)for(unsigned i=first;i<first+MAX_ROOMS+1;++i)f->reset(i);
 if(boundRoom>=first && boundRoom<first+MAX_ROOMS+1)boundRoom=NATIVE_ROOM_BANK_COUNT;
 switchRoomFields(0);
}
void loadPresentation(unsigned index) {
    if(index>=NATIVE_CONTROLLER_CAPACITY)throw std::out_of_range("Controller index");
    for(auto* field=fields();field;field=field->next)field->load(index);
}
void resetPresentation(unsigned index) {
    if(index>=NATIVE_CONTROLLER_CAPACITY)throw std::out_of_range("Controller index");
    for(auto* field=fields();field;field=field->next)field->reset(index);
}
void switchPresentation(unsigned previous,unsigned next) {
    if(previous>=NATIVE_CONTROLLER_CAPACITY || next>=NATIVE_CONTROLLER_CAPACITY)
        throw std::out_of_range("Controller index");
    if(previous==next)return;
    for(auto* field=fields();field;field=field->next)field->save(previous);
    for(auto* field=fields();field;field=field->next)field->load(next);
}
}}
