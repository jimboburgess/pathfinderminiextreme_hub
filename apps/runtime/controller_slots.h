#pragma once
#include "session/game_session.h"
#include "characters/creation_state.h"
#include "input/buttonstate.h"
#include "input/menu.h"
#include "Adafruit_ST7789.h"
#include <memory>

namespace pfme { namespace runtime {
// Native-only owned state. This is not another world or another gameplay engine.
// Production input and character creation are shared with the handheld adapter.
struct ControllerState {
    Character character{};
    CharacterCreationModel creation;
    ButtonInputState input;
    MenuState menu{};
    Direction facing=DIR_NORTH;
    uint8_t viewRoom=0;
    bool encoderInverted=false;
    Adafruit_ST7789 surface{0,0,0};
    ControllerState()=default;
    ControllerState(const ControllerState&)=delete;
    ControllerState& operator=(const ControllerState&)=delete;
};
enum class ControllerError { Ok, InvalidHandle, Disconnected, AlreadyConnected, StaleSlot, NoMemory };

// Caller owns the GameSession and verified capabilities. Socket serials lease
// existing character slots; they do not authorize or allocate identities.
class ControllerSlots {
public:
    explicit ControllerSlots(GameSession& session):session_(session){}
    ControllerError attach(const PlayerHandle&,uint64_t serial,ControllerState*&);
    ControllerError access(const PlayerHandle&,uint64_t serial,ControllerState*&);
    ControllerError detach(const PlayerHandle&,uint64_t serial);
    ControllerError leave(const PlayerHandle&,uint64_t serial);
    size_t occupied() const;
private:
    struct Slot { PlayerHandle handle;uint64_t lease=0;std::unique_ptr<ControllerState> state; };
    GameSession& session_;
    Slot slots_[MAX_MULTIPLAYER_PLAYERS];
    ControllerError resolve(const PlayerHandle&,Slot*&,SessionPlayer&);
};
}}
