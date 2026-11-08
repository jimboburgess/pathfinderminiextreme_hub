#include "native_party.h"
#include "runtime.h"
#include "native_rooms.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "forest/forest.h"
#include "town/town.h"
#include "characters/sheet.h"
#include "characters/items.h"
#include "dungeon/actioneconomy.h"
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
 enum Pane {Board,Code,Status,TradeRecipients,TradeItems,TradeReview,TradeWaiting} pane=Board;
 char code[PARTY_JOIN_CODE_SIZE]{};unsigned entered=0;
 char title[36]{},body[256]{},oldBody[256]{};
 MenuItem items[32]{};char letters[32][2]{};char rowTitles[32][48]{};
 PlayerID tradeRecipient=INVALID_PLAYER_ID;
 PlayerID tradeRecipientLookup[MAX_MULTIPLAYER_PLAYERS]{};
 bool selectedTradeItems[MAX_INVENTORY]{};
 Menu menu{};
};
std::array<Social,NATIVE_CONTROLLER_CAPACITY> social;
class RoutedTradeBook {
 std::array<TradeOfferBook,NATIVE_WORLD_CAPACITY> books{};
 std::array<TradeOffer,NATIVE_CONTROLLER_CAPACITY> views{};
 PlayerID local(PlayerID id)const{return isValidController(id)&&social[id].member?social[id].membership.player:INVALID_PLAYER_ID;}
 PlayerID global(PlayerID localID,unsigned world)const{for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(social[id].member&&social[id].world==world&&social[id].membership.player==localID)return id;return INVALID_PLAYER_ID;}
public:
 const TradeOffer* pendingFor(PlayerID id){auto member=local(id);if(!isValidPlayerID(member))return nullptr;auto world=social[id].world;auto* offer=books[world].pendingFor(member);if(!offer)return nullptr;views[id]=*offer;views[id].sender=global(offer->sender,world);views[id].recipient=global(offer->recipient,world);return &views[id];}
 void cancelForPlayer(PlayerID id){auto member=local(id);if(isValidPlayerID(member))books[social[id].world].cancelForPlayer(member);}
 bool cancel(uint32_t offer){return books[nativeWorldIndex()].cancel(offer);}
 TradeResult propose(PlayerID sender,PlayerID recipient,const TradeOfferLine* lines,uint8_t count,const Character& character,uint32_t& offer){return books[nativeWorldIndex()].propose(local(sender),local(recipient),lines,count,character,offer);}
 TradeResult accept(uint32_t offer,PlayerID recipient,Character& sender,Character& target){return books[nativeWorldIndex()].accept(offer,local(recipient),sender,target);}
 TradeResult decline(uint32_t offer,PlayerID recipient){return books[nativeWorldIndex()].decline(offer,local(recipient));}
} tradeOffers;
unsigned privateTownWorld(PlayerID id){bool used[NATIVE_WORLD_CAPACITY]{};for(const auto& other:social)if(other.member)used[other.world]=true;unsigned world=!used[id]?id:0;while(world<NATIVE_WORLD_CAPACITY&&used[world])++world;if(world==NATIVE_WORLD_CAPACITY)throw std::runtime_error("No private Town world");return world;}
const Menu* currentMenu(){return menuState.isOpen && menuState.depth?menuState.menuStack[menuState.depth-1]:nullptr;}
bool sameParty(PlayerID a,PlayerID b){
 if(!social[a].member||!social[b].member)return false;
 PartySnapshot first,second;
 return registry->snapshot(social[a].party,first)==PartyError::Ok && registry->snapshot(social[b].party,second)==PartyError::Ok && first.id.high==second.id.high && first.id.low==second.id.low;
}
bool connectedPartyMember(PlayerID id){
 if(!isValidController(id)||!social[id].member)return false;
 PartySnapshot snapshot;if(registry->snapshot(social[id].party,snapshot)!=PartyError::Ok)return false;
 const PlayerID member=social[id].membership.player;
 return isValidPlayerID(member)&&snapshot.members[member].occupied&&snapshot.members[member].connected;
}
PartyPlayerProfile profile(PlayerID id){
 PartyPlayerProfile result{};std::snprintf(result.displayName,sizeof(result.displayName),"%s",social[id].name[0]?social[id].name:"Adventurer");
 for(auto& c:result.displayName)if(c==' ')c='_';
 const auto* actor=nativeActor(id);const Character& character=actor?actor->character:player;
 result.characterClass=character.characterClass;result.level=character.level;return result;
}
void showError(PartyError error){char text[96];std::snprintf(text,sizeof(text),"Party: %s",partyErrorName(error));setGameMessage(text);}
void partyLifecycle(const char* event,PlayerID id,const PartySnapshot& party){
 std::fprintf(stderr,"{\"event\":\"%s\",\"controller\":%u,\"party\":\"%016llx%016llx\",\"player_id\":%u}\n",event,unsigned(id),static_cast<unsigned long long>(party.id.high),static_cast<unsigned long long>(party.id.low),unsigned(social[id].membership.player));
}
bool town(){return gameState==GAME_TOWN && !isTownRestActive();}
bool create(PlayerID id){
 if(!town()||social[id].member){setGameMessage("Create a party in Town first.");return false;}
 auto& s=social[id];auto error=registry->createParty(profile(id),s.party,s.membership);
 if(error!=PartyError::Ok){showError(error);return false;}
 bool used[NATIVE_WORLD_CAPACITY]{};for(auto& other:social)if(other.member)used[other.world]=true;
 unsigned world=!used[id]?id:0;while(world<NATIVE_WORLD_CAPACITY && used[world])++world;
 if(world==NATIVE_WORLD_CAPACITY){registry->leave(s.membership.handle);setGameMessage("Party runtime full.");return false;}
 s.member=true;s.world=world;assignNativeWorld(id,world);selectNativeWorld(world);resetNativeWorld();
 PartySnapshot created;registry->snapshot(s.party,created);partyLifecycle("JOINED_PARTY",id,created);
 nativePartyMessage("Party created. Share the code.");return true;
}
void item(Social& s,unsigned i,const char* title,const char* description){s.items[i]={title,description,MENU_NONE,nullptr,MENU_CLASS_ALL,ABILITY_NONE};}
Character selectedCharacter(PlayerID id){
 selectController(id);if(auto* actor=nativeActor(id))return actor->character;return player;
}
void assignSelectedCharacter(PlayerID id,const Character& character){
 selectController(id);if(auto* actor=nativeActor(id))actor->character=character;else player=character;
}
void messagePlayer(PlayerID id,const char* text){
 const auto owner=selectedController();if(selectController(id))setGameMessage(text);selectController(owner);
}
bool mayCommitCombatTrade(PlayerID id){
 if(!combat.active)return true;
 Entity* actor=nativeActor(id);
 return actor&&getCurrentCombatant()==actor&&isPlayerTurn()&&combat.waitingForPlayer&&
  canCharacterAct(actor->character)&&canUseStandardAction(actor->turn);
}
void buildTrade(PlayerID id){
 auto& s=social[id];const TradeOffer* pending=tradeOffers.pendingFor(id);
 std::memset(s.body,0,sizeof(s.body));
 if(pending){
  if(pending->recipient==id){
   s.pane=Social::TradeReview;std::snprintf(s.title,sizeof(s.title),"Trade from %s",social[pending->sender].name);
   int used=0;for(uint8_t line=0;line<pending->lineCount&&used>=0&&static_cast<size_t>(used)<sizeof(s.body);++line){
    char name[48];formatItemInstanceName(pending->lines[line].item,name,sizeof(name),true);
    used+=std::snprintf(s.body+used,sizeof(s.body)-used,"%s x%u\n",name,unsigned(pending->lines[line].quantity));
   }
   item(s,0,"Accept","Accept this one-way item offer.");item(s,1,"Decline","Decline without moving any item.");
   s.menu={s.title,s.items,2,nullptr,nullptr,s.body,96};
  }else{
   s.pane=Social::TradeWaiting;std::snprintf(s.title,sizeof(s.title),"Trade Pending");
   std::snprintf(s.body,sizeof(s.body),"Waiting for %s to accept or decline.",social[pending->recipient].name);
   item(s,0,"Cancel Offer","Keep every offered item.");s.menu={s.title,s.items,1,nullptr,nullptr,s.body,76};
  }
  openMenu(&s.menu);return;
 }
 s.pane=Social::TradeRecipients;std::snprintf(s.title,sizeof(s.title),"Trade Items");unsigned count=0;
 for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(peer!=id&&sameParty(id,peer)&&connectedPartyMember(peer)){
  std::snprintf(s.rowTitles[count],sizeof(s.rowTitles[count]),"%s",social[peer].name);
  item(s,count,s.rowTitles[count],"Offer items regardless of party distance.");s.tradeRecipientLookup[count]=peer;++count;
 }
 if(!count){std::snprintf(s.rowTitles[0],sizeof(s.rowTitles[0]),"No member available");item(s,0,s.rowTitles[0],"Connected party members appear here.");count=1;}
 s.menu={s.title,s.items,static_cast<uint8_t>(count),nullptr,nullptr,"Choose a party member.",30};openMenu(&s.menu);
}
void buildTradeItems(PlayerID id,PlayerID recipient){
 auto& s=social[id];s.pane=Social::TradeItems;s.tradeRecipient=recipient;
 std::snprintf(s.title,sizeof(s.title),"Offer to %s",social[recipient].name);
 Character character=selectedCharacter(id);const uint8_t itemCount=character.inventory.itemCount;
 for(uint8_t index=0;index<itemCount;++index){
  char name[40];formatItemInstanceName(character.inventory.slots[index].item,name,sizeof(name),true);
  std::snprintf(s.rowTitles[index],sizeof(s.rowTitles[index]),"[%c] %s",s.selectedTradeItems[index]?'x':' ',name);
  item(s,index,s.rowTitles[index],"Toggle one unit of this item.");
 }
 std::snprintf(s.rowTitles[itemCount],sizeof(s.rowTitles[itemCount]),"Send Offer");
 item(s,itemCount,s.rowTitles[itemCount],"Recipient must accept before ownership changes.");
 std::snprintf(s.rowTitles[itemCount+1],sizeof(s.rowTitles[itemCount+1]),"Cancel");
 item(s,itemCount+1,s.rowTitles[itemCount+1],"Return without making an offer.");
 s.menu={s.title,s.items,static_cast<uint8_t>(itemCount+2),nullptr,nullptr,"Select up to four item stacks; one unit each.",42};openMenu(&s.menu);
}
void build(PlayerID id,Social::Pane pane){
 auto& s=social[id];s.pane=pane;std::memset(s.body,0,sizeof(s.body));
 if(pane==Social::Board){
  std::snprintf(s.title,sizeof(s.title),"Bulletin Board");
  item(s,0,"Create Party","Become leader; share the join code.");
  item(s,1,"Join Party","Enter a five-character party code.");
  item(s,2,"Party Status","Members, classes and connection status.");
  item(s,3,"Trade Items","Offer carried items to a party member.");
  item(s,4,"Leave Party","In Town: leader departure closes party.");
  s.menu={s.title,s.items,5,nullptr,nullptr,s.body,30};
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
 bool peers[NATIVE_CONTROLLER_CAPACITY]{};for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)peers[peer]=sameParty(id,peer);
 const bool leader=s.membership.player==HOST_PLAYER_ID;
 if(leader)for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(peers[peer] && nativeActor(peer)){
  setGameMessage("All members must return to Town.");return false;
 }
 tradeOffers.cancelForPlayer(id);
 auto error=registry->leave(s.membership.handle);if(error!=PartyError::Ok){showError(error);return false;}
 partyLifecycle("LEFT_PARTY",id,before);s.member=false;
 if(leader){for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(peers[peer]){tradeOffers.cancelForPlayer(peer);social[peer].member=false;}}
 for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(peers[peer]&&!social[peer].member)assignNativeWorld(peer,privateTownWorld(peer));
 selectNativeWorld(nativeControllerWorld(id));
 const std::string message=leader?"Party closed; characters retained.":"A member left the party.";
 for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(peers[peer] && selectController(peer)){if(!social[peer].member)resetNativeWorld();setGameMessage(message.c_str());}
 selectController(id);if(!leader)setGameMessage("Left party; character retained.");return true;
}
}
PlayerID nativePartyPlayerID(ControllerID id){return isValidController(id)&&social[id].member?social[id].membership.player:static_cast<PlayerID>(id%MAX_MULTIPLAYER_PLAYERS);}
bool socialPartiesEnabled(){return registry!=nullptr;}
void initializeNativeParties(uint32_t){
 registry=std::make_unique<PartyRegistry>(entropy);
 for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)assignNativeWorld(id,id);
 selectNativeWorld(0);
}
void resetNativePartyCharacter(PlayerID id){if(isValidController(id)){tradeOffers.cancelForPlayer(id);social[id]=Social{};}}
void nativePartyCharacterName(PlayerID id,const char* name){if(!isValidController(id))return;
 if(!social[id].name[0] && registry){bool used[NATIVE_WORLD_CAPACITY]{};for(const auto& other:social)if(other.member)used[other.world]=true;
  unsigned world=!used[id]?id:0;while(world<NATIVE_WORLD_CAPACITY && used[world])++world;
  if(world<NATIVE_WORLD_CAPACITY){assignNativeWorld(id,world);selectNativeWorld(world);}
 }
 std::snprintf(social[id].name,sizeof(social[id].name),"%s",name);
}
bool nativePartySnapshot(PlayerID id,PartySnapshot& snapshot){return registry && isValidController(id) && social[id].member && registry->snapshot(social[id].party,snapshot)==PartyError::Ok;}
const TradeOffer* pendingNativeTrade(PlayerID player){return tradeOffers.pendingFor(player);}
bool canOpenNativeTradeMenu(){
 const PlayerID id=selectedController();if(!registry||!isValidController(id)||!social[id].member)return false;
 if(tradeOffers.pendingFor(id))return true;
 PartySnapshot snapshot;if(!nativePartySnapshot(id,snapshot)||snapshot.memberCount<2)return false;
 return mayCommitCombatTrade(id);
}
void openNativeTradeMenu(){
 const PlayerID id=selectedController();if(!canOpenNativeTradeMenu()){setGameMessage("Trade is unavailable now.");return;}
 std::memset(social[id].selectedTradeItems,0,sizeof(social[id].selectedTradeItems));buildTrade(id);
}
TradeResult proposeNativeTrade(PlayerID recipient,const TradeOfferLine* lines,uint8_t lineCount,uint32_t& offerID){
 const PlayerID sender=selectedController();offerID=0;
 if(!registry||!isValidController(sender)||!isValidController(recipient)||
    !sameParty(sender,recipient)||!connectedPartyMember(recipient))return TradeResult::INVALID_PLAYER;
 if(!mayCommitCombatTrade(sender))return TradeResult::PLAYER_BUSY;
 Character senderCharacter=selectedCharacter(sender);
 TradeResult result=tradeOffers.propose(sender,recipient,lines,lineCount,senderCharacter,offerID);
 if(result!=TradeResult::SUCCESS)return result;
 if(combat.active){Entity* actor=nativeActor(sender);if(actor)actor->turn.standardActionUsed=true;}
 char offered[48];formatItemInstanceName(lines[0].item,offered,sizeof(offered));
 char recipientMessage[96];std::snprintf(recipientMessage,sizeof(recipientMessage),"%s offers you %s.",social[sender].name,offered);
 char senderMessage[96];std::snprintf(senderMessage,sizeof(senderMessage),"Offer sent to %s.",social[recipient].name);
 messagePlayer(recipient,recipientMessage);messagePlayer(sender,senderMessage);selectController(sender);
 if(combat.active)checkEndPlayerTurn();
 return TradeResult::SUCCESS;
}
TradeResult respondToNativeTrade(bool accept){
 const PlayerID recipient=selectedController();const TradeOffer* pending=tradeOffers.pendingFor(recipient);
 if(!pending||pending->recipient!=recipient)return TradeResult::NOT_FOUND;
 const uint32_t offerID=pending->id;const PlayerID sender=pending->sender;
 if(!sameParty(sender,recipient)||!connectedPartyMember(sender)){
  tradeOffers.cancel(offerID);return TradeResult::INVALID_PLAYER;
 }
 if(!accept){TradeResult result=tradeOffers.decline(offerID,recipient);if(result==TradeResult::SUCCESS){
  char text[96];std::snprintf(text,sizeof(text),"%s declined the trade.",social[recipient].name);messagePlayer(sender,text);messagePlayer(recipient,"Trade declined.");selectController(recipient);}return result;}
 Character senderCharacter=selectedCharacter(sender);Character recipientCharacter=selectedCharacter(recipient);
 TradeResult result=tradeOffers.accept(offerID,recipient,senderCharacter,recipientCharacter);
 if(result==TradeResult::SUCCESS){assignSelectedCharacter(sender,senderCharacter);assignSelectedCharacter(recipient,recipientCharacter);
  char text[96];std::snprintf(text,sizeof(text),"%s accepted the trade.",social[recipient].name);messagePlayer(sender,text);messagePlayer(recipient,"Trade accepted.");}
 else if(result==TradeResult::INVENTORY_FULL){messagePlayer(sender,"Trade cancelled: inventory full.");messagePlayer(recipient,"Inventory full. Trade cancelled.");}
 else{messagePlayer(sender,"Trade cancelled: items changed.");messagePlayer(recipient,"Trade cancelled: items unavailable.");}
 selectController(recipient);return result;
}
bool cancelNativeTrade(){
 const PlayerID id=selectedController();const TradeOffer* pending=tradeOffers.pendingFor(id);
 if(!pending||pending->sender!=id)return false;const PlayerID recipient=pending->recipient;
 if(!tradeOffers.cancel(pending->id))return false;
 messagePlayer(recipient,"Trade offer cancelled.");messagePlayer(id,"Trade offer cancelled.");selectController(id);return true;
}
void openNativePartyMenu(){if(registry)build(selectedController(),Social::Board);}
bool cancelNativePartyMenu(){
 const auto id=selectedController();
 if(!registry || !isValidController(id) || currentMenu()!=&social[id].menu || social[id].pane==Social::Board)return false;
 if(social[id].pane==Social::TradeItems){buildTrade(id);return true;}
 if(social[id].pane==Social::TradeRecipients||social[id].pane==Social::TradeReview||social[id].pane==Social::TradeWaiting){closeMenu();return true;}
 build(id,Social::Board);return true;
}
bool activateNativePartyMenu(){
 const auto id=selectedController();if(!registry||!isValidController(id)||currentMenu()!=&social[id].menu)return false;
 auto& s=social[id];const auto index=menuState.cursorIndex;
 if(s.pane==Social::TradeRecipients){
  unsigned available=0;for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(peer!=id&&sameParty(id,peer)&&connectedPartyMember(peer))++available;
  if(index<available){std::memset(s.selectedTradeItems,0,sizeof(s.selectedTradeItems));buildTradeItems(id,s.tradeRecipientLookup[index]);}
  else setGameMessage("No party member is available.");
  return true;
 }
 if(s.pane==Social::TradeItems){
  Character character=selectedCharacter(id);const uint8_t count=character.inventory.itemCount;
  if(index<count){
   if(!s.selectedTradeItems[index]){unsigned selected=0;for(bool value:s.selectedTradeItems)selected+=value;if(selected>=MAX_TRADE_OFFER_ITEMS){setGameMessage("Offer can contain four item stacks.");return true;}}
   s.selectedTradeItems[index]=!s.selectedTradeItems[index];buildTradeItems(id,s.tradeRecipient);return true;
  }
  if(index==count){TradeOfferLine lines[MAX_TRADE_OFFER_ITEMS]{};uint8_t lineCount=0;
   for(uint8_t itemIndex=0;itemIndex<count&&lineCount<MAX_TRADE_OFFER_ITEMS;++itemIndex)if(s.selectedTradeItems[itemIndex]){
    lines[lineCount].item=character.inventory.slots[itemIndex].item;lines[lineCount].quantity=1;++lineCount;
   }
   if(!lineCount){setGameMessage("Select at least one item.");return true;}
   uint32_t offerID=0;TradeResult result=proposeNativeTrade(s.tradeRecipient,lines,lineCount,offerID);
   if(result==TradeResult::SUCCESS)buildTrade(id);
   else if(result==TradeResult::PLAYER_BUSY)setGameMessage("A player already has a pending trade.");
   else if(result==TradeResult::NOT_OWNED)setGameMessage("An offered item is no longer available.");
   else setGameMessage("Trade offer could not be sent.");
   return true;
  }
  closeMenu();return true;
 }
 if(s.pane==Social::TradeReview){respondToNativeTrade(index==0);closeMenu();return true;}
 if(s.pane==Social::TradeWaiting){cancelNativeTrade();closeMenu();return true;}
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
  for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer){PartySnapshot p;if(nativePartySnapshot(peer,p) && p.id.high==joined.id.high && p.id.low==joined.id.low){world=social[peer].world;break;}}
  if(world==NATIVE_WORLD_CAPACITY){registry->leave(membership.handle);build(id,Social::Board);setGameMessage("Party runtime unavailable.");return true;}
  s.member=true;s.party=party;s.membership=membership;s.world=world;partyLifecycle("JOINED_PARTY",id,joined);
  assignNativeWorld(id,world);selectNativeWorld(world);build(id,Social::Status);nativePartyMessage("An adventurer joined the party.");return true;
 }
 if(index==0){if(create(id))build(id,Social::Status);}
 else if(index==1){if(s.member)setGameMessage("Leave your current party first.");else{s.entered=0;s.code[0]=0;build(id,Social::Code);}}
 else if(index==2)build(id,Social::Status);
 else if(index==3){if(canOpenNativeTradeMenu())openNativeTradeMenu();else setGameMessage("Join another party member to trade.");}
 else if(index==4){if(leaveSocial(id))build(id,Social::Board);}
 return true;
}
void refreshNativePartyMenu(){
 const auto id=selectedController();if(!registry||!isValidController(id))return;
 auto& s=social[id];
 if(s.member){
  const auto location=gameState==GAME_FOREST?MultiplayerMemberLocation::FOREST:gameState==GAME_DUNGEON?MultiplayerMemberLocation::DUNGEON:MultiplayerMemberLocation::TOWN;
  registry->updateMember(s.membership.handle,profile(id),location);
 }
 if(!menuState.isOpen||currentMenu()!=&s.menu||s.pane==Social::Code)return;
 if(s.pane==Social::TradeRecipients||s.pane==Social::TradeItems||
    s.pane==Social::TradeReview||s.pane==Social::TradeWaiting){
  if((s.pane==Social::TradeReview||s.pane==Social::TradeWaiting)&&
     !tradeOffers.pendingFor(id))closeMenu();
  return;
 }
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
 for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(sameParty(owner,id) && selectController(id))setGameMessage(message.c_str());
 selectController(owner);
}
void nativePartyConnection(PlayerID id,bool connected){
 if(!registry||!isValidController(id)||!social[id].member)return;
 auto& s=social[id];
 if(connected){PartyMembership resumed;const auto error=registry->reconnect(s.party,s.membership.player,s.membership.reconnectToken,resumed);if(error==PartyError::Ok)s.membership=resumed;}
 else{tradeOffers.cancelForPlayer(id);registry->disconnect(s.membership.handle);}
}
bool retireNativePartyMember(PlayerID id){
 if(!registry||!social[id].member)return true;
 const auto owner=selectedController();
 bool peers[NATIVE_CONTROLLER_CAPACITY]{};const bool leader=social[id].membership.player==HOST_PLAYER_ID;
 for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)peers[peer]=peer==id || (leader && sameParty(id,peer));
 for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(peers[peer] && selectController(peer)){
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
 for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(sameParty(id,peer) && nativeActor(peer))return;
 registry->returnToTown(social[id].membership.handle);
}
bool canResetNativePartyAdventure(){
 if(!registry)return false;const auto id=selectedController();
 if(!isValidController(id)||!social[id].member||social[id].membership.player!=HOST_PLAYER_ID||!town()||combat.active)return false;
 for(PlayerID peer=0;peer<NATIVE_CONTROLLER_CAPACITY;++peer)if(sameParty(id,peer)&&nativeActor(peer))return false;
 return true;
}
bool resetNativePartyAdventure(){
 if(!canResetNativePartyAdventure()){setGameMessage("Only the leader can reset after all return.");return false;}
 resetNativeWorld();return true;
}
bool startNativePartyAdventure(bool forest){
 if(!registry||travelling)return false;
 const auto owner=selectedController();if(!isValidController(owner))return true;
 if(!town()){setGameMessage("Travel begins in Town.");return true;}
 if(!social[owner].member && !create(owner))return true;
 auto& s=social[owner];PartySnapshot party;
 if(!nativePartySnapshot(owner,party))return true;
 // An existing authenticated member may return with a loaded character while
 // peers keep exploring. This is not new party admission and never starts or
 // resets an activity. The initial party launch retains its original rules.
 if(party.activity!=PartyActivity::Town){
  const auto expected=forest?PartyActivity::Forest:PartyActivity::Dungeon;
  const auto local=s.membership.player;
  if(party.activity!=expected){setGameMessage("Resume the party's current adventure.");return true;}
  if(!isValidPlayerID(local)||!party.members[local].occupied||!party.members[local].connected||
     nativeActor(owner)||combat.active||pendingNativeTrade(owner)||player.health.currentHP<=0){
   setGameMessage("Wait for combat/trade. Rest in Town.");return true;
  }
  if((!forest&&(!dungeon.runActive||dungeon.completed))||(forest&&forestEntityCount==0)){
   setGameMessage("Current adventure is unavailable.");return true;
  }
  struct TravelScope {bool& flag;explicit TravelScope(bool& value):flag(value){flag=true;}~TravelScope(){flag=false;}} scope(travelling);
  closeMenu();closeTownHome();closeCharacterSheet();
  if(forest){if(!enterNativeForestController(false)){setGameMessage("No safe entry space.");return true;}}
  else enterDungeon(); // travelling bypasses only this social dispatch hook.
  refreshNativePartyMenu();return true;
 }
 if(s.membership.player!=HOST_PLAYER_ID){setGameMessage("Only the party leader starts travel.");return true;}
 bool members[NATIVE_CONTROLLER_CAPACITY]{};
 for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(sameParty(owner,id)){
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
 for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(members[id]){
  selectController(id);closeMenu();closeTownHome();closeCharacterSheet();
  if(forest){if(!enterNativeForestController(fresh))throw std::runtime_error("Party forest initialization failed");fresh=false;}
  else enterDungeon();
  refreshNativePartyMenu();
 }
 travelling=false;selectController(owner);nativePartyMessage(forest?"Party entered the forest.":"Party entered the dungeon.");return true;
}
}}
