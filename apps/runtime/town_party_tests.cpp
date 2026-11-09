#include "runtime.h"
#include "controller_slots.h"
#include "native_party.h"
#include "native_rooms.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "graphics/charcreationscreen.h"
#include "characters/sheet.h"
#include "graphics/messagelog.h"
#include "map/activemap.h"
#include "data/entityspawn.h"
#include "town/town.h"
#include "forest/forest.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <queue>
using namespace pfme::runtime;
using pfme::PartySnapshot;
namespace {
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void wait(unsigned ms){while(ms){auto step=std::min(ms,10u);advance(step);ms-=step;}}
void tap(unsigned id,const char* action){check(controllerInput(id,action,true),"down");wait(30);check(controllerInput(id,action,false),"up");wait(310);}
void choose(unsigned id,const char* title){
 check(selectController(id),"select");check(menuState.isOpen && menuState.depth,"menu open");
 auto* menu=menuState.menuStack[menuState.depth-1];int wanted=-1,index=0;
 for(unsigned i=0;i<menu->itemCount;++i)if(isMenuItemVisible(menu->items[i].action)){
  if(std::string(menu->items[i].title)==title)wanted=index;++index;
 }
 check(wanted>=0,"menu option present");
 for(unsigned n=0;menuState.cursorIndex!=wanted && n<40;++n)tap(id,menuState.cursorIndex<wanted?"cw":"ccw");
 check(menuState.cursorIndex==wanted,"correct menu selection");tap(id,"a");
}
void board(unsigned id){
 selectController(id);check(gameState==GAME_TOWN,"board only in Town");
 if(menuState.isOpen)tap(id,"b");
 for(unsigned n=0;townSelection!=TOWN_BULLETIN_BOARD && n<5;++n)tap(id,"cw");
 tap(id,"a");
}
void join(unsigned id,const char* code){board(id);choose(id,"Join Party");for(unsigned i=0;i<pfme::PARTY_JOIN_CODE_LENGTH;++i){char letter[2]={code[i],0};choose(id,letter);}PartySnapshot s;check(nativePartySnapshot(id,s),"joined through code menu");tap(id,"b");check(menuState.isOpen && std::string(menuState.menuStack[0]->title)=="Bulletin Board","Back restores Bulletin Board");tap(id,"b");}
bool selectAction(unsigned id,MenuAction action) {
    selectController(id);
    if(!menuState.isOpen || !menuState.depth)return false;
    uint16_t classMask=MENU_CLASS_FIGHTER;
    if(auto* actor=nativeActor(id)) {
        if(actor->character.characterClass==CLASS_ROGUE)classMask=MENU_CLASS_ROGUE;
        else if(actor->character.characterClass==CLASS_WIZARD)classMask=MENU_CLASS_WIZARD;
        else if(actor->character.characterClass==CLASS_CLERIC)classMask=MENU_CLASS_CLERIC;
    }
    const auto* menu=menuState.menuStack[menuState.depth-1];int target=-1,index=0;
    for(unsigned i=0;i<menu->itemCount;++i) {
        const auto& item=menu->items[i];
        if(!(item.classMask & classMask) || !isMenuItemVisible(item.action))continue;
        if(item.action==action)target=index;
        ++index;
    }
    if(target<0 || !isMenuItemEnabled(action))return false;
    for(unsigned tries=0;menuState.cursorIndex!=target && tries<40;++tries)
        tap(id,menuState.cursorIndex<target?"cw":"ccw");
    check(menuState.cursorIndex==target,"menu cursor");tap(id,"a");return true;
}

void face(unsigned id,Direction direction){selectController(id);for(unsigned n=0;moveDirection!=direction&&n<8;++n)tap(id,"cw");check(moveDirection==direction,"face");}
bool takePathStep(unsigned id,bool travelToEncounter) {
    selectController(id);auto* actor=nativeActor(id);if(!actor)return false;
    uint8_t entityCount=0;auto* entities=getActiveMapEntities(entityCount);
    const int start=actor->y*ROOM_WIDTH+actor->x;
    int parent[ROOM_WIDTH*ROOM_HEIGHT],direction[ROOM_WIDTH*ROOM_HEIGHT];
    std::fill(std::begin(parent),std::end(parent),-1);
    std::queue<int> pending;pending.push(start);parent[start]=start;int found=-1;
    while(!pending.empty() && found<0) {
        int cell=pending.front();pending.pop();int x=cell%ROOM_WIDTH,y=cell/ROOM_WIDTH;
        bool goal=false;
        if(travelToEncounter)goal=getActiveMapTile(x,y)==TILE_DOOR && x==ROOM_WIDTH-1;
        else for(unsigned i=0;i<entityCount;++i) {
            const auto& entity=entities[i];
            if(entity.active && entity.type==ENTITY_MONSTER && entity.character.state==STATE_ALIVE &&
               std::max(std::max(0,std::max(int(entity.x)-x,x-(int(entity.x)+getEntityTileWidth(entity)-1))),
                        std::max(0,std::max(int(entity.y)-y,y-(int(entity.y)+getEntityTileHeight(entity)-1))))<=1)goal=true;
        }
        if(goal && cell!=start){found=cell;break;}
        for(Direction d:{DIR_NORTH,DIR_EAST,DIR_SOUTH,DIR_WEST}) {
            int nx=x+directionOffsets[d].dx,ny=y+directionOffsets[d].dy;
            if(nx<0 || ny<0 || nx>=ROOM_WIDTH || ny>=ROOM_HEIGHT)continue;
            int next=ny*ROOM_WIDTH+nx;if(parent[next]>=0)continue;
            auto tile=getActiveMapTile(nx,ny);
            if(tile!=TILE_FLOOR && tile!=TILE_GRASS && tile!=TILE_RUBBLE && !(travelToEncounter && tile==TILE_DOOR && nx==ROOM_WIDTH-1))continue;
            auto* occupant=getEntityAt(entities,entityCount,nx,ny);
            if(occupant && occupant!=actor)continue;
            parent[next]=cell;direction[next]=d;pending.push(next);
        }
    }
    if(found<0)return false;
    while(parent[found]!=start)found=parent[found];
    const auto room=nativeActorRoom(id);const int x=actor->x,y=actor->y;
    face(id,static_cast<Direction>(direction[found]));tap(id,"encoder");
    return nativeActorRoom(id)!=room || nativeActor(id)->x!=x || nativeActor(id)->y!=y;
}

bool attack(unsigned id) {
    selectController(id);
    if(menuState.isOpen)tap(id,"b");
    tap(id,"a");
    if(!selectAction(id,MENU_ATTACK)) {if(menuState.isOpen)tap(id,"b");return false;}
    if(!selectAction(id,MENU_RANGED_ATTACK) || !isPlayerTargetingAttack()) {if(menuState.isOpen)tap(id,"b");return false;}
    if(!getSelectedAttackTarget()) {
        tap(id,"b");if(menuState.isOpen)tap(id,"b");return false;
    }
    tap(id,"a");return true;
}
void battle(unsigned count) {
    const bool inForest=gameState==GAME_FOREST;
    uint8_t entityCount=0;auto* entities=getActiveMapEntities(entityCount);
    unsigned initialHP=0;
    for(unsigned i=0;i<entityCount;++i)if(entities[i].type==ENTITY_MONSTER)
        initialHP+=std::max(0,entities[i].character.health.currentHP);
    check(initialHP>0,"real generated encounter fixture");
    wait(18000);
    const auto explorer=selectedController();
    for(unsigned approach=0;!combat.active && approach<(inForest?64u:16u);++approach) {
        takePathStep(explorer,false);wait(7000);
    }
    if(!combat.active && inForest){
        std::cout<<"FOREST DIAGNOSTIC selected="<<unsigned(selectedController())<<" room="<<unsigned(nativeActorRoom(selectedController()))<<" entities="<<unsigned(forestEntityCount)<<'\n';
        for(unsigned y=0;y<FOREST_HEIGHT;++y){for(unsigned x=0;x<FOREST_WIDTH;++x){auto* e=getEntityAt(forestEntities,forestEntityCount,x,y);std::cout<<(e?(e->type==ENTITY_PLAYER?'p':'m'):(getForestTile(x,y)==TILE_TREE?'#':'.'));}std::cout<<'\n';}
        for(unsigned i=0;i<forestEntityCount;++i){const auto& e=forestEntities[i];std::cout<<i<<" type="<<int(e.type)<<" active="<<e.active<<" hp="<<e.character.health.currentHP<<" state="<<int(e.character.state)<<" aware="<<e.awareOfPlayer<<" x="<<int(e.x)<<" y="<<int(e.y)<<'\n';}
    }
    check(combat.active,"production awareness starts shared combat");
    unsigned rosterPlayers=0;for(unsigned i=0;i<combat.combatantCount;++i)
        if(combat.initiativeOrder[i]->type==ENTITY_PLAYER)++rosterPlayers;
    check(rosterPlayers==count,"all actual characters share initiative");
    bool attacked[4]{},entered[4]{},ownershipChecked=false,graceChecked=count==1;
    for(unsigned pass=0;pass<6000 && combat.active;++pass) {
        wait(100);auto* current=getCurrentCombatant();
        if(!current || current->type!=ENTITY_PLAYER || !combat.waitingForPlayer || combat.phase!=COMBAT_TURN ||
           combat.attackResolutionPending || combat.abilityResolutionPending || combat.turnStartConditionPhase!=TURN_START_CONDITION_NONE)continue;
        const auto id=current->ownerPlayerID;selectController(id);
        if(count>1 && !ownershipChecked) {
            const auto other=static_cast<PlayerID>((id+1)%count);
            auto* owner=nativeActor(id);const auto turn=combat.currentTurnIndex;
            int x[4]{},y[4]{};for(unsigned j=0;j<count;++j){x[j]=nativeActor(j)->x;y[j]=nativeActor(j)->y;}
            tap(other,"encoder");tap(other,"b");
            check(getCurrentCombatant()==owner && combat.currentTurnIndex==turn,"other controller cannot consume current turn");
            for(unsigned j=0;j<count;++j)check(nativeActor(j)->x==x[j] && nativeActor(j)->y==y[j],"out-of-turn input cannot move a character");
            ownershipChecked=true;selectController(id);
        }
        if(count>1 && !graceChecked) {
            auto* retained=nativeActor(id);const auto gold=retained->character.inventory.gold;
            setControllerConnected(id,false);wait(16000);
            check(nativeActor(id)==retained && retained->character.inventory.gold==gold,"disconnect retains actor and inventory");
            check(getCurrentCombatant()!=retained || !combat.active,"disconnected turn yielded after bounded grace");
            setControllerConnected(id,true);graceChecked=true;continue;
        }
        if(!inForest && nativeActorRoom(id)!=1) {
            for(unsigned step=0;step<12 && combat.active && getCurrentCombatant()==nativeActor(id) &&
                            combat.waitingForPlayer && nativeActor(id)->turn.movementRemaining;++step)
                if(!takePathStep(id,true))break;
        }
        entered[id]|=nativeActorRoom(id)==(inForest?NATIVE_FOREST_ROOM:1);
        if(combat.active && getCurrentCombatant()==nativeActor(id) && combat.waitingForPlayer && nativeActorRoom(id)==(inForest?NATIVE_FOREST_ROOM:1)) {
            if(!attack(id)) {
                for(unsigned step=0;step<12 && combat.active && getCurrentCombatant()==nativeActor(id) &&
                                combat.waitingForPlayer && nativeActor(id)->turn.movementRemaining;++step)
                    if(!takePathStep(id,false))break;
                if(combat.active && getCurrentCombatant()==nativeActor(id) && combat.waitingForPlayer)attacked[id]|=attack(id);
            } else attacked[id]=true;
        }
        for(unsigned pause=0;pause<200 && combat.active && (combat.attackResolutionPending || combat.abilityResolutionPending);++pause)wait(100);
        if(combat.active && getCurrentCombatant()==nativeActor(id) && combat.waitingForPlayer) {
            selectController(id);if(menuState.isOpen)tap(id,"b");tap(id,"b");
        }
    }
    check(!combat.active,"completed shared encounter");unsigned surviving=0,activeAttacks=0,totalXP=0;
    for(unsigned id=0;id<count;++id) {
        const auto* actor=nativeActor(id);check(actor,"actor retained after encounter");
        surviving+=actor->character.state==STATE_ALIVE;activeAttacks+=attacked[id];totalXP+=actor->character.xp;
        std::cout<<"combat_player="<<id<<" room="<<int(nativeActorRoom(id))<<" hp="<<actor->character.health.currentHP<<" xp="<<actor->character.xp<<" attacked="<<attacked[id]<<" entered="<<entered[id]<<'\n';
    }
    if(inForest)check(activeAttacks>0,"original Forest attacks resolved");
    else check(surviving>0 && totalXP>0 && activeAttacks>0,"production damage, rewards and surviving party");
    check(graceChecked,"disconnect turn policy exercised");
    if(count==1) {
        selectController(0);const auto xp=nativeActor(0)->character.xp;
        if(menuState.isOpen)tap(0,"b");tap(0,"a");
        check(selectAction(0,MENU_GAME) && selectAction(0,MENU_RETURN_TO_TOWN),"original return-to-town menus");
        check(gameState==GAME_TOWN && player.xp==xp && !nativeActor(0),"solo adventure loop retains earned character in town");
    }
    std::cout<<"PASS shared combat party="<<count<<" initial_monster_hp="<<initialHP<<" surviving="<<surviving<<" total_xp="<<totalXP<<" world_ms="<<worldTime()<<'\n';
}

void forestInitiative(unsigned count){
 const auto explorer=count-1;selectController(explorer);
 for(unsigned n=0;!combat.active && n<60;++n){
  if(!takePathStep(explorer,false))wait(2000);else wait(3500);
 }
 check(combat.active,"original forest awareness starts combat");
 unsigned players=0;for(unsigned i=0;i<combat.combatantCount;++i)players+=combat.initiativeOrder[i]->type==ENTITY_PLAYER;
 check(players==count,"one shared forest initiative contains all party members");
 for(unsigned n=0;n<100 && combat.active;++n){
  auto* current=getCurrentCombatant();
  if(current && current->type==ENTITY_PLAYER && combat.waitingForPlayer && combat.phase==COMBAT_TURN){
   const auto owner=current->ownerPlayerID;const auto other=static_cast<PlayerID>((owner+1)%count);
   int x[4]{},y[4]{};for(unsigned id=0;id<count;++id){auto* actor=nativeActor(id);check(actor,"forest actor remains");x[id]=actor->x;y[id]=actor->y;}
   selectController(owner);const auto message=std::string(getGameMessage());
   for(unsigned id=0;id<count;++id){selectController(id);check(std::string(getGameMessage())==message,"shared forest combat message");}
   tap(other,"encoder");
   check(getCurrentCombatant()==current,"non-active forest player cannot take turn");
   for(unsigned id=0;id<count;++id){auto* actor=nativeActor(id);check(actor->x==x[id] && actor->y==y[id],"forest initiative never teleports other players");}
   std::cout<<"PASS forest shared initiative party="<<count<<" message="<<message<<'\n';return;
  }
  wait(100);
 }
 throw std::runtime_error("forest player turn not reached");
}
void capture(unsigned id,const std::string& name){auto* pixels=controllerPixels(id);std::ofstream out(name,std::ios::binary);for(unsigned i=0;i<57600;++i){out.put(char(pixels[i]));out.put(char(pixels[i]>>8));}}
}
int main(int argc,char** argv){try{
 const unsigned count=argc>1?std::stoul(argv[1]):2;const bool forest=argc>2 && (std::string(argv[2])=="forest" || std::string(argv[2])=="forest-initiative" || std::string(argv[2])=="forest-combat");
 const bool forestCombat=argc>2 && std::string(argv[2])=="forest-initiative";
 const bool combatTest=argc>2 && (std::string(argv[2])=="combat" || std::string(argv[2])=="forest-combat");
 const bool separate=argc>2 && std::string(argv[2])=="separate";
 check(count>=1 && count<=4,"party count");initializeControllers(2,true);
 ControllerState states[4];Character original[4];
 for(unsigned id=0;id<count;++id){check(bindController(id,states[id]),"bind");nativePartyCharacterName(id,("Tester"+std::to_string(id)).c_str());
  tap(id,"a");for(unsigned n=0;n<(combatTest?0:id%4);++n)tap(id,"cw");tap(id,"a");if(getCharacterCreationState()==CCS_WEAPON_STYLE_SELECT)tap(id,"a");tap(id,"a");tap(id,"a");tap(id,"encoder");
  check(gameState==GAME_TOWN,"original character creation");original[id]=player;
 }
 if(!combatTest){
 selectController(0);check(townSelection==TOWN_STAY_HOME,"home selection");tap(0,"a");
 check(isTownHomeOpen(),"Stay Home opens original menu");tap(0,"a");wait(6500);check(!isTownRestActive(),"original rest completes");
 tap(0,"cw");tap(0,"a");check(menuState.isOpen && std::string(menuState.menuStack[0]->title)=="Save Game","save opens three-slot selector");tap(0,"b");
 tap(0,"cw");tap(0,"a");check(menuState.isOpen && std::string(menuState.menuStack[0]->title)=="Load Character","Home Load opens character selector");tap(0,"b");
   tap(0,"cw");tap(0,"a");check(isCharacterSheetVisible(),"home character sheet");tap(0,"b");
 tap(0,"b");check(!isTownHomeOpen(),"home cancel");
 for(unsigned n=0;townSelection!=TOWN_SHOP&&n<5;++n)tap(0,"cw");tap(0,"a");choose(0,"Buy");
 const auto goldBefore=player.inventory.gold;tap(0,"a");check(player.inventory.gold<goldBefore,"production shop purchase");tap(0,"b");
 choose(0,"Sell");const auto goldBought=player.inventory.gold;tap(0,"a");check(player.inventory.gold>goldBought,"production shop sale");tap(0,"b");choose(0,"Leave");
 original[0]=player;
 }
 board(0);choose(0,"Join Party");tap(0,"b");check(menuState.isOpen && std::string(menuState.menuStack[0]->title)=="Bulletin Board","code cancellation returns to board");choose(0,"Create Party");PartySnapshot owner;check(nativePartySnapshot(0,owner)&&owner.memberCount==1,"party created");tap(0,"b");check(menuState.isOpen && std::string(menuState.menuStack[0]->title)=="Bulletin Board","status Back returns to board");tap(0,"b");
 for(unsigned id=1;id<count;++id){if(separate){board(id);choose(id,"Create Party");tap(id,"b");tap(id,"b");}else join(id,owner.joinCode);}
 if(!separate){check(nativePartySnapshot(0,owner)&&owner.memberCount==count,"all members");
  if(count>1){setControllerConnected(1,false);check(nativePartySnapshot(0,owner)&&!owner.members[1].connected,"disconnect retained member");setControllerConnected(1,true);check(nativePartySnapshot(0,owner)&&owner.members[1].connected,"secure adapter reconnect");}
 }
 selectController(0);for(unsigned n=0;townSelection!=(forest?TOWN_FOREST:TOWN_DUNGEON)&&n<5;++n)tap(0,"cw");tap(0,"a");
 for(unsigned id=0;id<count;++id){selectController(id);
  if(separate && id){check(gameState==GAME_TOWN && !nativeActor(id),"other party stays in Town");continue;}
  check(gameState==(forest?GAME_FOREST:GAME_DUNGEON),"leader shared adventure transition");
  auto* actor=nativeActor(id);check(actor && actor->ownerPlayerID==id,"independent production actor");
  check(actor->character.characterClass==original[id].characterClass && actor->character.health.maxHP==original[id].health.maxHP && actor->character.inventory.gold==original[id].inventory.gold && actor->character.magic.maxMP==original[id].magic.maxMP,"full character retained");
  check(nativeActorRoom(id)==(forest?NATIVE_FOREST_ROOM:0),"entrance");capture(id,"town-party-"+std::to_string(id)+".rgb565");
 }
 if(separate){selectController(1);for(unsigned n=0;townSelection!=TOWN_DUNGEON&&n<5;++n)tap(1,"cw");tap(1,"a");check(nativeActor(1)&&!nativeActor(0),"other world has no first-party actors");auto* second=nativeActor(1);selectController(0);check(nativeActor(0)&&!nativeActor(1),"first world contains no second-party actor");check(nativeActor(0)!=second,"resident ownership isolated");
 for(unsigned n=0;nativeActorRoom(0)==0 && n<60;++n)check(takePathStep(0,true),"independent party corridor step");
 check(dungeon.rooms[1].discovered,"first party actually discovers next room");
 selectController(1);check(!dungeon.rooms[1].discovered && nativeActorRoom(1)==0 && nativeActor(1)==second && !combat.active,"other party world and initiative unchanged");
 }
 else{
  selectController(count-1);auto* explorer=nativeActor(count-1);const int x=explorer->x,y=explorer->y;
  if(forest){
   for(Direction direction:{DIR_EAST,DIR_SOUTH,DIR_NORTH,DIR_WEST}){
    const int nx=explorer->x+directionOffsets[direction].dx,ny=explorer->y+directionOffsets[direction].dy;
    if(getActiveMapTile(nx,ny)!=TILE_GRASS || getEntityAt(forestEntities,forestEntityCount,nx,ny))continue;
    face(count-1,direction);tap(count-1,"encoder");break;
   }
   check(explorer->x!=x || explorer->y!=y,"real independent forest movement");
  }
  else{
   for(unsigned n=0;nativeActorRoom(count-1)==0&&n<60;++n)check(takePathStep(count-1,true),"production corridor path step");
   check(nativeActorRoom(count-1)!=0,"original dungeon room transition");
   if(count>1)check(nativeActorRoom(0)==0,"other player was not teleported");
  }
  if(forestCombat)forestInitiative(count);
  if(combatTest){selectController(count-1);battle(count);}
  if(count>1){selectController(0);auto* actor=nativeActor(0);auto saved=*actor;setControllerConnected(0,false);setControllerConnected(0,true);check(nativeActor(0)==actor && actor->x==saved.x&&actor->y==saved.y,"reconnect retains actual character and position");}
 }
 if(!combat.active && !separate){
  for(unsigned id=0;id<count;++id){selectController(id);if(gameState==GAME_TOWN)continue;if(menuState.isOpen)tap(id,"b");
   // The original A action loots a facing corpse before opening the menu.
   auto* actor=nativeActor(id);uint8_t countHere=0;auto* entitiesHere=getActiveMapEntities(countHere);
   for(unsigned direction=0;direction<8;++direction){const auto d=static_cast<Direction>(direction);
    if(!getEntityAt(entitiesHere,countHere,actor->x+directionOffsets[d].dx,actor->y+directionOffsets[d].dy)){face(id,d);break;}}
   tap(id,"a");
   check(selectAction(id,MENU_GAME)&&selectAction(id,MENU_RETURN_TO_TOWN),"original return-to-Town menu");
   check(gameState==GAME_TOWN && !nativeActor(id),"own character returned to Town");
   if(id==0&&count>1&&!combatTest&&!forestCombat){
    Entity* others[4]{};int xs[4]{},ys[4]{};
    for(unsigned peer=1;peer<count;++peer){others[peer]=nativeActor(peer);check(others[peer],"remaining explorer");xs[peer]=others[peer]->x;ys[peer]=others[peer]->y;}
    townSelection=forest?TOWN_FOREST:TOWN_DUNGEON;tap(id,"a");if(!forest)choose(id,"Resume Dungeon");
    check(gameState==(forest?GAME_FOREST:GAME_DUNGEON)&&nativeActor(id),"returning member independently resumes current adventure");
    for(unsigned peer=1;peer<count;++peer)check(nativeActor(peer)==others[peer]&&others[peer]->x==xs[peer]&&others[peer]->y==ys[peer],"resume does not teleport exploring peers");
    tap(id,"a");check(selectAction(id,MENU_GAME)&&selectAction(id,MENU_RETURN_TO_TOWN),"resumed character returns independently");
   }
  }
  PartySnapshot returned;check(nativePartySnapshot(0,returned)&&returned.activity==pfme::PartyActivity::Town&&!returned.locked,"party returned, unlocked after every member");
  if(!forest && count==2){
   // Entry policy for a retained completed run (the same flag written by the
   // original final-encounter/Town-return rules), without replaying a whole campaign.
   selectController(0);dungeon.completed=true;
   for(unsigned n=0;townSelection!=TOWN_DUNGEON&&n<5;++n)tap(0,"cw");tap(0,"a");
   check(!dungeon.completed && !dungeon.rooms[1].discovered,"completed run starts a fresh authoritative dungeon");
   for(unsigned id=0;id<count;++id){selectController(id);check(gameState==GAME_DUNGEON && nativeActor(id),"fresh shared entrance after completed run");tap(id,"a");check(selectAction(id,MENU_GAME)&&selectAction(id,MENU_RETURN_TO_TOWN),"return from fresh adventure");}
  }
  if(count>1){board(count-1);choose(count-1,"Leave Party");check(!nativePartySnapshot(count-1,returned),"explicit leave releases membership");check(nativeControllerWorld(count-1)==count-1,"departed member has a private Town world");check(!dungeon.runActive,"no closed-party Resume option remains");tap(count-1,"b");check(nativePartySnapshot(0,returned)&&returned.memberCount==count-1,"remaining membership intact");}
  board(0);choose(0,"Leave Party");check(!nativePartySnapshot(0,returned),"leader closes party");
  for(unsigned id=1;id<count;++id)check(!nativePartySnapshot(id,returned),"closed member has no stale party capability");
 }
 std::cout<<"Town menu party size="<<count<<" adventure="<<(forest?"forest":separate?"separate parties":"dungeon")<<" production movement/ownership verified\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" "<<diagnostic()<<'\n';return 1;}}
