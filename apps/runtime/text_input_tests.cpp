#include "party_runtime.h"
#include "runtime.h"
#include "data/game.h"
#include "graphics/charcreationscreen.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
using namespace pfme;using namespace pfme::runtime;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{
 GameSession game;check(game.initialize(2)==SessionError::Ok,"session");PartyRuntime runtime(game,2);PlayerHandle explorer,creator;
 check(game.join("Explorer",explorer)==SessionError::Ok,"explorer");check(runtime.start(explorer,1),"explorer controller");
 auto wait=[&](unsigned ms){while(ms){auto step=std::min(ms,10u);runtime.tick(step);ms-=step;}};
 auto tap=[&](const PlayerHandle& p,uint64_t serial,const char* action){check(runtime.input(p,serial,action,true),"press");wait(30);check(runtime.input(p,serial,action,false),"release");wait(310);};
 for(int i=0;i<10&&!textInputActive();++i)tap(explorer,1,"a");check(textInputActive(),"explorer name entry");check(runtime.input(explorer,1,"key:X",true),"explorer name");tap(explorer,1,"encoder");check(gameState==GAME_TOWN,"production creation reaches Town");check(!textInputActive(),"name mode must end on leaving character creation");
 for(int i=0;i<4;++i)tap(explorer,1,"cw");tap(explorer,1,"a");tap(explorer,1,"a");tap(explorer,1,"b");tap(explorer,1,"b");tap(explorer,1,"ccw");tap(explorer,1,"ccw");tap(explorer,1,"a");check(gameState==GAME_DUNGEON,"explorer in authoritative Dungeon");
 check(game.join("Creator",creator)==SessionError::Ok,"creator");check(runtime.start(creator,2),"creator controller");
 for(int i=0;i<10&&!textInputActive();++i)tap(creator,2,"a");check(textInputActive(),"other controller entering a name");
 check(runtime.pixels(explorer,1)!=nullptr,"explorer authenticated frame");check(!textInputActive(),"Dungeon frame must NEVER inherit another player's name-entry flag");
 tap(explorer,1,"a");check(!textInputActive(),"Space/Button A opens gameplay UI instead of text input");
 check(runtime.pixels(creator,2)!=nullptr&&textInputActive(),"creator retains independent name UI");
 for(int i=0;i<40;++i)check(runtime.input(creator,2,"key:X",true),"typing past name limit must not disconnect");
 check(!runtime.input(creator,2,"key:!",true),"unsupported native text still rejected");
 check(runtime.pixels(explorer,2)==nullptr,"wrong controller serial denied");
 check(runtime.pixels(explorer,1)!=nullptr&&!textInputActive(),"owner isolation preserved after text saturation");
 std::cout<<"PASS: Dungeon Space/Button A, independent name-entry flags, bounded full-name typing and controller ownership\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
