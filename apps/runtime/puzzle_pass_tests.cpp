#include "runtime.h"
#include "controller_slots.h"
#include "native_rooms.h"
#include "data/game.h"
#include "data/entityspawn.h"
#include "data/dice.h"
#include "dungeon/dungeon.h"
#include "dungeon/roomgen.h"
#include "dungeon/roomdraw.h"
#include "dungeon/puzzleroom.h"
#include "dungeon/riddlepuzzle.h"
#include "dungeon/bellpuzzle.h"
#include "dungeon/numbertilepuzzle.h"
#include "dungeon/brazierpuzzle.h"
#include "dungeon/combat.h"
#include "dungeon/furniture.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "map/interaction.h"
#include "map/activemap.h"
#include "map/playermovement.h"
#include "input/menu.h"
#include "town/town.h"
#include "audio/audio.h"
#include <Arduino.h>
#include <algorithm>
#include <array>
#include <queue>
#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace pfme::runtime;
extern Adafruit_ST7789 tft;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void wait(unsigned ms){while(ms){unsigned step=std::min(ms,10u);advance(step);ms-=step;}}
void roomGeometry(DungeonRoom& room){
 room= DungeonRoom{};room.type=ROOM_PUZZLE;
 for(int y=0;y<ROOM_HEIGHT;++y)for(int x=0;x<ROOM_WIDTH;++x)room.map.tiles[y][x]=(x==0||y==0||x==ROOM_WIDTH-1||y==ROOM_HEIGHT-1)?TILE_WALL:TILE_FLOOR;
 room.connectionCount=2;room.connections[0].direction=DIR_WEST;room.connections[0].x=0;room.connections[0].y=6;
 room.connections[1].direction=DIR_EAST;room.connections[1].x=ROOM_WIDTH-1;room.connections[1].y=6;
 room.map.tiles[6][0]=room.map.tiles[6][ROOM_WIDTH-1]=TILE_DOOR;
}
void configure(DungeonRoom& room,DungeonPuzzleType type,unsigned level=1){
 roomGeometry(room);room.puzzleBypassed=true;uint8_t rolls[128];for(unsigned i=0;i<128;++i)rolls[i]=(i*37+11)%256;
 if(type==PUZZLE_RIDDLEMAN){uint8_t shuffle[3]{};check(configureRiddlemanPuzzleRoom(room,DIR_EAST,static_cast<RiddleID>(0),shuffle),"configure Bertram");}
 else check(configureSelectedPuzzleRoom(room,type,DIR_EAST,level,rolls,128),"real puzzle configurator");
 check(!room.puzzleBypassed,"fresh puzzle configuration clears a previous bypass");
}
Entity& singleActor(){
 dungeon.currentRoom=0;dungeon.roomCount=1;dungeon.entities=dungeon.activeDungeonEntities;dungeon.entityCount=1;
 auto& a=dungeon.entities[0];a=Entity{};a.type=ENTITY_PLAYER;a.active=true;a.ownerPlayerID=0;a.x=ROOM_WIDTH-2;a.y=6;
 a.character.characterClass=CLASS_ROGUE;a.character.state=STATE_ALIVE;a.character.team=TEAM_PLAYER;a.character.level=1;a.character.health.currentHP=a.character.health.maxHP=12;
 player=a.character;gameState=GAME_DUNGEON;moveDirection=DIR_EAST;combat=Combat{};return a;
}
std::array<uint16_t,256> tile(const DungeonRoom& room,int x,int y){
 drawRoomTile(room,x,y);std::array<uint16_t,256> result{};const auto* pixels=tft.getBuffer();
 for(int j=0;j<16;++j)for(int i=0;i<16;++i)result[j*16+i]=pixels[(y*16+j)*240+x*16+i];return result;
}
void bellVisual(){
 initialize(42);auto& a=singleActor();(void)a;auto& room=dungeon.rooms[0];configure(room,PUZZLE_BELLS);
 const DungeonFurnitureInstance* bells[3]{};for(const auto& f:room.furniture){if(f.type==FURNITURE_BELL_LOW)bells[0]=&f;if(f.type==FURNITURE_BELL_MID)bells[1]=&f;if(f.type==FURNITURE_BELL_HIGH)bells[2]=&f;}
 std::array<uint16_t,256> baseline[3];for(unsigned i=0;i<3;++i){check(bells[i],"bell asset present");baseline[i]=tile(room,bells[i]->x,bells[i]->y);}
 setAudioMuted(true);check(strikeCurrentBellAt(bells[0]->x,bells[0]->y),"muted player bell");
 check(getCurrentBellVisualTone(millis())==BELL_LOW,"player strike visual independent of mute");
 auto ringing=tile(room,bells[0]->x,bells[0]->y);check(ringing!=baseline[0],"original bell framebuffer gets short marks");
 for(auto point:std::array<unsigned,4>{4*16,9*16,4*16+15,9*16+15})check(ringing[point]==ST77XX_WHITE,"two left and two right primitive marks");
 check(tile(room,bells[1]->x,bells[1]->y)==baseline[1]&&tile(room,bells[2]->x,bells[2]->y)==baseline[2],"other bells do not flash");
 wait(299);check(getCurrentBellVisualTone(millis())==BELL_LOW,"299ms pulse retained");wait(1);check(getCurrentBellVisualTone(millis())==BELL_TONE_COUNT,"pulse ends at 300ms");check(tile(room,bells[0]->x,bells[0]->y)==baseline[0],"ring lines disappear exactly");
 check(playBellPuzzleSequence(room.bellPuzzle),"muted sequence still accepted");const auto began=millis();
 for(unsigned i=0;i<room.bellPuzzle.sequenceLength;++i){check(getCurrentBellVisualTone(began+i*460)==room.bellPuzzle.sequence[i],"stored sequence shared timing");check(getCurrentBellVisualTone(began+i*460+300)==BELL_TONE_COUNT,"160ms inter-note visual pause");}
 check(getCurrentBellVisualTone(began+room.bellPuzzle.sequenceLength*460)==BELL_TONE_COUNT,"late/reconnecting viewer sees no stuck pulse");
 check(isCurrentBellListeningRuneAt(room.bellPuzzle.runeX,room.bellPuzzle.runeY),"physical replay button present");
 check(handleCurrentBellListeningRuneEntry(dungeon.entities[0],room.bellPuzzle.runeX,room.bellPuzzle.runeY),"replay from physical control");
 dungeon.entities[0].x=room.bellPuzzle.runeX;dungeon.entities[0].y=room.bellPuzzle.runeY;
 check(tryInteractWithFacingEntity()&&getCurrentBellVisualTone(millis())==room.bellPuzzle.sequence[0],"A replays while standing on button without leaving room");
 std::cout<<"PASS original C++ bell framebuffer: four short marks, only ringing bell, 300ms, mute, stored sequence and physical replay\n";
}
void bypass(){
 initialize(19);for(unsigned level=1;level<=20;++level)check(getRiddlemanLockDisableDC(level)==19+(level+1)/2,"central DC progression");
 for(auto type:{PUZZLE_BELLS,PUZZLE_NUMBER_TILES,PUZZLE_BRAZIERS,PUZZLE_RIDDLEMAN}){
  auto& actor=singleActor();auto& room=dungeon.rooms[0];configure(room,type);const auto bell=room.bellPuzzle;const auto number=room.numberPuzzle;const auto brazier=room.brazierPuzzle;
  for(auto c:{CLASS_FIGHTER,CLASS_WIZARD,CLASS_CLERIC}){actor.character.characterClass=c;check(!canAttemptCurrentPuzzleBypass(actor,DIR_EAST)&&!attemptCurrentPuzzleBypass(actor,DIR_EAST,999),"non-Rogue denied even forged high check");}
  actor.character.characterClass=CLASS_ROGUE;check(canAttemptCurrentPuzzleBypass(actor,DIR_EAST),"Rogue physically facing locked exit");
  check(!attemptCurrentPuzzleBypass(actor,DIR_EAST,19)&&!room.puzzleBypassed,"failed check preserves room");
  actor.x=2;check(!attemptCurrentPuzzleBypass(actor,DIR_EAST,999),"remote exit rejected");actor.x=ROOM_WIDTH-2;
  combat.active=true;check(!attemptCurrentPuzzleBypass(actor,DIR_EAST,999),"combat/out-of-turn action denied");combat.active=false;
  Entity impostor=actor;check(!attemptCurrentPuzzleBypass(impostor,DIR_EAST,999),"only actual controlled actor may bypass");
  unsigned failSeed=0,passSeed=0;
  for(unsigned seed=1;!failSeed||!passSeed;++seed){randomSeed(seed);const auto roll=rollDie(20);if(roll==1)failSeed=seed;if(roll==20)passSeed=seed;check(seed<10000,"bounded deterministic dice fixtures");}
  actor.character.abilities.intelligence=20;player=actor.character;
  check(tryInteractWithFacingEntity()&&menuState.isOpen,"original Rogue contextual Pick Lock menu");randomSeed(failSeed);pickRiddlemanDoorLock();
  check(!room.puzzleBypassed&&canAttemptCurrentPuzzleBypass(actor,DIR_EAST),"failed real Disable Device attempt leaves normal retry available");
  if(type==PUZZLE_RIDDLEMAN){check(std::string(getGameMessage())=="Hey! What are you doing?","Bertram first real attempt reaction retained");check(!noteCurrentRiddlemanBypassAttempt(),"reaction only once");}
  check(tryInteractWithFacingEntity()&&menuState.isOpen,"retry physical lock");randomSeed(passSeed);pickRiddlemanDoorLock();
  check(room.puzzleBypassed&&isDungeonPuzzleComplete(room),"real Disable Device roll and skill machinery unlock passage");
  if(type==PUZZLE_NUMBER_TILES){
   // An unfinished crossing must not become a fake physical solution after bypass.
   dungeon.roomRuntime[0].numberCrossingActive=true;
   actor.x=number.fieldX+number.fieldWidth;actor.y=number.fieldY;
   handleCurrentNumberPuzzleMovement(actor,actor.x-1,actor.y);
   check(room.numberPuzzle.progress==NUMBER_PUZZLE_UNSOLVED,"bypassed field movement cannot forge physical completion");
  }
  check(std::memcmp(&bell,&room.bellPuzzle,sizeof(bell))==0&&std::memcmp(&number,&room.numberPuzzle,sizeof(number))==0&&std::memcmp(&brazier,&room.brazierPuzzle,sizeof(brazier))==0,"bypass does not forge physical solution");
  check(tryUnlockCurrentBellExit(DIR_EAST)&&tryUnlockCurrentNumberPuzzleExit(DIR_EAST)&&tryUnlockCurrentBrazierPuzzleExit(DIR_EAST)&&tryUnlockCurrentRiddleExit(DIR_EAST),"all passage gates honor bypass");
  if(type==PUZZLE_BRAZIERS)check(isCurrentBrazierPuzzleRewardSealed(),"optional brazier chest still requires actual solution");
  check(dungeon.entityCount==1,"bypass creates no bonus key/reward");
  auto persisted=room;room= DungeonRoom{};room=persisted;check(room.puzzleBypassed&&isDungeonPuzzleComplete(room),"room persistence retains bypass");
 }
 std::cout<<"PASS Rogue-only Bell/Number/Brazier/Bertram physical exit bypass, DC20..29, failure/retry, no remote/foreign/combat action, optional rewards preserved\n";
}
void generation(){
 initialize(5);unsigned counts[5]{};uint32_t rules=0;unsigned stateful=0;
 for(unsigned level: {1u,4u,5u,9u,10u,12u,16u,20u})for(unsigned seed=1;seed<=80;++seed){
  player.level=level;randomSeed(seed*101+level);gameState=GAME_TOWN;generateDungeon(dungeon);check(dungeon.runActive,"production generated dungeon");
  for(unsigned i=1;i<dungeon.roomCount;++i){const auto& room=dungeon.rooms[i];if(room.type!=ROOM_PUZZLE)continue;check(room.puzzleType!=PUZZLE_NONE,"no empty puzzle enum in production");++counts[room.puzzleType];
   if(room.puzzleType!=PUZZLE_NUMBER_TILES)continue;const auto& n=room.numberPuzzle;check(validateNumberPuzzleSafePath(room),"production numbered room has legal solution");check(getNumberTileClue(n)[0],"production clue readable");rules|=1u<<n.rule;stateful+=getNumberTileRuleDefinition(n.rule)->evaluationMode==NUMBER_RULE_STATEFUL;
   for(unsigned y=0;y<n.fieldHeight;++y)for(unsigned x=0;x<n.fieldWidth;++x){auto d=getNumberPuzzleDigitAt(room,n.fieldX+x,n.fieldY+y);check(d>=n.minimumDigit&&d<=n.maximumDigit,"digits match level/rule range");}
  }
 }
 check(counts[PUZZLE_BELLS]&&counts[PUZZLE_NUMBER_TILES]&&counts[PUZZLE_BRAZIERS]&&counts[PUZZLE_RIDDLEMAN]&&stateful,"all puzzles and stateful number rules reachable through actual generator");
 unsigned weight[5]{};for(unsigned roll=0;roll<100;++roll)++weight[selectRandomPuzzleType(roll)];check(weight[PUZZLE_BELLS]==34&&weight[PUZZLE_NUMBER_TILES]==33&&weight[PUZZLE_BRAZIERS]==33,"unchanged rotating pool weights");
 std::cout<<"PASS actual production generation 640 dungeons bell="<<counts[2]<<" number="<<counts[3]<<" brazier="<<counts[4]<<" Bertram="<<counts[1]<<" stateful="<<stateful<<" rule_mask="<<rules<<'\n';
}
using Point=std::pair<int,int>;
bool inside(const NumberTilePuzzleState& n,int x,int y){return x>=n.fieldX&&x<n.fieldX+n.fieldWidth&&y>=n.fieldY&&y<n.fieldY+n.fieldHeight;}
bool nearEdge(const NumberTilePuzzleState& n,int x,int y){return n.lockedExitDirection==DIR_EAST?x==n.fieldX:n.lockedExitDirection==DIR_WEST?x==n.fieldX+n.fieldWidth-1:n.lockedExitDirection==DIR_SOUTH?y==n.fieldY:y==n.fieldY+n.fieldHeight-1;}
bool farEdge(const NumberTilePuzzleState& n,int x,int y){return n.lockedExitDirection==DIR_EAST?x==n.fieldX+n.fieldWidth-1:n.lockedExitDirection==DIR_WEST?x==n.fieldX:n.lockedExitDirection==DIR_SOUTH?y==n.fieldY+n.fieldHeight-1:y==n.fieldY;}
std::vector<Point> solution(const DungeonRoom& room){
 const auto& n=room.numberPuzzle;bool stateful=getNumberTileRuleDefinition(n.rule)->evaluationMode==NUMBER_RULE_STATEFUL;
 struct Node{int x,y,step,parent;};std::vector<Node> nodes;bool seen[9][ROOM_HEIGHT][ROOM_WIDTH]{};unsigned head=0;int goal=-1;
 for(int y=n.fieldY;y<n.fieldY+n.fieldHeight;++y)for(int x=n.fieldX;x<n.fieldX+n.fieldWidth;++x)if(nearEdge(n,x,y)){
  auto d=getNumberPuzzleDigitAt(room,x,y);if(stateful?d==n.requiredSequence[0]:isNumberTileSafe(n.rule,d)){nodes.push_back({x,y,0,-1});seen[0][y][x]=true;}}
 while(head<nodes.size()){
  const auto current=nodes[head];if(farEdge(n,current.x,current.y)&&(!stateful||current.step==n.requiredLength-1)){goal=head;break;}
  for(unsigned d=0;d<8;d+=2){int x=current.x+directionOffsets[d].dx,y=current.y+directionOffsets[d].dy,step=stateful?current.step+1:0;
   if(!inside(n,x,y)||(stateful&&step>=n.requiredLength)||seen[step][y][x])continue;
   auto digit=getNumberPuzzleDigitAt(room,x,y);if(stateful?digit!=n.requiredSequence[step]:!isNumberTileSafe(n.rule,digit))continue;
   seen[step][y][x]=true;nodes.push_back({x,y,step,int(head)});
  }++head;
 }
 check(goal>=0,"independent solver finds cardinal solution");std::vector<Point> path;
 for(int i=goal;i>=0;i=nodes[i].parent)path.push_back({nodes[i].x,nodes[i].y});std::reverse(path.begin(),path.end());return path;
}
void moveOne(Point target){auto* a=getActiveMapPlayer();check(a,"controlled actor");const int dx=target.first-a->x,dy=target.second-a->y;bool found=false;
 for(unsigned d=0;d<8;++d)if(directionOffsets[d].dx==dx&&directionOffsets[d].dy==dy){moveDirection=static_cast<Direction>(d);found=true;break;}
 check(found,"one tile movement");check(tryMovePlayer(dungeon),"actual production movement accepts path tile");check(a->x==target.first&&a->y==target.second,"only requested tile reached");
}
std::vector<Point> floorPath(Point from,Point goal){
 int parent[ROOM_HEIGHT][ROOM_WIDTH];for(auto& row:parent)std::fill(std::begin(row),std::end(row),-1);std::queue<Point> q;q.push(from);parent[from.second][from.first]=from.second*ROOM_WIDTH+from.first;
 const auto& room=dungeon.rooms[dungeon.currentRoom];bool found=false;
 while(!q.empty()){auto a=q.front();q.pop();if(a==goal){found=true;break;}
  for(unsigned d=0;d<8;d+=2){int x=a.first+directionOffsets[d].dx,y=a.second+directionOffsets[d].dy;if(x<=0||y<=0||x>=ROOM_WIDTH-1||y>=ROOM_HEIGHT-1||parent[y][x]>=0)continue;
   auto t=room.map.tiles[y][x];if(t!=TILE_FLOOR&&t!=TILE_RUBBLE&&t!=TILE_NUMBER_CLUE_PLAQUE&&t!=TILE_BELL_LISTEN_RUNE)continue;
   auto* e=getEntityAt(dungeon.entities,dungeon.entityCount,x,y);if(e&&e!=getActiveMapPlayer())continue;parent[y][x]=a.second*ROOM_WIDTH+a.first;q.push({x,y});
  }
 }
 check(found,"physical floor approach exists");std::vector<Point> result;for(Point a=goal;a!=from;){result.push_back(a);int v=parent[a.second][a.first];a={v%ROOM_WIDTH,v/ROOM_WIDTH};}std::reverse(result.begin(),result.end());return result;
}
Point landing(const NumberTilePuzzleState& n,Point first){auto d=directionOffsets[n.lockedExitDirection];return {first.first-d.dx,first.second-d.dy};}
void numberGeneration(){
 initialize(57);uint32_t ruleMask=0;unsigned cases=0;
 for(unsigned level=1;level<=20;++level)for(auto dir:{DIR_NORTH,DIR_EAST,DIR_SOUTH,DIR_WEST})for(unsigned seed=0;seed<64;++seed){
  auto& room=dungeon.rooms[0];roomGeometry(room);
  if(dir==DIR_NORTH||dir==DIR_SOUTH){
   room.map.tiles[6][0]=room.map.tiles[6][ROOM_WIDTH-1]=TILE_WALL;
   room.connections[0]={DIR_NORTH,7,0};room.connections[1]={DIR_SOUTH,7,ROOM_HEIGHT-1};
   room.map.tiles[0][7]=room.map.tiles[ROOM_HEIGHT-1][7]=TILE_DOOR;
  }
  uint8_t rolls[128];uint32_t rng=seed*73+level;for(auto& roll:rolls){rng=rng*1664525u+1013904223u;roll=rng>>24;}
  // Include the worst repeated input to exercise deterministic seed fallback.
  if(seed==0)std::fill(std::begin(rolls),std::end(rolls),0);
  if(!configureNumberTilePuzzleRoom(room,dir,level,rolls,128))throw std::runtime_error("Number generation level="+std::to_string(level)+" direction="+std::to_string(dir)+" seed="+std::to_string(seed));
  const auto& n=room.numberPuzzle;check(validateNumberPuzzleSafePath(room),"all generated Number fields have cardinal solutions");ruleMask|=1u<<n.rule;
  for(int y=n.fieldY;y<n.fieldY+n.fieldHeight;++y)for(int x=n.fieldX;x<n.fieldX+n.fieldWidth;++x){auto digit=getNumberPuzzleDigitAt(room,x,y);check(digit>=n.minimumDigit&&digit<=n.maximumDigit,"all digit ranges valid");}
  dungeon.currentRoom=0;dungeon.roomCount=1;dungeon.entities=nullptr;dungeon.entityCount=0;
  Point near=dir==DIR_EAST?Point{1,6}:dir==DIR_WEST?Point{ROOM_WIDTH-2,6}:dir==DIR_SOUTH?Point{7,1}:Point{7,ROOM_HEIGHT-2};
  Point far=dir==DIR_EAST?Point{ROOM_WIDTH-2,6}:dir==DIR_WEST?Point{1,6}:dir==DIR_SOUTH?Point{7,ROOM_HEIGHT-2}:Point{7,1};
  const auto route=floorPath(near,far);check(!route.empty()&&room.numberPuzzle.progress==NUMBER_PUZZLE_UNSOLVED,"physical lock approach never grants a fake puzzle solution");++cases;
 }
 check(ruleMask==((1u<<NUMBER_RULE_COUNT)-1),"every existing numeric rule is reachable across supported levels");
 std::cout<<"PASS Number generation cases="<<cases<<" all_rules="<<unsigned(NUMBER_RULE_COUNT)<<" levels=1..20 cardinal solutions, bounded fallback, physical lock approach and valid digits\n";
}

