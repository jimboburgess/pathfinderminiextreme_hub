#include "native_party.h"
#include "runtime.h"
#include "native_rooms.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "forest/forest.h"
#include "town/town.h"
#include "characters/sheet.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <memory>
#include <string>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/random.h>
#include <cerrno>
#endif
namespace pfme { namespace runtime {
namespace {
class Entropy final:public PartyEntropy {
 bool next(uint64_t& value) override {
#ifdef _WIN32
  return BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),sizeof(value),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0;
#else
  size_t used=0;while(used<sizeof(value)){
   auto n=getrandom(reinterpret_cast<char*>(&value)+used,sizeof(value)-used,0);
   if(n<0 && errno==EINTR)continue;if(n<=0)return false;used+=static_cast<size_t>(n);
  }return true;
#endif
 }
};
Entropy entropy;
std::unique_ptr<PartyRegistry> registry;
bool travelling=false;
constexpr const char* alphabet="23456789ABCDEFGHJKMNPQRSTUVWXYZ";
struct Social {
 bool member=false;PartyHandle party{};PartyMembership membership{};unsigned world=0;
 char name[NETWORK_PLAYER_NAME_SIZE]{};
 enum Pane {Board,Code,Status} pane=Board;
 char code[PARTY_JOIN_CODE_SIZE]{};unsigned entered=0;
 char title[36]{},body[256]{},oldBody[256]{};
 MenuItem items[32]{};char letters[32][2]{};Menu menu{};
};
std::array<Social,MAX_MULTIPLAYER_PLAYERS> social;
const Menu* currentMenu(){return menuState.isOpen && menuState.depth?menuState.menuStack[menuState.depth-1]:nullptr;}
bool sameParty(PlayerID a,PlayerID b){
 if(!social[a].member||!social[b].member)return false;
 PartySnapshot first,second;
 return registry->snapshot(social[a].party,first)==PartyError::Ok && registry->snapshot(social[b].party,second)==PartyError::Ok && first.id.high==second.id.high && first.id.low==second.id.low;
}
PartyPlayerProfile profile(PlayerID id){
 PartyPlayerProfile result{};std::snprintf(result.displayName,sizeof(result.displayName),"%s",social[id].name[0]?social[id].name:"Adventurer");
 for(auto& c:result.displayName)if(c==' ')c='_';
 const auto* actor=nativeActor(id);const Character& character=actor?actor->character:player;
 result.characterClass=character.characterClass;result.level=character.level;return result;
}
void showError(PartyError error){char text[96];std::snprintf(text,sizeof(text),"Party: %s",partyErrorName(error));setGameMessage(text);}
bool town(){return gameState==GAME_TOWN && !isTownRestActive();}
bool create(PlayerID id){
 if(!town()||social[id].member){setGameMessage("Create a party in Town first.");return false;}
 auto& s=social[id];auto error=registry->createParty(profile(id),s.party,s.membership);
 if(error!=PartyError::Ok){showError(error);return false;}
 bool used[NATIVE_WORLD_CAPACITY]{};for(auto& other:social)if(other.member)used[other.world]=true;
 unsigned world=!used[id]?id:0;while(world<NATIVE_WORLD_CAPACITY && used[world])++world;
 if(world==NATIVE_WORLD_CAPACITY){registry->leave(s.membership.handle);setGameMessage("Party runtime full.");return false;}
 s.member=true;s.world=world;assignNativeWorld(id,world);selectNativeWorld(world);resetNativeWorld();
 nativePartyMessage("Party created. Share the code.");return true;
}
void item(Social& s,unsigned i,const char* title,const char* description){s.items[i]={title,description,MENU_NONE,nullptr,MENU_CLASS_ALL,ABILITY_NONE};}
void build(PlayerID id,Social::Pane pane){
 auto& s=social[id];s.pane=pane;std::memset(s.body,0,sizeof(s.body));
 if(pane==Social::Board){
  std::snprintf(s.title,sizeof(s.title),"Bulletin Board");
  item(s,0,"Create Party","Become leader; share the join code.");
  item(s,1,"Join Party","Enter a five-character party code.");
  item(s,2,"Party Status","Members, classes and connection status.");
  item(s,3,"Leave Party","In Town: leader departure closes party.");
  s.menu={s.title,s.items,4,nullptr,nullptr,s.body,30};
 }else if(pane==Social::Code){
  std::snprintf(s.title,sizeof(s.title),"Join Code: %s",s.code);
  for(unsigned i=0;alphabet[i];++i){s.letters[i][0]=alphabet[i];s.letters[i][1]=0;item(s,i,s.letters[i],"Choose each character; B cancels.");}
  s.menu={s.title,s.items,static_cast<uint8_t>(std::strlen(alphabet)),nullptr,nullptr,nullptr,0};
 }else{
  std::snprintf(s.title,sizeof(s.title),"Party Status");item(s,0,"Back","Return to the Bulletin Board.");
  s.menu={s.title,s.items,1,nullptr,nullptr,s.body,112};
 }
 openMenu(&s.menu);refreshNativePartyMenu();
}
bool leaveSocial(PlayerID id){
 auto& s=social[id];if(!s.member){setGameMessage("Not in a party.");return true;}
 if(!town()||nativeActor(id)||combat.active){setGameMessage("Return to Town before leaving.");return false;}
 PartySnapshot before;if(registry->snapshot(s.party,before)!=PartyError::Ok){s.member=false;return true;}
 bool peers[MAX_MULTIPLAYER_PLAYERS]{};for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer)peers[peer]=sameParty(id,peer);
 const bool leader=s.membership.player==HOST_PLAYER_ID;
 if(leader)for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer)if(peers[peer] && nativeActor(peer)){
  setGameMessage("All members must return to Town.");return false;
 }
 auto error=registry->leave(s.membership.handle);if(error!=PartyError::Ok){showError(error);return false;}
 s.member=false;
 if(leader){for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer)if(peers[peer]){social[peer].member=false;assignNativeWorld(peer,peer);}}
 else assignNativeWorld(id,id);
 selectNativeWorld(nativeControllerWorld(id));
 const std::string message=leader?"Party closed; characters retained.":"A member left the party.";
 for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer)if(peers[peer] && selectController(peer)){if(!social[peer].member)resetNativeWorld();setGameMessage(message.c_str());}
 selectController(id);if(!leader)setGameMessage("Left party; character retained.");return true;
}
}
bool socialPartiesEnabled(){return registry!=nullptr;}
void initializeNativeParties(uint32_t){
 registry=std::make_unique<PartyRegistry>(entropy);
 for(PlayerID id=0;id<MAX_MULTIPLAYER_PLAYERS;++id)assignNativeWorld(id,id);
 selectNativeWorld(0);
}
void resetNativePartyCharacter(PlayerID id){if(isValidPlayerID(id))social[id]=Social{};}
void nativePartyCharacterName(PlayerID id,const char* name){if(!isValidPlayerID(id))return;
 if(!social[id].name[0] && registry){bool used[NATIVE_WORLD_CAPACITY]{};for(const auto& other:social)if(other.member)used[other.world]=true;
  unsigned world=!used[id]?id:0;while(world<NATIVE_WORLD_CAPACITY && used[world])++world;
  if(world<NATIVE_WORLD_CAPACITY){assignNativeWorld(id,world);selectNativeWorld(world);}
 }
 std::snprintf(social[id].name,sizeof(social[id].name),"%s",name);
}
bool nativePartySnapshot(PlayerID id,PartySnapshot& snapshot){return registry && isValidPlayerID(id) && social[id].member && registry->snapshot(social[id].party,snapshot)==PartyError::Ok;}
void openNativePartyMenu(){if(registry)build(selectedController(),Social::Board);}
bool cancelNativePartyMenu(){
 const auto id=selectedController();
 if(!registry || !isValidPlayerID(id) || currentMenu()!=&social[id].menu || social[id].pane==Social::Board)return false;
 build(id,Social::Board);return true;
}
bool activateNativePartyMenu(){
 const auto id=selectedController();if(!registry||!isValidPlayerID(id)||currentMenu()!=&social[id].menu)return false;
 auto& s=social[id];const auto index=menuState.cursorIndex;
 if(s.pane==Social::Status){build(id,Social::Board);return true;}
 if(s.pane==Social::Code){
  if(index>=std::strlen(alphabet)||s.entered>=PARTY_JOIN_CODE_LENGTH)return true;
  s.code[s.entered++]=alphabet[index];s.code[s.entered]=0;
  if(s.entered<PARTY_JOIN_CODE_LENGTH){build(id,Social::Code);return true;}
  if(!town()||s.member){setGameMessage("Already in a party or not in Town.");build(id,Social::Board);return true;}
  PartyHandle party;PartyMembership membership;
  auto error=registry->findParty(s.code,party);
  if(error==PartyError::Ok)error=registry->joinParty(s.code,profile(id),membership);
  if(error!=PartyError::Ok){build(id,Social::Board);showError(error);return true;}
  PartySnapshot joined;registry->snapshot(party,joined);
  unsigned world=NATIVE_WORLD_CAPACITY;
  for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer){PartySnapshot p;if(nativePartySnapshot(peer,p) && p.id.high==joined.id.high && p.id.low==joined.id.low){world=social[peer].world;break;}}
  if(world==NATIVE_WORLD_CAPACITY){registry->leave(membership.handle);build(id,Social::Board);setGameMessage("Party runtime unavailable.");return true;}
  s.member=true;s.party=party;s.membership=membership;s.world=world;
  assignNativeWorld(id,world);selectNativeWorld(world);build(id,Social::Status);nativePartyMessage("An adventurer joined the party.");return true;
 }
 if(index==0){if(create(id))build(id,Social::Status);}
 else if(index==1){if(s.member)setGameMessage("Leave your current party first.");else{s.entered=0;s.code[0]=0;build(id,Social::Code);}}
 else if(index==2)build(id,Social::Status);
 else if(index==3){if(leaveSocial(id))build(id,Social::Board);}
 return true;
}
void refreshNativePartyMenu(){
 const auto id=selectedController();if(!registry||!isValidPlayerID(id))return;
 auto& s=social[id];
 if(s.member){
  const auto location=gameState==GAME_FOREST?MultiplayerMemberLocation::FOREST:gameState==GAME_DUNGEON?MultiplayerMemberLocation::DUNGEON:MultiplayerMemberLocation::TOWN;
  registry->updateMember(s.membership.handle,profile(id),location);
 }
 if(!menuState.isOpen||currentMenu()!=&s.menu||s.pane==Social::Code)return;
 PartySnapshot party;
 if(!nativePartySnapshot(id,party))std::snprintf(s.body,sizeof(s.body),"No party. Create or join in Town.");
 else if(s.pane==Social::Board)std::snprintf(s.body,sizeof(s.body),"Code %s  %u/4",party.joinCode,party.memberCount);
 else{
  int used=std::snprintf(s.body,sizeof(s.body),"Code %s %s\n",party.joinCode,partyActivityName(party.activity));
  for(const auto& member:party.members)if(member.occupied && used>0 && static_cast<size_t>(used)<sizeof(s.body))
   used+=std::snprintf(s.body+used,sizeof(s.body)-used,"%s%s Lv%u\n%s %s\n",member.leader?"* ":"",member.profile.displayName,member.profile.level,partyCharacterClassName(member.profile.characterClass),member.connected?partyLocationName(member.location):"offline");
 }
 if(std::strcmp(s.body,s.oldBody)){std::snprintf(s.oldBody,sizeof(s.oldBody),"%s",s.body);requestMenuRedraw();}
}
void nativePartyMessage(const char* text){
 if(!registry)return;const auto owner=selectedController();const std::string message=text?text:"";
 for(PlayerID id=0;id<MAX_MULTIPLAYER_PLAYERS;++id)if(sameParty(owner,id) && selectController(id))setGameMessage(message.c_str());
 selectController(owner);
}
void nativePartyConnection(PlayerID id,bool connected){
 if(!registry||!isValidPlayerID(id)||!social[id].member)return;
 auto& s=social[id];
 if(connected){PartyMembership resumed;const auto error=registry->reconnect(s.party,s.membership.player,s.membership.reconnectToken,resumed);if(error==PartyError::Ok)s.membership=resumed;}
 else registry->disconnect(s.membership.handle);
}
bool retireNativePartyMember(PlayerID id){
 if(!registry||!social[id].member)return true;
 const auto owner=selectedController();
 bool peers[MAX_MULTIPLAYER_PLAYERS]{};const bool leader=social[id].membership.player==HOST_PLAYER_ID;
 for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer)peers[peer]=peer==id || (leader && sameParty(id,peer));
 for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer)if(peers[peer] && selectController(peer)){
  if(combat.active){selectController(owner);return false;}
  if(nativeActor(peer) && !leaveNativeRoom(dungeon)){selectController(owner);return false;}
  gameState=GAME_TOWN;closeMenu();closeTownHome();closeCharacterSheet();backgroundNeedsRedraw=true;redrawType=REDRAW_FULL;needsRedraw=true;
  nativePartyReturnedToTown();
 }
 selectController(id);const bool result=leaveSocial(id);selectController(owner);return result;
}
void nativePartyReturnedToTown(){
 if(!registry)return;const auto id=selectedController();if(!social[id].member)return;
 refreshNativePartyMenu();
 for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer)if(sameParty(id,peer) && nativeActor(peer))return;
 registry->returnToTown(social[id].membership.handle);
}
bool canResetNativePartyAdventure(){
 if(!registry)return false;const auto id=selectedController();
 if(!isValidPlayerID(id)||!social[id].member||social[id].membership.player!=HOST_PLAYER_ID||!town()||combat.active)return false;
 for(PlayerID peer=0;peer<MAX_MULTIPLAYER_PLAYERS;++peer)if(sameParty(id,peer)&&nativeActor(peer))return false;
 return true;
}
bool resetNativePartyAdventure(){
 if(!canResetNativePartyAdventure()){setGameMessage("Only the leader can reset after all return.");return false;}
 resetNativeWorld();return true;
}
bool startNativePartyAdventure(bool forest){
 if(!registry||travelling)return false;
 const auto owner=selectedController();if(!isValidPlayerID(owner))return true;
 if(!town()){setGameMessage("Travel begins in Town.");return true;}
 if(!social[owner].member && !create(owner))return true;
 auto& s=social[owner];PartySnapshot party;
 if(!nativePartySnapshot(owner,party))return true;
 if(s.membership.player!=HOST_PLAYER_ID){setGameMessage("Only the party leader starts travel.");return true;}
 bool members[MAX_MULTIPLAYER_PLAYERS]{};
 for(PlayerID id=0;id<MAX_MULTIPLAYER_PLAYERS;++id)if(sameParty(owner,id)){
  members[id]=true;
  selectController(id);
  const auto local=social[id].membership.player;
  if(!party.members[local].connected || !town() || player.health.currentHP<=0){selectController(owner);setGameMessage("All members must be connected in Town.");return true;}
 }
 selectController(owner);
 // A completed run is not a resumable dungeon. All members are already
 // verified home above, so a new adventure can safely replace this world.
 if(!forest && dungeon.runActive && dungeon.completed)resetNativeWorld();
 auto error=forest?registry->startForest(s.membership.handle):registry->startDungeon(s.membership.handle);
 if(error!=PartyError::Ok){showError(error);return true;}
 registry->setLocked(s.membership.handle,true);travelling=true;bool fresh=true;
 for(PlayerID id=0;id<MAX_MULTIPLAYER_PLAYERS;++id)if(members[id]){
  selectController(id);closeMenu();closeTownHome();closeCharacterSheet();
  if(forest){if(!enterNativeForestController(fresh))throw std::runtime_error("Party forest initialization failed");fresh=false;}
  else enterDungeon();
  refreshNativePartyMenu();
 }
 travelling=false;selectController(owner);nativePartyMessage(forest?"Party entered the forest.":"Party entered the dungeon.");return true;
}
}}
