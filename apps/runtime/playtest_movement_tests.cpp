#include "runtime.h"
#include "controller_slots.h"
#include "native_rooms.h"
#include "native_party.h"
#include "data/game.h"
#include "data/entityspawn.h"
#include "data/dice.h"
#include "dungeon/dungeon.h"
#include "dungeon/combat.h"
#include "dungeon/actioneconomy.h"
#include "dungeon/furniture.h"
#include "forest/forest.h"
#include "town/town.h"
#include "map/activemap.h"
#include "map/playermovement.h"
#include "map/movement.h"
#include "map/mapeffects.h"
#include "graphics/messagelog.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace pfme::runtime;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void wait(unsigned n){while(n){auto s=n<10?n:10;advance(s);n-=s;}}
void tap(unsigned id,const char* a){check(controllerInput(id,a,true),"input down");wait(30);check(controllerInput(id,a,false),"input up");wait(310);}
void choose(unsigned id,const char* title){
 selectController(id);check(menuState.isOpen&&menuState.depth,"menu open");auto* m=menuState.menuStack[menuState.depth-1];int wanted=-1,index=0;
 for(unsigned i=0;i<m->itemCount;++i)if(isMenuItemVisible(m->items[i].action)){if(std::string(m->items[i].title)==title)wanted=index;++index;}
 check(wanted>=0,"menu option");for(unsigned n=0;menuState.cursorIndex!=wanted&&n<40;++n)tap(id,menuState.cursorIndex<wanted?"cw":"ccw");check(menuState.cursorIndex==wanted,"menu cursor");tap(id,"a");
}
Entity* actors[4];bool forest=false;
void tile(int x,int y,TileType t){if(forest)setForestTileAuthoritative(x,y,t);else dungeon.rooms[0].map.tiles[y][x]=t;}
void invariant(){for(unsigned i=0;i<4;++i)for(unsigned j=i+1;j<4;++j)if(actors[i]->x==actors[j]->x&&actors[i]->y==actors[j]->y){std::cerr<<"overlap ids="<<i<<","<<j<<" xy="<<unsigned(actors[i]->x)<<","<<unsigned(actors[i]->y)<<" pointers="<<actors[i]<<","<<actors[j]<<"\n";check(false,"no final overlap");}}
void reset(bool battle=false){
 selectController(0);combat=Combat{};clearMapEffects();if(!forest)for(auto& trap:dungeon.rooms[0].traps)trap=TrapInstance{};
 for(int y=0;y<getActiveMapHeight();++y)for(int x=0;x<getActiveMapWidth();++x)tile(x,y,forest?TILE_TREE:TILE_WALL);
 for(int x=1;x<12;++x)tile(x,6,TILE_FLOOR);
 uint8_t n;auto* entities=getActiveMapEntities(n);for(unsigned i=0;i<n;++i)if(entities[i].type!=ENTITY_PLAYER)entities[i].active=false;
 for(unsigned i=0;i<4;++i){actors[i]=nativeActor(i);check(actors[i],"original full actor");actors[i]->x=3+i;actors[i]->y=6;actors[i]->character.team=TEAM_PLAYER;actors[i]->character.state=STATE_ALIVE;actors[i]->character.health.currentHP=actors[i]->character.health.maxHP=100;actors[i]->character.conditions=ConditionData{};actors[i]->turn=TurnState{};actors[i]->turn.movementRemaining=6;}
 if(battle){combat.active=true;combat.phase=COMBAT_TURN;combat.waitingForPlayer=true;combat.combatantCount=4;combat.currentTurnIndex=0;for(unsigned i=0;i<4;++i)combat.initiativeOrder[i]=actors[i];actors[0]->turn.turnActive=true;}
 moveDirection=DIR_EAST;invariant();
}
bool move(){bool changed=false;auto result=tryMovePlayerAuthoritative(dungeon,0,DIR_EAST,true,changed);invariant();return result;}
int main(int argc,char** argv){try{
 forest=argc>1&&std::string(argv[1])=="forest";initializeControllers(2,true);ControllerState states[4];
 for(unsigned i=0;i<4;++i){check(bindController(i,states[i]),"bind");for(unsigned j=0;j<5;++j)tap(i,"a");tap(i,"encoder");check(gameState==GAME_TOWN,"character creation");}
 selectController(0);townSelection=TOWN_BULLETIN_BOARD;tap(0,"a");choose(0,"Create Party");pfme::PartySnapshot party;check(nativePartySnapshot(0,party),"party created");tap(0,"b");tap(0,"b");
 for(unsigned id=1;id<4;++id){selectController(id);townSelection=TOWN_BULLETIN_BOARD;tap(id,"a");choose(id,"Join Party");for(unsigned j=0;j<pfme::PARTY_JOIN_CODE_LENGTH;++j){char c[2]={party.joinCode[j],0};choose(id,c);}tap(id,"b");tap(id,"b");}
 selectController(0);townSelection=forest?TOWN_FOREST:TOWN_DUNGEON;tap(0,"a");check(gameState==(forest?GAME_FOREST:GAME_DUNGEON),"original shared party world entry");
 unsigned scenarios=0;
 reset();actors[2]->y=7;actors[3]->y=7;check(move()&&actors[0]->x==5&&actors[1]->x==4,"single ally corridor");++scenarios;
 reset();check(move()&&actors[0]->x==7&&actors[1]->x==4&&actors[2]->x==5&&actors[3]->x==6,"multiple allies corridor");++scenarios;
 reset();tile(7,6,forest?TILE_TREE:TILE_WALL);check(!move()&&actors[0]->x==3,"no landing: cannot end on ally");++scenarios;
 reset();auto* enemy=spawnActiveMapEntity(ENTITY_MONSTER,7,6);check(enemy,"enemy fixture");enemy->character.state=STATE_ALIVE;enemy->character.team=TEAM_MONSTER;check(!move()&&actors[0]->x==3,"enemy beyond allies remains blocking");++scenarios;
 reset(true);actors[2]->y=7;actors[3]->y=7;check(move()&&actors[0]->turn.movementRemaining==4&&actors[0]->turn.moveActionUsed,"two ordinary squares cost two movement");++scenarios;
 reset(true);actors[0]->turn.movementRemaining=3;check(!move()&&actors[0]->x==3&&actors[0]->turn.movementRemaining==3,"insufficient movement cannot end on ally");++scenarios;
 reset(true);actors[2]->y=7;actors[3]->y=7;tile(5,6,TILE_RUBBLE);
 if(!forest){actors[0]->character.abilities.dexterity=1;unsigned seed=1;for(;seed<10000;++seed){randomSeed(seed);if(rollDie(20)==1)break;}randomSeed(seed);}
 check(move()&&actors[0]->turn.movementRemaining==3,"difficult landing charges both steps");++scenarios;
 reset(true);actors[0]->x=2;check(move()&&actors[0]->x==3,"partial first step");check(move()&&actors[0]->turn.movementRemaining==1,"pass-through preserves partial cost");check(canUseDoubleMove(actors[0]->turn),"Double Move remains available");const auto speed=getEffectiveSpeed(actors[0]->character);beginDoubleMove();check(actors[0]->turn.standardActionUsed&&actors[0]->turn.movementRemaining==1+speed,"Double Move adds full allowance after pass-through");check(move()&&actors[0]->turn.movementRemaining==speed,"movement continues after Double Move");++scenarios;
 reset(true);bool changed=false;check(!tryMovePlayerAuthoritative(dungeon,1,DIR_EAST,true,changed)&&actors[1]->x==4,"non-acting owner rejected");invariant();++scenarios;
 reset();MapEffect effect;effect.active=true;effect.roundsRemaining=5;effect.type=MAP_EFFECT_WEB;effect.conditionType=CONDITION_GRAPPLED;effect.tileCount=1;effect.tiles[0]={4,6};check(addMapEffect(effect),"web fixture");check(!move()&&actors[0]->x==3,"unsafe intermediate hazard cannot strand overlapping actor");++scenarios;
 reset();actors[2]->y=7;actors[3]->y=7;tap(0,"encoder");check(actors[0]->x==5&&actors[1]->x==4,"actual handheld encoder input passes ally");invariant();++scenarios;
 if(!forest){reset(true);for(unsigned i=1;i<4;++i)actors[i]->y=7;actors[0]->turn.movementRemaining=0;actors[0]->character.abilities.strength=40;check(addDungeonFurniture(dungeon.rooms[0],FURNITURE_CRATE,4,6),"crate fixture");check(!move()&&dungeon.rooms[0].map.tiles[6][4]==TILE_CRATE,"no free crate push with no movement");++scenarios;}
 if(!forest){reset();auto& trap=dungeon.rooms[0].traps[0];trap.id=TRAP_SPIKE_PLATE;trap.x=4;trap.y=6;trap.hp=10;check(!move()&&actors[0]->x==3,"active intermediate trap is not skipped");++scenarios;trap.disabled=true;check(move()&&actors[0]->x==7,"disarmed intermediate trap no longer blocks allied passage");++scenarios;}
 if(!forest){
  reset(true);check(dungeon.roomCount>1,"real generated destination");
  dungeon.rooms[0].east=1;dungeon.rooms[1].west=0;
  tile(ROOM_WIDTH-1,6,TILE_DOOR);
  for(int x=1;x<ROOM_WIDTH-1;++x)tile(x,6,TILE_FLOOR);
  for(unsigned i=0;i<4;++i){actors[i]->x=ROOM_WIDTH-5+i;actors[i]->y=6;}
  bool changed=false;
  check(tryMovePlayerAuthoritative(dungeon,0,DIR_EAST,true,changed)&&changed,"crowded entrance traverses allies and actual room transition");
  check(nativeActorRoom(0)==1&&nativeActor(0)->turn.movementRemaining==2,"door plus three allied squares cost four, new actor pointer owns remaining movement");
  for(unsigned i=1;i<4;++i)check(nativeActorRoom(i)==0&&nativeActor(i)->x==ROOM_WIDTH-5+i,"room travel never relocates waiting allies");
  check(getCurrentCombatant()==nativeActor(0),"initiative retargeted to transported actor");++scenarios;
 }
 std::cout<<"PASS production "<<(forest?"forest":"dungeon")<<" movement scenarios="<<scenarios<<" original controllers and frames, no final overlap\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
