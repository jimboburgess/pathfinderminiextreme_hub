#include "party_runtime.h"
#include "session_bank.h"
#include <vector>
#include <random>
#include <algorithm>
#include "runtime.h"
#include "character_saves.h"
#include "native_party.h"
#include "native_rooms.h"
#include "Preferences.h"
#include "characters/charactercreation.h"
#include "graphics/charcreationscreen.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "data/game.h"
#include "data/savegame.h"
#include "dungeon/combat.h"
#include "input/savemenu.h"
#include "town/town.h"
#include "forest/forest.h"
#include "map/activemap.h"
#include "data/entityspawn.h"
#include <queue>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <fstream>
using namespace pfme;using namespace pfme::runtime;
namespace fs=std::filesystem;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void env(const char* k,const std::string& v){
#ifdef _WIN32
 _putenv_s(k,v.c_str());
#else
 setenv(k,v.c_str(),1);
#endif
}
void secureDir(const fs::path& path){fs::create_directories(path);fs::permissions(path,fs::perms::owner_all,fs::perm_options::replace);}
void wait(PartyRuntime& runtime,unsigned ms){while(ms){unsigned step=ms<10?ms:10;runtime.tick(step);ms-=step;}}
void tap(PartyRuntime& runtime,const PlayerHandle& h,uint64_t serial,const char* action){check(runtime.input(h,serial,action,true),"down");wait(runtime,30);check(runtime.input(h,serial,action,false),"up");wait(runtime,310);}
void capture(const char* name){
 if(const char* folder=std::getenv("PFME_SAVE_TEST_FRAMES")){
  fs::create_directories(folder);refreshDisplay();const auto* pixels=controllerPixels(selectedController());
  std::ofstream out(fs::path(folder)/(std::string(name)+".rgb565"),std::ios::binary);
  for(unsigned i=0;i<57600;++i){out.put(char(pixels[i]));out.put(char(pixels[i]>>8));}
 }
}
void create(PartyRuntime& runtime,const PlayerHandle& h,uint64_t serial,const char* name){
 for(unsigned n=0;n<12&&!textInputActive();++n)tap(runtime,h,serial,"a");check(textInputActive(),"original creation name field");
 for(const char* p=name;*p;++p)check(runtime.input(h,serial,std::string("key:")+*p,true),"name");
 tap(runtime,h,serial,"encoder");check(gameState==GAME_TOWN,"original creation reaches Town");
}
void choose(PartyRuntime& rt,const PlayerHandle& h,uint64_t serial,const char* title){
 check(rt.input(h,serial,"a",false),"settle released menu input");wait(rt,350);if(!menuState.isOpen||!menuState.depth)throw std::runtime_error(std::string("menu closed before ")+title);auto* menu=menuState.menuStack[menuState.depth-1];int target=-1,index=0;
 for(unsigned i=0;i<menu->itemCount;++i)if(isMenuItemVisible(menu->items[i].action)){if(std::string(menu->items[i].title)==title)target=index;++index;}
 if(target<0)throw std::runtime_error(std::string("missing option ")+title+" in "+menu->title);for(unsigned n=0;menuState.cursorIndex!=target&&n<40;++n)tap(rt,h,serial,menuState.cursorIndex<target?"cw":"ccw");tap(rt,h,serial,"a");
}
bool pathStep(PartyRuntime& rt,const PlayerHandle& h,uint64_t serial,unsigned id){
 selectController(id);auto* actor=nativeActor(id);if(!actor)return false;
 const int start=actor->y*ROOM_WIDTH+actor->x;int parent[ROOM_WIDTH*ROOM_HEIGHT],dirs[ROOM_WIDTH*ROOM_HEIGHT];
 std::fill(std::begin(parent),std::end(parent),-1);std::queue<int> q;q.push(start);parent[start]=start;int found=-1;
 while(!q.empty()&&found<0){int cell=q.front();q.pop();int x=cell%ROOM_WIDTH,y=cell/ROOM_WIDTH;
  if(getActiveMapTile(x,y)==TILE_DOOR&&x==ROOM_WIDTH-1){found=cell;break;}
  for(unsigned d=0;d<8;d+=2){int nx=x+directionOffsets[d].dx,ny=y+directionOffsets[d].dy;
   if(nx<0||ny<0||nx>=ROOM_WIDTH||ny>=ROOM_HEIGHT)continue;int next=ny*ROOM_WIDTH+nx;if(parent[next]>=0)continue;
   auto tile=getActiveMapTile(nx,ny);if(tile!=TILE_FLOOR&&tile!=TILE_RUBBLE&&!(tile==TILE_DOOR&&nx==ROOM_WIDTH-1))continue;
   auto* occupant=getEntityAt(dungeon.entities,dungeon.entityCount,nx,ny);if(occupant&&occupant!=actor)continue;
   parent[next]=cell;dirs[next]=d;q.push(next);
  }
 }
 if(found<0||found==start)return false;while(parent[found]!=start)found=parent[found];
 for(unsigned n=0;moveDirection!=dirs[found]&&n<8;++n)tap(rt,h,serial,"cw");
 const auto room=nativeActorRoom(id);const int x=actor->x,y=actor->y;tap(rt,h,serial,"encoder");
 return nativeActorRoom(id)!=room||actor->x!=x||actor->y!=y;
}
int main(int argc,char** argv){try{
 const bool forest=argc>2&&std::string(argv[2])=="forest";
 const unsigned count=argc>1?std::stoul(argv[1]):20;check(count>=1&&count<=20,"account count");
 SessionBank bank(true);check(bank.initialize(2)==SessionError::Ok,"bank");PartyRuntime runtime(bank,2);
 std::vector<PlayerHandle> handles(count);std::vector<uint64_t> serials(count);std::vector<PartySnapshot> parties;
 for(unsigned id=0;id<count;++id){serials[id]=id+1;check(bank.join(("Account"+std::to_string(id)).c_str(),handles[id])==SessionError::Ok,"admission");check(runtime.start(handles[id],serials[id],"acct-load-"+std::to_string(id)),"account-owned controller");create(runtime,handles[id],serials[id],("Hero"+std::to_string(id)).c_str());}
 check(bank.playerCount()==count,"exact account count");
 if(count==20){PlayerHandle extra;check(bank.join("TwentyFirst",extra)==SessionError::Full,"21st character rejected without state changes");check(bank.playerCount()==20,"rejection preserves all characters");}
 for(unsigned start=0;start<count;start+=4){
  selectController(start);townSelection=TOWN_BULLETIN_BOARD;tap(runtime,handles[start],serials[start],"a");choose(runtime,handles[start],serials[start],"Create Party");
  PartySnapshot p;check(nativePartySnapshot(start,p),"production party");parties.push_back(p);tap(runtime,handles[start],serials[start],"b");tap(runtime,handles[start],serials[start],"b");
  for(unsigned id=start+1;id<count&&id<start+4;++id){selectController(id);townSelection=TOWN_BULLETIN_BOARD;tap(runtime,handles[id],serials[id],"a");choose(runtime,handles[id],serials[id],"Join Party");for(unsigned n=0;n<5;++n){char c[2]={p.joinCode[n],0};choose(runtime,handles[id],serials[id],c);}tap(runtime,handles[id],serials[id],"b");tap(runtime,handles[id],serials[id],"b");}
  if(start==0&&count>4){unsigned outsider=4;selectController(outsider);townSelection=TOWN_BULLETIN_BOARD;tap(runtime,handles[outsider],serials[outsider],"a");choose(runtime,handles[outsider],serials[outsider],"Join Party");for(unsigned n=0;n<5;++n){char c[2]={p.joinCode[n],0};choose(runtime,handles[outsider],serials[outsider],c);}PartySnapshot full;check(nativePartySnapshot(0,full)&&full.memberCount==4&&!nativePartySnapshot(outsider,full),"fifth party member rejected without mutation");tap(runtime,handles[outsider],serials[outsider],"b");}
 }
 for(unsigned start=0;start<count;start+=4){selectController(start);townSelection=forest?TOWN_FOREST:TOWN_DUNGEON;tap(runtime,handles[start],serials[start],"a");}
 std::vector<Entity*> actors(count);std::vector<unsigned> worlds(count);std::vector<Character> characters(count);
 for(unsigned id=0;id<count;++id){selectController(id);actors[id]=nativeActor(id);worlds[id]=nativeWorldIndex();check(actors[id]&&gameState==(forest?GAME_FOREST:GAME_DUNGEON),"all accounts play production dungeon");check(actors[id]->nativeControllerID==id,"global owner is unique");check(actors[id]->ownerPlayerID==id%4,"party PlayerID remains 0..3");characters[id]=actors[id]->character;PartySnapshot p;check(nativePartySnapshot(id,p),"membership");check(p.memberCount==std::min(4u,count-id/4*4),"party size stays four");for(unsigned peer=0;peer<count;++peer)if(peer/4!=id/4)check(nativeActor(peer)==nullptr,"foreign-party actor never exposed");check(runtime.pixels(handles[id],serials[id]),"authorized original framebuffer");}
 for(unsigned id=0;id<count;++id)for(unsigned peer=0;peer<count;++peer)check((worlds[id]==worlds[peer])==(id/4==peer/4),"one authoritative world per party");
 // Exercise original movement/door rules at high controller IDs, not only allocation.
 if(!forest)for(unsigned start=0;start<count;start+=4){
  const unsigned explorer=std::min(start+3,count-1);selectController(explorer);
  int xs[4]{},ys[4]{};for(unsigned id=start;id<count&&id<start+4;++id){xs[id-start]=nativeActor(id)->x;ys[id-start]=nativeActor(id)->y;}
  for(unsigned n=0;nativeActorRoom(explorer)==0&&n<60;++n)check(pathStep(runtime,handles[explorer],serials[explorer],explorer),"actual high-controller corridor movement");
  check(nativeActorRoom(explorer)==1,"actual high-controller room transition");
  check(dungeon.rooms[1].discovered,"own authoritative room discovered");
  for(unsigned id=start;id<count&&id<start+4;++id)if(id!=explorer){check(nativeActorRoom(id)==0&&nativeActor(id)->x==xs[id-start]&&nativeActor(id)->y==ys[id-start],"peers stay in original room");selectController(id);check(dungeon.currentRoom==0,"own camera remains independent");}
  for(unsigned other=start+4;other<count;other+=4){selectController(other);check(!dungeon.rooms[1].discovered,"other parties do not inherit exploration");}
 }
 if(forest)for(unsigned start=0;start<count;start+=4){selectController(start);unsigned players=0;
  for(unsigned n=0;n<forestEntityCount;++n)if(forestEntities[n].active&&forestEntities[n].type==ENTITY_PLAYER){++players;check(forestEntities[n].nativeControllerID>=start&&forestEntities[n].nativeControllerID<std::min(start+4,count),"no unowned or foreign forest actor");}
  check(players==std::min(4u,count-start),"exact original forest actors, no orphan duplicate");
 }
 // Room transport may relocate an actor within its authoritative entity store.
 for(unsigned id=0;id<count;++id){selectController(id);actors[id]=nativeActor(id);check(actors[id]&&actors[id]->nativeControllerID==id,"actor after room transition");}
 std::mt19937 seeded(7319);
 for(unsigned pass=0;pass<12;++pass){unsigned id=seeded()%count;selectController(id);const auto local=actors[id]->ownerPlayerID;const auto x=actors[id]->x,y=actors[id]->y;
  runtime.detach(handles[id],serials[id]);check(bank.disconnect(handles[id])==SessionError::Ok,"temporary disconnect");PartySnapshot p;check(nativePartySnapshot(id,p)&&p.memberCount==std::min(4u,count-id/4*4),"leader disconnect never closes party");
  if(count>1){auto other=(id+1)%count;check(runtime.input(handles[other],serials[other],"cw",true),"others remain responsive");}
  check(bank.reconnect(handles[id])==SessionError::Ok,"reserved session reconnect");const auto old=serials[id];serials[id]+=100;
  check(runtime.start(handles[id],serials[id],"acct-load-"+std::to_string(id)),"stable account reconnect");check(!runtime.input(handles[id],old,"a",true),"old lease cannot control reconnect");check(nativeActor(id)==actors[id]&&actors[id]->x==x&&actors[id]->y==y&&actors[id]->ownerPlayerID==local&&std::strcmp(actors[id]->character.name,characters[id].name)==0,"same actor/party/id/character restored");
 }
 std::cout<<"PASS accounts="<<count<<" parties="<<parties.size()<<" party_limit=4 seeded_reconnects=12 original_C++_characters_views_worlds_isolated\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" "<<diagnostic()<<'\n';return 1;}}
