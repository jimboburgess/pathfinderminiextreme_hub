#include "runtime.h"
#include "controller_slots.h"
#include "native_rooms.h"
#include "data/game.h"
#include "map/activemap.h"
#include "input/menu.h"
#include "graphics/charcreationscreen.h"
#include "characters/sheet.h"
#include "dungeon/combat.h"
#include "map/mapeffects.h"
#include "data/entityspawn.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <queue>
using namespace pfme::runtime;
namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
void wait(unsigned ms) {while(ms){auto step=std::min(ms,10u);advance(step);ms-=step;}}
void tap(unsigned id,const char* action) {
 check(controllerInput(id,action,true),"input down");wait(30);
 check(controllerInput(id,action,false),"input up");wait(310);
}
void face(unsigned id,Direction dir) {
 check(selectController(id),"select");
 for(unsigned i=0;moveDirection!=dir && i<8;++i)tap(id,"cw");
 check(moveDirection==dir,"independent facing");
}
uint64_t hash(unsigned id) {
 auto* p=controllerPixels(id);check(p,"pixels");uint64_t h=1469598103934665603ULL;
 for(unsigned i=0;i<57600;++i){h^=p[i];h*=1099511628211ULL;}return h;
}
void capture(unsigned id) {
 std::ofstream out("controller-"+std::to_string(id)+".rgb565",std::ios::binary);
 auto* pixels=controllerPixels(id);
 for(unsigned i=0;i<57600;++i){out.put(char(pixels[i]));out.put(char(pixels[i]>>8));}
}
bool selectAction(unsigned id,MenuAction action) {
    selectController(id);
    if(!menuState.isOpen || !menuState.depth)return false;
    const auto* menu=menuState.menuStack[menuState.depth-1];int target=-1,index=0;
    for(unsigned i=0;i<menu->itemCount;++i) {
        const auto& item=menu->items[i];
        if(!(item.classMask & MENU_CLASS_FIGHTER) || !isMenuItemVisible(item.action))continue;
        if(item.action==action)target=index;
        ++index;
    }
    if(target<0 || !isMenuItemEnabled(action))return false;
    for(unsigned tries=0;menuState.cursorIndex!=target && tries<40;++tries)
        tap(id,menuState.cursorIndex<target?"cw":"ccw");
    check(menuState.cursorIndex==target,"menu cursor");tap(id,"a");return true;
}
bool takePathStep(unsigned id,bool travelToEncounter) {
    selectController(id);auto* actor=nativeActor(id);if(!actor)return false;
    const int start=actor->y*ROOM_WIDTH+actor->x;
    int parent[ROOM_WIDTH*ROOM_HEIGHT],direction[ROOM_WIDTH*ROOM_HEIGHT];
    std::fill(std::begin(parent),std::end(parent),-1);
    std::queue<int> pending;pending.push(start);parent[start]=start;int found=-1;
    while(!pending.empty() && found<0) {
        int cell=pending.front();pending.pop();int x=cell%ROOM_WIDTH,y=cell/ROOM_WIDTH;
        bool goal=false;
        if(travelToEncounter)goal=dungeon.rooms[dungeon.currentRoom].map.tiles[y][x]==TILE_DOOR && x==ROOM_WIDTH-1;
        else for(unsigned i=0;i<dungeon.entityCount;++i) {
            const auto& entity=dungeon.entities[i];
            if(entity.active && entity.type==ENTITY_MONSTER && entity.character.state==STATE_ALIVE &&
               std::max(abs(x-int(entity.x)),abs(y-int(entity.y)))<=1)goal=true;
        }
        if(goal && cell!=start){found=cell;break;}
        for(Direction d:{DIR_NORTH,DIR_EAST,DIR_SOUTH,DIR_WEST}) {
            int nx=x+directionOffsets[d].dx,ny=y+directionOffsets[d].dy;
            if(nx<0 || ny<0 || nx>=ROOM_WIDTH || ny>=ROOM_HEIGHT)continue;
            int next=ny*ROOM_WIDTH+nx;if(parent[next]>=0)continue;
            auto tile=dungeon.rooms[dungeon.currentRoom].map.tiles[ny][nx];
            if(tile!=TILE_FLOOR && tile!=TILE_RUBBLE && !(travelToEncounter && tile==TILE_DOOR && nx==ROOM_WIDTH-1))continue;
            auto* occupant=getEntityAt(dungeon.entities,dungeon.entityCount,nx,ny);
            if(occupant && occupant!=actor)continue;
            parent[next]=cell;direction[next]=d;pending.push(next);
        }
    }
    if(found<0)return false;
    while(parent[found]!=start)found=parent[found];
    const auto room=dungeon.currentRoom;const int x=actor->x,y=actor->y;
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
    unsigned initialHP=0;
    for(unsigned i=0;i<dungeon.entityCount;++i)if(dungeon.entities[i].type==ENTITY_MONSTER)
        initialHP+=std::max(0,dungeon.entities[i].character.health.currentHP);
    check(initialHP>0,"real generated encounter fixture");
    wait(18000);
    const auto explorer=selectedController();
    for(unsigned approach=0;!combat.active && approach<16;++approach) {
        takePathStep(explorer,false);wait(7000);
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
        if(nativeActorRoom(id)!=1) {
            for(unsigned step=0;step<12 && combat.active && getCurrentCombatant()==nativeActor(id) &&
                            combat.waitingForPlayer && nativeActor(id)->turn.movementRemaining;++step)
                if(!takePathStep(id,true))break;
        }
        entered[id]|=nativeActorRoom(id)==1;
        if(combat.active && getCurrentCombatant()==nativeActor(id) && combat.waitingForPlayer && nativeActorRoom(id)==1) {
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
    check(surviving>0 && totalXP>0 && activeAttacks>0,"production damage, rewards and surviving party");
    check(graceChecked,"disconnect turn policy exercised");
    if(count==1) {
        selectController(0);const auto xp=nativeActor(0)->character.xp;
        if(menuState.isOpen)tap(0,"b");tap(0,"a");
        check(selectAction(0,MENU_GAME) && selectAction(0,MENU_RETURN_TO_TOWN),"original return-to-town menus");
        check(gameState==GAME_TOWN && player.xp==xp && !nativeActor(0),"solo adventure loop retains earned character in town");
    }
    std::cout<<"PASS shared combat party="<<count<<" initial_monster_hp="<<initialHP<<" surviving="<<surviving<<" total_xp="<<totalXP<<" world_ms="<<worldTime()<<'\n';
}

}
int main(int argc,char** argv) {
 try {
  unsigned count=argc>1?std::stoul(argv[1]):2;check(count>=1&&count<=4,"party size");
  const uint32_t seed=argc>3?std::stoul(argv[3]):2;
  pfme::GameSession session;check(session.initialize(seed)==pfme::SessionError::Ok,"session");
  ControllerSlots slots(session);pfme::PlayerHandle handles[4];ControllerState* states[4]{};
  initializeControllers(seed);
  for(unsigned id=0;id<count;++id) {
   check(session.join(("Player"+std::to_string(id)).c_str(),handles[id])==pfme::SessionError::Ok,"join");
   check(slots.attach(handles[id],id+1,states[id])==ControllerError::Ok,"lease");
   check(bindController(id,*states[id]),"runtime bind");
   check(gameState==GAME_START,"independent original title");
   tap(id,"a");check(gameState==GAME_CHARACTER_CREATION,"original creation");
   // Combat fixtures use duplicate Fighters; the mixed fixture exercises all
   // original classes through the same rotary/press character-creation UI.
   const unsigned classIndex=argc>2 && std::string(argv[2])=="mixed"?id%4:0;
   for(unsigned rotation=0;rotation<classIndex;++rotation)tap(id,"cw");
   tap(id,"a");if(getCharacterCreationState()==CCS_WEAPON_STYLE_SELECT)tap(id,"a");
   check(getCharacterCreationState()==CCS_VIEW_STATS,"original rolled class preview");
   tap(id,"a");tap(id,"a");tap(id,"encoder");
   check(gameState==GAME_TOWN&&player.health.currentHP>0,"production character accepted");
   check(player.characterClass==classIndex && player.level==1 && player.xp==0 && player.inventory.gold==STARTING_GOLD,"full original character progression and inventory");
   check(player.magic.currentMP==player.magic.maxMP,"production MP retained");
  }
  for(unsigned id=0;id<count;++id) {
   selectController(id);check(gameState==GAME_TOWN,"retained town");
   tap(id,"cw");tap(id,"cw");tap(id,"a");
   if(menuState.isOpen)tap(id,"a");
   check(gameState==GAME_DUNGEON && dungeon.runActive,"shared dungeon entry");
   check(nativeActor(id)&&nativeActor(id)->ownerPlayerID==id,"production actor ownership");
   std::cout<<"entered id="<<id<<" x="<<int(nativeActor(id)->x)<<" y="<<int(nativeActor(id)->y)<<" hp="<<nativeActor(id)->character.health.currentHP<<'\n';
  }
  auto step=[&](unsigned id,Direction direction) {
    int x[4]{},y[4]{};for(unsigned j=0;j<count;++j){x[j]=nativeActor(j)->x;y[j]=nativeActor(j)->y;}
    face(id,direction);tap(id,"encoder");
    check(nativeActor(id)->x==x[id]+directionOffsets[direction].dx && nativeActor(id)->y==y[id]+directionOffsets[direction].dy,"independent production tile movement");
    for(unsigned j=0;j<count;++j)if(j!=id)check(nativeActor(j)->x==x[j] && nativeActor(j)->y==y[j],"input cannot move a different actor");
  };
  if(count==4) {step(3,DIR_EAST);step(0,DIR_EAST);step(3,DIR_EAST);step(0,DIR_EAST);step(1,DIR_SOUTH);step(1,DIR_EAST);step(2,DIR_EAST);}
  else {step(0,DIR_EAST);if(count>1)step(1,DIR_SOUTH);if(count>2){step(0,DIR_EAST);step(1,DIR_EAST);step(2,DIR_EAST);}}
  if(count==4) {
    uint64_t before[4]{};for(unsigned id=0;id<4;++id)before[id]=hash(id);
    const auto clock=worldTime();const auto events=session.latestSequence();pfme::PlayerHandle fifth;
    check(session.join("Fifth",fifth)==pfme::SessionError::Full,"fifth occupied slot rejected");
    check(worldTime()==clock && session.latestSequence()==events,"full rejection preserves clock and session events");
    for(unsigned id=0;id<4;++id)check(hash(id)==before[id],"full rejection preserves original views");
  }
  // Exercise actual menus while another controller remains in its own menu.
  tap(0,"a");check(menuState.isOpen,"first original map menu");
  if(count>1) {
   selectController(1);check(!menuState.isOpen,"second menu initially closed");
   tap(1,"a");tap(1,"cw");auto secondCursor=menuState.cursorIndex;
   selectController(0);check(menuState.isOpen&&menuState.cursorIndex==0,"first menu retained");
   tap(0,"b");selectController(1);check(menuState.isOpen&&menuState.cursorIndex==secondCursor,"second menu retained");tap(1,"b");
  } else tap(0,"b");
  // The authored entrance is a straight eastward corridor at y=6. Other
  // players receive nonoverlapping nearby spawn squares from production rules.
  const unsigned explorer=count==4?3:0;
  selectController(explorer);face(explorer,DIR_EAST);
  int otherX[4]{},otherY[4]{};for(unsigned i=0;i<count;++i)if(i!=explorer){otherX[i]=nativeActor(i)->x;otherY[i]=nativeActor(i)->y;}
  unsigned moved=0;
  while(dungeon.currentRoom==0 && moved<16) {
   auto before=nativeActor(explorer)->x;tap(explorer,"encoder");++moved;
   if(dungeon.currentRoom==0)check(nativeActor(explorer)->x==before+1,"one original tile per encoder click");
   for(unsigned i=0;i<count;++i)if(i!=explorer)check(nativeActor(i)->x==otherX[i]&&nativeActor(i)->y==otherY[i],"movement did not teleport another player");
  }
  check(dungeon.currentRoom==1,"production door transition");
  capture(explorer);auto first=hash(explorer);
  for(unsigned id=0;id<count;++id)if(id!=explorer) {
   selectController(id);check(dungeon.currentRoom==0,"other player remains in entrance");
   check(nativeActor(id)==getActiveMapPlayer(),"active player selected by owner");
   controllerInput(id,"release",true);capture(id);
   check(hash(id)!=first,"different room framebuffer");
   std::cout<<"independent id="<<id<<" room="<<int(dungeon.currentRoom)<<" frame="<<hash(id)<<'\n';
  }
  selectController(explorer);check(dungeon.currentRoom==1,"first room retained after view swaps");
  unsigned monsters=0;for(unsigned i=0;i<dungeon.entityCount;++i)if(dungeon.entities[i].type==ENTITY_MONSTER)++monsters;
  std::cout<<"room1_type="<<int(dungeon.rooms[1].type)<<" monsters="<<monsters<<" seed="<<seed<<'\n';
  std::cout<<"PASS production exploration party="<<count<<" room0_actor=0 room="<<int(dungeon.currentRoom)<<" frame="<<first<<" world_ms="<<worldTime()<<'\n';
  if(argc>2 && std::string(argv[2])=="combat")battle(count);
  if(argc>2 && std::string(argv[2])=="world-effects") {
    check(count==2 && nativeActorRoom(0)==1 && nativeActorRoom(1)==0,"two-room regression fixture");
    selectController(0);MapEffect fire{};fire.active=true;fire.type=MAP_EFFECT_WALL_OF_FIRE;
    fire.roundsRemaining=3;fire.tileCount=1;fire.tiles[0]={static_cast<int8_t>(nativeActor(1)->x),static_cast<int8_t>(nativeActor(1)->y)};
    fire.damageType=DAMAGE_FIRE;fire.flatDamage=3;check(addMapEffect(fire),"production ground effect fixture");
    const auto hp=nativeActor(1)->character.health.currentHP;
    combat=Combat{};combat.active=true;combat.phase=COMBAT_TURN;combat.waitingForPlayer=true;
    combat.combatantCount=2;combat.initiativeOrder[0]=nativeActor(0);combat.initiativeOrder[1]=nativeActor(1);
    tap(0,"b");
    check(nativeActor(1)->character.health.currentHP==hp,"turn-start hazard lookup uses the next actor's actual room");
    selectController(1);check(!getMapEffectAt(fire.tiles[0].x,fire.tiles[0].y),"room effects not copied into another room");
    MapEffect web{};web.active=true;web.type=MAP_EFFECT_WEB;web.roundsRemaining=3;web.tileCount=1;web.tiles[0]={3,6};
    check(addMapEffect(web),"second room effect fixture");
    combat=Combat{};clearMapEffects();
    check(!getMapEffectAt(3,6),"combat cleanup clears selected room");
    selectController(0);check(!getMapEffectAt(fire.tiles[0].x,fire.tiles[0].y),"combat cleanup clears retained other room");
    std::cout<<"PASS room-local hazard and shared cleanup regression\n";
  }
  if(argc>2 && std::string(argv[2])=="lifecycle") {
    const unsigned retiring=count-1;auto old=handles[retiring];
    check(unbindController(retiring),"retire production binding");
    check(slots.leave(old,retiring+1)==ControllerError::Ok,"retire verified lease");
    check(!nativeActor(retiring),"retired actor removed from shared room");
    pfme::PlayerHandle replacement;
    check(session.join("Replacement",replacement)==pfme::SessionError::Ok,"new reservation");
    ControllerState* fresh=nullptr;
    check(slots.attach(replacement,100,fresh)==ControllerError::Ok,"new lease");
    check(fresh->character.health.currentHP==0 && !fresh->menu.isOpen,"fresh character and UI");
    check(bindController(retiring,*fresh),"bind replacement controller");
    check(gameState==GAME_START && !nativeActor(retiring),"replacement starts at original title");
    pfme::SessionPlayer rejected;check(session.player(old,rejected)!=pfme::SessionError::Ok,"retired capability rejected");
    std::cout<<"PASS retirement/reuse party="<<count<<'\n';
  }
 } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<" "<<diagnostic()<<'\n';return 1;}
}
