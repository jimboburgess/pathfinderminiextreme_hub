#pragma once
#include <cstddef>
#include "dungeon/dungeontopology.h"
#include <type_traits>
#include "input/buttonstate.h"
#include "characters/creation_state.h"
#include "multiplayer/multiplayer_types.h"
namespace pfme { namespace runtime {
class ContextField {
public:
    ContextField();
    virtual ~ContextField()=default;
    virtual void save(unsigned)=0;
    virtual void load(unsigned)=0;
    virtual void reset(unsigned)=0;
    ContextField* next=nullptr;
};
void switchPresentation(unsigned previous, unsigned next);
void resetPresentation(unsigned index);
void loadPresentation(unsigned index);
ButtonInputState& productionButtonState();
CharacterCreationModel& productionCreationState();
template<class T,unsigned Count=MAX_MULTIPLAYER_PLAYERS> class ValueBank {
    T& live;
    T slots[Count];
    T initial;
    template<class Value> static void copy(Value& to,const Value& from) {
        if constexpr(std::is_array<Value>::value) {
            for(std::size_t i=0;i<std::extent<Value>::value;++i)copy(to[i],from[i]);
        } else to=from;
    }
public:
    explicit ValueBank(T& value):live(value) {
        copy(initial,live);
        for(unsigned i=0;i<Count;++i) save(i);
    }
    void save(unsigned i) { copy(slots[i],live); }
    void load(unsigned i) { copy(live,slots[i]); }
    void reset(unsigned i) {copy(slots[i],initial);}
};
template<class T> class PresentationField final : public ContextField, public ValueBank<T> {
public:
    explicit PresentationField(T& value):ValueBank<T>(value){}
    void save(unsigned i) override {ValueBank<T>::save(i);}
    void load(unsigned i) override {ValueBank<T>::load(i);}
    void reset(unsigned i) override {ValueBank<T>::reset(i);}
};
class RoomContextField {
public:
    RoomContextField();
    virtual ~RoomContextField()=default;
    virtual void save(unsigned)=0;
    virtual void load(unsigned)=0;
    RoomContextField* next=nullptr;
};
void switchRoomFields(unsigned next);
template<class T> class RoomField final : public RoomContextField, public ValueBank<T,MAX_ROOMS> {
public:
    explicit RoomField(T& value):ValueBank<T,MAX_ROOMS>(value){}
    void save(unsigned i) override {ValueBank<T,MAX_ROOMS>::save(i);}
    void load(unsigned i) override {ValueBank<T,MAX_ROOMS>::load(i);}
};
}}
#define PFME_ROOM_FIELD(name) static pfme::runtime::RoomField<decltype(name)> roomContext_##name(name)
#define PFME_CONTEXT_FIELD(name) static pfme::runtime::PresentationField<decltype(name)> context_##name(name)