void approach(Point target){auto* a=getActiveMapPlayer();for(auto p:floorPath({a->x,a->y},target))moveOne(p);}
void productionSolve(){
 initialize(81);unsigned solved=0,invalid=0,stateful=0;
 for(unsigned level:{1u,12u,20u}){
  int index=-1;
  for(unsigned seed=1;seed<=1000&&index<0;++seed){player=Character{};player.level=level;player.characterClass=CLASS_ROGUE;player.team=TEAM_PLAYER;player.state=STATE_ALIVE;player.health.currentHP=player.health.maxHP=12;randomSeed(seed*101+level);gameState=GAME_TOWN;generateDungeon(dungeon);
   for(unsigned i=1;i<dungeon.roomCount;++i)if(dungeon.rooms[i].puzzleType==PUZZLE_NUMBER_TILES&&(level==1||getNumberTileRuleDefinition(dungeon.rooms[i].numberPuzzle.rule)->evaluationMode==NUMBER_RULE_STATEFUL)){index=i;break;}}
  check(index>=0,"Number room selected by production generator");auto expected=dungeon.rooms[index].numberPuzzle;const auto dir=expected.lockedExitDirection;
  const auto entry=dir==DIR_EAST?ENTRY_WEST:dir==DIR_WEST?ENTRY_EAST:dir==DIR_SOUTH?ENTRY_NORTH:ENTRY_SOUTH;
  check(persistActiveDungeonRoom(dungeon),"persist entrance");dungeon.currentRoom=index;check(loadRoom(dungeon,entry),"production load generated Number room");gameState=GAME_DUNGEON;
  check(std::strstr(getGameMessage(),"Follow the rule.")!=nullptr,"entry objective through original message bar");
  check(interactWithCurrentNumberPuzzleClue(expected.clueX,expected.clueY),"original clue interaction");
  const bool sequence=getNumberTileRuleDefinition(expected.rule)->evaluationMode==NUMBER_RULE_STATEFUL;stateful+=sequence;
  const auto path=solution(dungeon.rooms[index]);auto* actor=getActiveMapPlayer();check(actor,"production actor in Number room");
  // An invalid first tile rejects the step without damage; the valid route remains.
  for(int y=expected.fieldY;y<expected.fieldY+expected.fieldHeight&&!invalid;++y)for(int x=expected.fieldX;x<expected.fieldX+expected.fieldWidth&&!invalid;++x)if(nearEdge(expected,x,y)){
   const auto d=getNumberPuzzleDigitAt(dungeon.rooms[index],x,y);if(sequence?d==expected.requiredSequence[0]:isNumberTileSafe(expected.rule,d))continue;
   approach(landing(expected,{x,y}));const auto hp=actor->character.health.currentHP;moveDirection=static_cast<Direction>(dir);check(!tryMovePlayer(dungeon)&&actor->character.health.currentHP==hp,"invalid number step nonlethal and rejected");++invalid;
  }
  const auto stable=dungeon.rooms[index].numberPuzzle;
  check(persistActiveDungeonRoom(dungeon),"persist generated room");dungeon.currentRoom=0;check(loadRoom(dungeon,ENTRY_START),"leave generated room");dungeon.currentRoom=index;check(loadRoom(dungeon,entry),"return generated room");
  check(std::memcmp(&stable,&dungeon.rooms[index].numberPuzzle,sizeof(stable))==0,"exact Number rule/digits/state persist on re-entry");actor=getActiveMapPlayer();
  approach(landing(expected,path.front()));for(auto point:path)moveOne(point);
  const auto d=directionOffsets[dir];moveOne({actor->x+d.dx,actor->y+d.dy});
  check(dungeon.rooms[index].numberPuzzle.progress==NUMBER_PUZZLE_COMPLETE&&tryUnlockCurrentNumberPuzzleExit(static_cast<Direction>(dir)),"actual cardinal crossing solves and unlocks");++solved;
  check(persistActiveDungeonRoom(dungeon),"persist solved Number room");dungeon.currentRoom=0;check(loadRoom(dungeon,ENTRY_START),"leave solved");dungeon.currentRoom=index;check(loadRoom(dungeon,entry),"return solved");check(isDungeonPuzzleComplete(dungeon.rooms[index]),"solved exit never relocks");
 }
 check(solved==3&&invalid,"production solve and invalid-path cases ran");std::cout<<"PASS actual generated Number room: entrance/load/clue/invalid step/cardinal solution/re-entry persistence at levels1,12,20; stateful="<<stateful<<'\n';
}
void tap(unsigned id,const char* action){check(controllerInput(id,action,true),"controller down");wait(30);check(controllerInput(id,action,false),"controller up");wait(310);}
void multiplayer(){
 initializeControllers(2);ControllerState states[2];
 for(unsigned id=0;id<2;++id){check(bindController(id,states[id]),"bind independent controller");tap(id,"a");tap(id,"a");tap(id,"a");tap(id,"a");tap(id,"a");tap(id,"encoder");check(gameState==GAME_TOWN,"original character creation");tap(id,"cw");tap(id,"cw");tap(id,"a");if(menuState.isOpen)tap(id,"a");check(gameState==GAME_DUNGEON,"shared original dungeon");}
 selectController(0);auto& room=dungeon.rooms[1];bool found=false;
 for(unsigned seed=1;seed<256&&!found;++seed){uint8_t rolls[128];for(unsigned i=0;i<128;++i)rolls[i]=(i*37+seed)%256;rolls[0]=0;roomGeometry(room);if(!configureNumberTilePuzzleRoom(room,DIR_EAST,12,rolls,128))continue;
  if(getNumberTileRuleDefinition(room.numberPuzzle.rule)->evaluationMode!=NUMBER_RULE_STATEFUL)continue;
  unsigned starts=0;for(int y=room.numberPuzzle.fieldY;y<room.numberPuzzle.fieldY+room.numberPuzzle.fieldHeight;++y)starts+=getNumberPuzzleDigitAt(room,room.numberPuzzle.fieldX,y)==room.numberPuzzle.requiredSequence[0];found=starts>=2;
 }
 check(found,"generated stateful Number room with independent entry choices");const auto blueprint=room.numberPuzzle;
 dungeon.currentRoom=1;check(loadRoom(dungeon,ENTRY_WEST),"first player enters Number room");check(nativeActorRoom(0)==1&&nativeActorRoom(1)==0,"room transition never teleports peer");
 nativeActor(0)->character.characterClass=CLASS_ROGUE;
 selectController(1);check(!canAttemptCurrentPuzzleBypass(*nativeActor(1),DIR_EAST),"player elsewhere cannot use Rogue in another room");dungeon.currentRoom=1;check(loadRoom(dungeon,ENTRY_WEST),"second player enters same authoritative room");auto* a=nativeActor(0);auto* b=nativeActor(1);
 check(a&&b&&a!=b&&std::memcmp(&blueprint,&dungeon.rooms[1].numberPuzzle,sizeof(blueprint))==0,"same rule and digits, distinct actors");
 auto route=solution(room);std::vector<Point> starts;for(int y=blueprint.fieldY;y<blueprint.fieldY+blueprint.fieldHeight;++y)if(getNumberPuzzleDigitAt(room,blueprint.fieldX,y)==blueprint.requiredSequence[0])starts.push_back({blueprint.fieldX,y});
 auto second=*std::find_if(starts.begin(),starts.end(),[&](Point p){return p!=route.front();});
 selectController(0);approach(landing(blueprint,route.front()));moveOne(route.front());check(a->nativeNumberCurrentStep==1&&b->nativeNumberCurrentStep==0,"first progression belongs only to first actor");
 selectController(1);approach(landing(blueprint,second));moveOne(second);check(a->nativeNumberCurrentStep==1&&b->nativeNumberCurrentStep==1,"second independently enters first sequence step");
 // Put the first actor on the preferred reset landing using real movement.
 selectController(0);moveOne(landing(blueprint,route.front()));
 Point preferred={blueprint.fieldX-1,blueprint.fieldY+blueprint.fieldHeight/2};approach(preferred);
 const auto ax=a->x,ay=a->y;selectController(1);bool badStep=false;const auto hp=b->character.health.currentHP;
 for(unsigned d=0;d<8&&!badStep;d+=2){int x=b->x+directionOffsets[d].dx,y=b->y+directionOffsets[d].dy;
  if(!inside(blueprint,x,y)||getNumberPuzzleDigitAt(room,x,y)==blueprint.requiredSequence[1])continue;
  if(getEntityAt(dungeon.entities,dungeon.entityCount,x,y))continue;moveDirection=static_cast<Direction>(d);
  check(!tryMovePlayer(dungeon),"stateful wrong tile rejected");badStep=true;
 }
 check(badStep&&b->nativeNumberCurrentStep==0&&b->character.health.currentHP==hp,"stateful invalid path resets only acting player without damage");
 check(a->x==ax&&a->y==ay&&(a->x!=b->x||a->y!=b->y),"reset chooses free near landing, never stacks or teleports peer");
 selectController(0);approach(landing(blueprint,route.front()));moveOne(route.front());
 const auto bx=b->x,by=b->y;selectController(0);for(unsigned i=1;i<route.size();++i)moveOne(route[i]);moveOne({a->x+1,a->y});check(room.numberPuzzle.progress==NUMBER_PUZZLE_COMPLETE&&b->x==bx&&b->y==by,"one completion unlocks shared room without moving peer");
 selectController(1);check(tryUnlockCurrentNumberPuzzleExit(DIR_EAST),"peer sees authoritative unlocked route");check(controllerPixels(0)&&controllerPixels(1)&&controllerPixels(0)!=controllerPixels(1),"independent original framebuffer surfaces");
 // The same room-owned visual clock is visible from either authorized observer.
 configure(room,PUZZLE_BELLS);const DungeonFurnitureInstance* bell=nullptr;for(const auto& f:room.furniture)if(f.type==FURNITURE_BELL_MID)bell=&f;check(bell,"shared bell fixture");
 selectController(0);setAudioMuted(true);check(strikeCurrentBellAt(bell->x,bell->y),"first rings shared bell");const auto began=millis();selectController(1);check(getCurrentBellVisualTone(began)==BELL_MID,"second sees same shared bell, even muted");wait(300);check(getCurrentBellVisualTone(millis())==BELL_TONE_COUNT,"both observers see expiry");
 // Only the Rogue physically at this room's exit can perform the bypass.
 selectController(1);dungeon.currentRoom=0;check(loadRoom(dungeon,ENTRY_START),"Fighter returns to other room");
 check(!canAttemptCurrentPuzzleBypass(*nativeActor(1),DIR_EAST),"Fighter cannot borrow remote Rogue ability");
 selectController(0);approach({ROOM_WIDTH-2,6});moveDirection=DIR_EAST;
 check(tryInteractWithFacingEntity()&&menuState.isOpen,"original Pick Lock contextual menu at physical puzzle door");
 closeMenu();check(attemptCurrentPuzzleBypass(*a,DIR_EAST,20)&&room.puzzleBypassed,"local Rogue unlocks shared bell exit");
 selectController(1);dungeon.currentRoom=1;check(loadRoom(dungeon,ENTRY_WEST),"Fighter enters bypassed room");check(tryUnlockCurrentBellExit(DIR_EAST),"other member sees same bypassed passage");
 std::cout<<"PASS two production controllers: independent Number progress and coordinates, shared layout/clue/completion, separate framebuffers, room-local bypass authority and shared muted bell pulse\n";
}

}
int main(int argc,char** argv){try{const std::string mode=argc>1?argv[1]:"bell";if(mode=="bell")bellVisual();else if(mode=="bypass")bypass();else if(mode=="generation")generation();else if(mode=="production")productionSolve();else if(mode=="number-generation")numberGeneration();else if(mode=="multiplayer")multiplayer();else throw std::runtime_error("mode");}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" "<<diagnostic()<<'\n';return 1;}}
