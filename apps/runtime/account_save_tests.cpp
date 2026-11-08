#include "party_runtime.h"
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
int main(int argc,char** argv){fs::path root;try{
 const bool verify=argc>2&&std::string(argv[1])=="verify";
 root=argc>2?fs::path(argv[2]):fs::temp_directory_path()/("pfme-account-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 secureDir(root);env("PFME_SAVE_DIR",root.string());env("PFME_REQUIRE_ACCOUNT_SAVES","1");
 GameSession game;check(game.initialize(2)==SessionError::Ok,"session");PartyRuntime runtime(game,2);PlayerHandle owner,peer;
 check(game.join("Owner",owner)==SessionError::Ok&&runtime.start(owner,1,"acct-stable-owner"),"verified owner controller");
 const auto id=selectedController();const auto ownerDirectory=CharacterFiles::owner(id);
 if(verify){
   Character loaded{};check(loadGameFromSlot(loaded,0)&&std::string(loaded.name)=="Fighter","disk save survives independent process");
   check(loadGameFromSlot(loaded,1)&&std::string(loaded.name)=="Wizard","second process slot2");
   check(loadGameFromSlot(loaded,2)&&std::string(loaded.name)=="Rogue","second process slot3");
   std::cout<<"PASS separate-process account persistence\n";return 0;
 }
 check(!CharacterFiles::bind((id+1)%4,"acct-stable-owner"),"one account cannot occupy two controllers");
 create(runtime,owner,1,"Fighter");player.level=3;player.xp=321;player.inventory.gold=501;
 player.conditions.count=1;player.conditions.conditions[0].type=CONDITION_CURSED;player.conditions.conditions[0].roundsRemaining=7;
 check(saveAccountCharacter(0)==nullptr,"first slot save");
 check(saveAccountCharacter(1)!=nullptr,"same character cannot be cloned to another slot");
 check(saveAccountCharacter(3)!=nullptr,"fourth saved slot denied");
 Character loaded{};check(loadGameFromSlot(loaded,0)&&loaded.conditions.count==1&&loaded.inventory.gold==501&&loaded.level==3,"full native condition/gold/level roundtrip");
 for(unsigned slot=1;slot<3;++slot){
  check(newAccountCharacter()==nullptr,"new character preserves outgoing save");create(runtime,owner,1,slot==1?"Wizard":"Rogue");
  player.inventory.gold=100+slot;check(saveAccountCharacter(slot)==nullptr,"new character gets distinct slot");
 }
 // Load through the actual original menu, with an explicit default-No prompt.
 openTownHome();rotateTownHomeSelection(true);rotateTownHomeSelection(true);
 check(getTownHomeSelection()==TOWN_HOME_LOAD_GAME,"Home Save and Load share original screen");capture("home");
 openLoadGameMenu();capture("load");check(std::strstr(menuBodyScratch,"Current: Rogue")&&std::strstr(menuBodyScratch,"Fighter"),"current and saved names/levels shown");
 choose(runtime,owner,1,"Slot 1");check(std::string(player.name)=="Rogue","selection does not silently discard character");
 choose(runtime,owner,1,"Yes");check(std::string(player.name)=="Fighter"&&player.level==3,"Home confirmed Load restores own character");
 player.inventory.gold=3;check(loadAccountCharacter(0,loaded)==nullptr&&loaded.inventory.gold==3,"same-slot load flushes latest state, cannot rewind inventory");player=loaded;
 check(game.join("Peer",peer)==SessionError::Ok&&runtime.start(peer,2,"acct-distinct-peer"),"second verified owner");const auto peerId=selectedController();
 check(!loadGameFromSlot(loaded,0),"another account cannot read owner's slot");create(runtime,peer,2,"Other");
 check(saveAccountCharacter(0)==nullptr,"peer slot independent");
 selectController(id);openLoadGameMenu();selectController(peerId);openLoadGameMenu();
 check(runtime.pixels(owner,1)&&std::strstr(menuBodyScratch,"Fighter")&&!std::strstr(menuBodyScratch,"Other"),"owner save summaries never show peer names");
 check(runtime.pixels(peer,2)&&std::strstr(menuBodyScratch,"Other")&&!std::strstr(menuBodyScratch,"Fighter"),"peer save summaries remain private");
 closeMenu();wait(runtime,350);
 // Form a real party using production menus and join code.
 townSelection=TOWN_BULLETIN_BOARD;tap(runtime,peer,2,"a");choose(runtime,peer,2,"Create Party");
 PartySnapshot party;check(nativePartySnapshot(peerId,party),"real party");tap(runtime,peer,2,"b");tap(runtime,peer,2,"b");
 check(selectController(id),"owner select");closeMenu();closeTownHome();wait(runtime,350);townSelection=TOWN_BULLETIN_BOARD;tap(runtime,owner,1,"a");choose(runtime,owner,1,"Join Party");
 for(unsigned n=0;n<5;++n){char letter[2]={party.joinCode[n],0};choose(runtime,owner,1,letter);}tap(runtime,owner,1,"b");tap(runtime,owner,1,"b");
 selectController(peerId);townSelection=TOWN_DUNGEON;tap(runtime,peer,2,"a");check(nativeActor(peerId),"peer in production dungeon");
 selectController(id);check(nativeActor(id),"owner in same dungeon");check(loadAccountCharacter(1,loaded)!=nullptr,"cannot replace an exploring actor");
 // Production Return to Town action leaves the other player exploring.
 tap(runtime,owner,1,"a");choose(runtime,owner,1,"Game");choose(runtime,owner,1,"Return to Town");
 check(gameState==GAME_TOWN&&!nativeActor(id),"own production return to Town");
 const auto beforeDungeon=dungeon;auto* retained=nativeActor(peerId);check(retained,"peer remains in dungeon");const auto retainedActor=*retained;
 const auto clock=worldTime();combat.active=true;check(loadAccountCharacter(1,loaded)!=nullptr,"shared initiative blocks switch");combat.active=false;
 openLoadGameMenu();check(activateSaveMenuAction(MENU_LOAD_SLOT_2)&&activateSaveMenuAction(MENU_OVERWRITE_YES),"actual Load routing");
 check(std::string(player.name)=="Wizard","own alternate character loaded");
 check(worldTime()==clock&&dungeon.runActive==beforeDungeon.runActive&&dungeon.completed==beforeDungeon.completed,"load preserves shared world and clock");
 check(!std::memcmp(dungeon.rooms,beforeDungeon.rooms,sizeof(dungeon.rooms)),"load preserves every dungeon room");
 check(nativeActor(peerId)==retained&&retained->x==retainedActor.x&&retained->y==retainedActor.y&&retained->character.inventory.gold==retainedActor.character.inventory.gold,"other actor and position unchanged");
 player.inventory.gold=47;runtime.detach(owner,1);check(runtime.start(owner,3,"acct-stable-owner"),"verified reconnect");
 check(std::string(player.name)=="Wizard"&&player.inventory.gold==47,"reconnect retains correct character");check(runtime.pixels(owner,1)==nullptr,"stale serial denied");
 check(loadGameFromSlot(loaded,1)&&loaded.inventory.gold==47,"disconnect flushes current character");
 player.inventory.gold=48;runtime.tick(30000);selectController(id);check(loadGameFromSlot(loaded,1)&&loaded.inventory.gold==48,"bounded periodic autosave");
 auto file=root/ownerDirectory/"save_slot_3.dat";const auto size=fs::file_size(file);
 {std::ofstream out(file,std::ios::binary|std::ios::trunc);out<<"corrupt";}
 check(!loadGameFromSlot(loaded,2),"corrupt character rejected without disconnect");
 Character cleric{};createCharacter(cleric,CLASS_CLERIC,WEAPON_GROUP_NONE);setCharacterName(cleric,"Spent");
 check(cleric.classAbilities.channelEnergyMax>0,"real Cleric channel resource");cleric.classAbilities.channelEnergyCurrent=0;
 check(saveGameToSlot(cleric,2)&&loadGameFromSlot(loaded,2)&&loaded.classAbilities.channelEnergyCurrent==0,"load cannot refill spent channel energy");
 Character rogue{};createCharacter(rogue,CLASS_ROGUE,WEAPON_GROUP_NONE);setCharacterName(rogue,"Rogue");check(saveGameToSlot(rogue,2),"repair own corrupt slot");check(fs::file_size(file)==size,"bounded native payload");
#ifdef __linux__
 fs::permissions(root,fs::perms::owner_all|fs::perms::group_read,fs::perm_options::replace);
 check(!saveGameToSlot(player,1),"insecure root permissions fail closed");fs::permissions(root,fs::perms::owner_all,fs::perm_options::replace);
 const auto other=root/"not-a-save";{std::ofstream o(other);o<<"untouched";}fs::remove(file);fs::create_symlink(other,file);
 check(!loadGameFromSlot(loaded,2)&&!saveGameToSlot(player,2),"symlink reads and writes denied");check(fs::file_size(other)==9,"symlink target untouched");fs::remove(file);check(saveGameToSlot(rogue,2),"restore regular owned slot");fs::remove(other);
#endif
 std::cout<<"PASS 3 account slots, full native state, Home Save/Load, world preservation with peer exploring, ownership, reconnect, autosave, fail-closed files\n";
 if(argc<=2){CharacterFiles::reset();fs::remove_all(root);}return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" "<<diagnostic()<<'\n';return 1;}}
