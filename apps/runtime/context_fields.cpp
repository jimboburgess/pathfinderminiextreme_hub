#include "context_fields.h"
#include <stdexcept>
namespace pfme { namespace runtime {
namespace {
ContextField*& fields() {static ContextField* first=nullptr;return first;}
RoomContextField*& roomFields() {static RoomContextField* first=nullptr;return first;}
unsigned boundRoom=MAX_ROOMS;
}
ContextField::ContextField() { next=fields();fields()=this; }
RoomContextField::RoomContextField() {next=roomFields();roomFields()=this;}
void switchRoomFields(unsigned next) {
    if(next>=MAX_ROOMS || next==boundRoom)return;
    if(boundRoom<MAX_ROOMS)for(auto* f=roomFields();f;f=f->next)f->save(boundRoom);
    for(auto* f=roomFields();f;f=f->next)f->load(next);
    boundRoom=next;
}
void loadPresentation(unsigned index) {
    if(index>=MAX_MULTIPLAYER_PLAYERS)throw std::out_of_range("Controller index");
    for(auto* field=fields();field;field=field->next)field->load(index);
}
void resetPresentation(unsigned index) {
    if(index>=MAX_MULTIPLAYER_PLAYERS)throw std::out_of_range("Controller index");
    for(auto* field=fields();field;field=field->next)field->reset(index);
}
void switchPresentation(unsigned previous,unsigned next) {
    if(previous>=MAX_MULTIPLAYER_PLAYERS || next>=MAX_MULTIPLAYER_PLAYERS)
        throw std::out_of_range("Controller index");
    if(previous==next)return;
    for(auto* field=fields();field;field=field->next)field->save(previous);
    for(auto* field=fields();field;field=field->next)field->load(next);
}
}}
