#include "native_party.h"
#include "native_rooms.h"
#include "dungeon/combat.h"
#include <array>
#include <memory>
#include <stdexcept>
#include "context_fields.h"
namespace pfme { namespace runtime {
namespace {
struct World {Dungeon dungeon{};Combat combat{};};
std::array<World,NATIVE_WORLD_CAPACITY> worlds;
std::array<unsigned,NATIVE_CONTROLLER_CAPACITY> ownership{};
unsigned selectedWorld=0;
}
unsigned nativeWorldIndex(){return selectedWorld;}
unsigned nativeControllerWorld(PlayerID id){return isValidController(id)?ownership[id]:NATIVE_WORLD_CAPACITY;}
void selectNativeWorld(unsigned next){
 if(next>=NATIVE_WORLD_CAPACITY)throw std::out_of_range("World index");
 if(next==selectedWorld)return;
 saveNativeRoomView(dungeon);
 worlds[selectedWorld].dungeon=dungeon;worlds[selectedWorld].combat=combat;
 selectNativeForestWorld(selectedWorld,next);switchNativeWorldRandom(selectedWorld,next);
 selectedWorld=next;dungeon=worlds[next].dungeon;combat=worlds[next].combat;
 invalidateNativeRoomView();
}
void resetNativeWorld(){
 if(combat.active)throw std::logic_error("Cannot reset active combat");
 for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(nativeActor(id))throw std::logic_error("Cannot reset occupied world");
 auto fresh=std::make_unique<World>();dungeon=fresh->dungeon;combat=fresh->combat;
 resetNativeResidentRooms();resetNativeForest();resetNativeRoomFields();
}
void assignNativeWorld(PlayerID id,unsigned next){
 if(!isValidController(id)||next>=NATIVE_WORLD_CAPACITY)throw std::out_of_range("World owner");
 ownership[id]=next;
}
}}
