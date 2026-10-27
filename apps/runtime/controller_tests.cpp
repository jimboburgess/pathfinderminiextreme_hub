#include "controller_slots.h"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace pfme;
using namespace pfme::runtime;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void party(unsigned count,bool duplicateClasses) {
    GameSession game;check(game.initialize(2)==SessionError::Ok,"world creation");
    ControllerSlots controllers(game);std::array<PlayerHandle,MAX_MULTIPLAYER_PLAYERS> handles;
    std::array<ControllerState*,MAX_MULTIPLAYER_PLAYERS> states{};
    for(unsigned i=0;i<count;++i) {
        check(game.join(("Player"+std::to_string(i)).c_str(),handles[i])==SessionError::Ok,"join capability");
        check(controllers.attach(handles[i],100+i,states[i])==ControllerError::Ok,"controller attach");
        auto& state=*states[i];state.creation.enter();
        for(unsigned c=0;c<(duplicateClasses?0:i);++c)state.creation.rotateClass(true);
        if(!state.creation.createPreview())check(state.creation.createPreview(),"fighter style selection");
        check(state.creation.accept(state.character),"production character acceptance");
        check(state.character.health.currentHP>0&&state.character.inventory.gold==STARTING_GOLD,"full production character");
        check(state.character.characterClass==static_cast<CharacterClass>(duplicateClasses?0:i),"independent class selection");
        check(state.input.buttonAPressed(false,1),"independent A edge");
        check(!state.input.buttonAPressed(false,2),"held A does not repeat");
        check(!state.input.encoderButtonLongPressed(false,100),"long press begins");
        state.surface.fillScreen(static_cast<uint16_t>(i+1));state.menu.cursorIndex=static_cast<uint8_t>(i);
    }
    // Interleave all partial encoders: a detent from one must not complete another.
    for(unsigned i=0;i<count;++i)states[i]->input.reset(true,true,true,true,true,0);
    for(int phase:{2,0,1,3})for(unsigned i=0;i<count;++i) {
        const auto step=states[i]->input.readEncoder((phase&2)!=0,(phase&1)!=0,false);
        check(step==(phase==3?QUADRATURE_CLOCKWISE_STEP:0),"interleaved original quadrature");
    }
    for(unsigned i=0;i<count;++i) {
        auto& s=*states[i];check(s.surface.getBuffer()[0]==i+1&&s.menu.cursorIndex==i,"UI/frame isolation");
        check(!s.input.encoderButtonLongPressed(false,849),"long press threshold");
        check(s.input.encoderButtonLongPressed(false,850),"750ms long press per controller");
        check(!s.input.encoderButtonLongPressed(false,851),"long press only once");
        s.character.xp=100+i;s.character.inventory.gold=200+i;s.character.magic.currentMP=static_cast<int>(i);
        check(controllers.detach(handles[i],9999)==ControllerError::InvalidHandle,"wrong lease detach rejected");
        check(controllers.detach(handles[i],100+i)==ControllerError::Ok,"detach only own input");
        check(game.disconnect(handles[i])==SessionError::Ok,"disconnect retained actor");
        ControllerState* out=reinterpret_cast<ControllerState*>(1);
        check(controllers.access(handles[i],100+i,out)==ControllerError::InvalidHandle&&out==nullptr,"detached socket denied");
        check(game.reconnect(handles[i])==SessionError::Ok,"verified reconnect");
        check(controllers.attach(handles[i],200+i,out)==ControllerError::Ok&&out==states[i],"reconnect existing allocation");
        check(out->character.xp==100+i&&out->character.inventory.gold==200+i&&out->character.magic.currentMP==static_cast<int>(i),"full state survives reconnect");
        check(controllers.attach(handles[i],300+i,out)==ControllerError::AlreadyConnected&&out==nullptr,"duplicate live connection denied");
    }
    check(controllers.occupied()==count&&game.playerCount()==count,"connections are not characters");
    if(count==MAX_MULTIPLAYER_PLAYERS) {
        const auto revision=game.latestSequence();PlayerHandle fifth;
        check(game.join("Fifth",fifth)==SessionError::Full,"fifth occupied slot rejected");
        check(game.latestSequence()==revision&&controllers.occupied()==MAX_MULTIPLAYER_PLAYERS,"party full unchanged");
    }
    GameSession foreign;check(foreign.initialize(2)==SessionError::Ok,"foreign world");PlayerHandle forged;
    check(foreign.join("Foreign",forged)==SessionError::Ok,"foreign capability");ControllerState* out=nullptr;
    check(controllers.access(forged,200,out)==ControllerError::InvalidHandle&&out==nullptr,"foreign handle cannot inspect");
    auto old=handles[0];check(controllers.leave(handles[0],200)==ControllerError::Ok,"explicit leave");
    PlayerHandle replacement;check(game.join("Replacement",replacement)==SessionError::Ok,"slot reused");
    check(controllers.attach(replacement,900,out)==ControllerError::Ok&&out->character.xp==0,"new generation has no old character");
    check(controllers.access(old,200,out)==ControllerError::InvalidHandle,"old capability retired");
    std::cout<<"party="<<count<<" duplicate_classes="<<duplicateClasses<<" input/creation/full-state/lease/frame checks passed\n";
}
int main(){try{for(unsigned n=1;n<=MAX_MULTIPLAYER_PLAYERS;++n){party(n,false);party(n,true);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
