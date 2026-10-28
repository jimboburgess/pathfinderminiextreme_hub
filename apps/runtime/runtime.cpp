#include "controller_slots.h"
#include "context_fields.h"
#include "native_rooms.h"
#include "input/buttons.h"
#include "graphics/display.h"
#include "map/awareness.h"
#include "map/monsteridle.h"
#include "graphics/elementalvisual.h"
#include "town/town.h"
#include "audio/audio.h"
#include "map/mapeffects.h"
#include "characters/sheet.h"
#include <limits>
#include "runtime.h"
#include "Arduino.h"
#include "SPI.h"
#include "config.h"
#include "data/game.h"
#include "map/activemap.h"
#include "dungeon/combat.h"
#include "graphics/messagelog.h"
#include "graphics/charcreationscreen.h"
#include "input/menu.h"
#include <array>
#include <random>
#include <sstream>
#include <stdexcept>
void setup();
void loop();
NativeSerial Serial;
NativeSPI SPI;
namespace {
std::mt19937 rng;
uint32_t nowMs=1;
std::array<int,64> pins;
bool initialized=false;
bool multiController=false;
uint8_t selected=INVALID_PLAYER_ID;
bool worldEvent=false;
struct WorldEventScope {bool previous=worldEvent;WorldEventScope(){worldEvent=true;}~WorldEventScope(){worldEvent=previous;}};
struct TargetingView {
    CombatAttackType attack=COMBAT_ATTACK_NONE;
    int8_t target=-1;bool opening=false;
    AbilityID ability=ABILITY_NONE;bool scroll=false;
    ItemInstance scrollItem={ITEM_NONE,0,WEAPON_PROPERTY_NONE};
    int8_t x=-1,y=-1;Direction direction=DIR_NORTH;DamageType damage=DAMAGE_NONE;
};
bool hadCombat=false;
struct BoundController {
    pfme::runtime::ControllerState* state=nullptr;
    std::array<int,64> pins{};
    bool connected=false;
    uint32_t lastWorldDraw=0;
    bool disconnectGraceUsed=false;uint32_t graceStart=0;
    bool inspecting=false;int8_t inspected=-1;
    TargetingView targeting;
    BoundController() { pins.fill(HIGH); }
};
BoundController bound[MAX_MULTIPLAYER_PLAYERS];
}
unsigned long millis() { return nowMs; }
void delay(unsigned long elapsed) { nowMs+=static_cast<uint32_t>(elapsed); }
uint32_t esp_random() { return static_cast<uint32_t>(rng()); }
void randomSeed(unsigned long seed) { rng.seed(static_cast<uint32_t>(seed)); }
long random(long upper) { return upper<=0?0:static_cast<long>(esp_random()%static_cast<uint32_t>(upper)); }
long random(long lower,long upper) { return upper<=lower?lower:lower+random(upper-lower); }
int digitalRead(int pin) { return pin>=0 && pin<64?pins[pin]:HIGH; }
void digitalWrite(int pin,int value) { if(pin>=0 && pin<64) pins[pin]=value; }
void pinMode(int pin,int mode) { if(mode==INPUT_PULLUP) digitalWrite(pin,HIGH); }
namespace pfme { namespace runtime {
void initialize(uint32_t seed) {
    if(initialized) throw std::logic_error("Only one production runtime per process");
    initialized=true; pins.fill(HIGH); randomSeed(seed);
    if(!tft.getBuffer()) throw std::runtime_error("Display allocation failed");
    setup(); loop();
}
namespace {
void synchronizeCombatViews() {
    if(hadCombat!=combat.active) {
        for(auto& controller:bound)controller.targeting=TargetingView{};
        hadCombat=combat.active;
    }
}
void renderController() {
    // Other actors dirty the shared world, not this observer's private tile
    // cache. Refresh each live map at 20 Hz; input/UI redraws remain immediate.
    if(multiController && isValidPlayerID(selected) && gameState==GAME_DUNGEON &&
       uint32_t(nowMs-bound[selected].lastWorldDraw)>=50) {
        bound[selected].lastWorldDraw=nowMs;
        backgroundNeedsRedraw=true;redrawType=REDRAW_FULL;needsRedraw=true;
    }
    updateMonsterVisibility();updateElementalVisualEffect();updateGameMessage();updateTownRest();
    if(needsRedraw)refreshDisplay();
    if(gameState==GAME_START)drawStartAnimation();
}
void inputStep() {
    if(!multiController) {loop();return;}
    const auto owner=selected;
    handleButtons();synchronizeCombatViews();
    if(selected!=owner)selectController(owner);
    renderController();
}
void storeController() {
    if(!isValidPlayerID(selected) || !bound[selected].state)return;
    auto* actor=nativeActor(selected);
    bound[selected].state->character=actor && gameState==GAME_DUNGEON?actor->character:player;
    bound[selected].state->viewRoom=dungeon.currentRoom;
    bound[selected].state->facing=moveDirection;
    bound[selected].pins=pins;
    bound[selected].state->input=productionButtonState();
    bound[selected].state->creation=productionCreationState();
    bound[selected].state->menu=menuState;
    bound[selected].inspecting=combat.inspecting;bound[selected].inspected=combat.inspectedEntityIndex;
    if(!combat.active)bound[selected].targeting={combat.attackType,combat.selectedTargetIndex,combat.openingAttackTargeting,
        combat.selectedAbility,combat.selectedAbilityFromScroll,combat.selectedAbilityScroll,
        combat.selectedAbilityX,combat.selectedAbilityY,combat.selectedAbilityDirection,combat.selectedAbilityDamageType};
}
}
PlayerID selectedController() {return selected;}
bool nativeRoomsActive() {return multiController;}
uint32_t worldTime() {return nowMs;}
void initializeControllers(uint32_t seed) {
    initialize(seed);multiController=true;
}
bool selectController(uint8_t id) {
    if(!multiController || !isValidPlayerID(id) || !bound[id].state)return false;
    if(selected==id)return true;
    storeController();
    saveNativeRoomView(dungeon);
    if(isValidPlayerID(selected)) tft.exchangeSurface(bound[selected].state->surface);
    if(isValidPlayerID(selected))switchPresentation(selected,id);
    else loadPresentation(id);
    selected=id;player=bound[id].state->character;pins=bound[id].pins;
    productionButtonState()=bound[id].state->input;
    productionCreationState()=bound[id].state->creation;
    menuState=bound[id].state->menu;
    combat.inspecting=bound[id].inspecting;combat.inspectedEntityIndex=bound[id].inspected;
    if(!combat.active) {
        const auto& view=bound[id].targeting;
        combat.attackType=view.attack;combat.selectedTargetIndex=view.target;combat.openingAttackTargeting=view.opening;
        combat.selectedAbility=view.ability;combat.selectedAbilityFromScroll=view.scroll;combat.selectedAbilityScroll=view.scrollItem;
        combat.selectedAbilityX=view.x;combat.selectedAbilityY=view.y;combat.selectedAbilityDirection=view.direction;combat.selectedAbilityDamageType=view.damage;
    }
    tft.exchangeSurface(bound[id].state->surface);
    bindNativeRoom(dungeon,id);
    return true;
}
bool bindController(uint8_t id,ControllerState& state) {
    if(!multiController || !isValidPlayerID(id))return false;
    if(bound[id].state && bound[id].state!=&state)return false;
    bound[id].state=&state;bound[id].connected=true;
    if(!selectController(id))return false;
    renderController();return true;
}
void setControllerConnected(uint8_t id,bool connected) {
    if(!selectController(id))return;
    releaseInputs();productionButtonState()=ButtonInputState{};resetButtonStates();
    bound[id].connected=connected;
    if(connected && (!combat.active || getCurrentCombatant()!=nativeActor(id))) {bound[id].disconnectGraceUsed=false;bound[id].graceStart=0;}
    storeController();
}
bool unbindController(uint8_t id) {
    if(!selectController(id) || combat.active)return false;
    if(nativeActor(id) && !leaveNativeRoom(dungeon))return false;
    storeController();tft.exchangeSurface(bound[id].state->surface);
    bound[id]=BoundController{};selected=INVALID_PLAYER_ID;
    resetPresentation(id);retireNativeObserver(id);return true;
}
bool controllerInput(uint8_t id,const std::string& action,bool down) {
    if(!isValidPlayerID(id) || !bound[id].connected || !selectController(id))return false;
    const bool result=input(action,down);storeController();return result;
}
const uint16_t* controllerPixels(uint8_t id) {
    if(!isValidPlayerID(id) || !bound[id].state)return nullptr;
    return id==selected?tft.getBuffer():bound[id].state->surface.getBuffer();
}
bool isNativeWorldEvent() {return multiController && worldEvent;}
bool broadcastNativeMessage(const char* text) {
    if(!isNativeWorldEvent() || !isValidPlayerID(selected))return false;
    const std::string message=text?text:"";const auto owner=selected,room=nativeActorRoom(owner);
    worldEvent=false;
    for(PlayerID id=0;id<MAX_MULTIPLAYER_PLAYERS;++id)
        if(bound[id].state && nativeActorRoom(id)==room && room!=NO_ROOM) {
            selectController(id);setGameMessage(message.c_str());
        }
    selectController(owner);worldEvent=true;return true;
}
namespace {
PlayerID combatController(Entity* entity) {
    if(!entity)return INVALID_PLAYER_ID;
    if(entity->type==ENTITY_PLAYER)return entity->ownerPlayerID;
    auto* pending=combat.monsterAttackTarget;
    if(pending && pending->type==ENTITY_PLAYER && nativeEntityRoom(pending)==nativeEntityRoom(entity))return pending->ownerPlayerID;
    const auto room=nativeEntityRoom(entity);int distance=std::numeric_limits<int>::max();PlayerID result=INVALID_PLAYER_ID;
    for(PlayerID id=0;id<MAX_MULTIPLAYER_PLAYERS;++id) {
        auto* actor=nativeActor(id);
        if(!actor || !bound[id].state || nativeActorRoom(id)!=room || actor->character.state!=STATE_ALIVE)continue;
        const int candidate=getEntityGridDistance(*entity,*actor);
        if(candidate<distance){distance=candidate;result=id;}
    }
    return result;
}
void updateNativeCombat() {
    if(!combat.active)return;
    auto* current=getCurrentCombatant();const auto id=combatController(current);
    if(!isValidPlayerID(id)) {
        if(current && current->type!=ENTITY_PLAYER && combat.phase==COMBAT_TURN)nextTurn();
        return;
    }
    selectController(id);WorldEventScope scope;
    if(current->type==ENTITY_PLAYER && combat.phase==COMBAT_TURN && combat.waitingForPlayer && !bound[id].connected) {
        auto& owner=bound[id];
        if(!owner.graceStart)owner.graceStart=nowMs;
        if(owner.disconnectGraceUsed || uint32_t(nowMs-owner.graceStart)>=15000) {
            owner.disconnectGraceUsed=true;endPlayerTurn();return;
        }
    }
    updateCombat();synchronizeCombatViews();storeController();
}
}
bool clearNativeMapEffects() {
    static bool clearing=false;
    if(!multiController || clearing || !isValidPlayerID(selected))return false;
    const auto owner=selected;clearing=true;bool found=false;
    for(uint8_t room=0;room<dungeon.roomCount;++room)if(bindNativeWorldRoom(dungeon,room)) {
        found=true;clearMapEffects();updateCurrentDungeonRoomCompletion(dungeon);
    }
    if(!found)clearMapEffects();
    bindNativeRoom(dungeon,owner);clearing=false;return true;
}
void scanNativeRoomAwareness() {
    const auto owner=selected,room=dungeon.currentRoom;
    for(PlayerID id=0;id<MAX_MULTIPLAYER_PLAYERS;++id) {
        auto* actor=nativeActor(id);
        if(!bound[id].state || !actor || nativeActorRoom(id)!=room || actor->character.state!=STATE_ALIVE)continue;
        selectController(id);uint8_t count=0;auto* entities=getActiveMapEntities(count);
        if(scanPlayerMonsterAwareness(actor,entities,count))break;
    }
    selectController(owner);
}
void prepareNativeCombatant(Entity* entity) {
    const auto id=combatController(entity);if(isValidPlayerID(id))selectController(id);
}
void tickNativeCombatRooms() {
    const auto owner=selected;bool visited[MAX_ROOMS]{};
    for(unsigned i=0;i<combat.combatantCount;++i) {
        const auto room=nativeEntityRoom(combat.initiativeOrder[i]);
        if(room==NO_ROOM || visited[room])continue;visited[room]=true;
        bool controllerFound=false;
        for(PlayerID id=0;id<MAX_MULTIPLAYER_PLAYERS;++id)if(bound[id].state && nativeActorRoom(id)==room) {
            selectController(id);controllerFound=true;break;
        }
        if(!controllerFound)bindNativeWorldRoom(dungeon,room);
        tickMapEffects();updateElementalTrapCharges();checkReinforcementAwareness();
    }
    selectController(owner);bindNativeRoom(dungeon,owner);
}
void advance(uint32_t elapsed) {
    if(!initialized) return;
    // Bounded 10 ms ticks preserve production timers and input debounce.
    elapsed=std::min(elapsed,100u);
    while(elapsed) { const auto step=std::min(elapsed,10u); nowMs+=step; elapsed-=step;
        if(!multiController)loop();
        else {
            const auto original=selected;
            // One clock increment per world tick, independent of client count.
            bool visited[MAX_ROOMS]{};
            for(uint8_t id=0;id<MAX_MULTIPLAYER_PLAYERS;++id)if(bound[id].state) {
                selectController(id);
                if(bound[id].connected)handleButtons();
                if(gameState==GAME_DUNGEON && nativeActor(id) && nativeActor(id)->character.state==STATE_ALIVE && dungeon.currentRoom<MAX_ROOMS && !visited[dungeon.currentRoom]) {
                    visited[dungeon.currentRoom]=true;
                    WorldEventScope scope;
                    updateAwareness();updateElementalTrapCharges();updateMonsterIdleBehavior();
                    synchronizeCombatViews();
                }
                renderController();storeController();
            }
            updateNativeCombat();
            if(isValidPlayerID(original))selectController(original);
        }
     }
}
void releaseInputs() { pins[ENCODER_SW]=pins[BUTTON_A]=pins[BUTTON_B]=HIGH; }
bool input(const std::string& action,bool down) {
    if(!initialized) return false;
    if(action=="release") { releaseInputs(); inputStep(); return true; }
    if(action=="cw" || action=="ccw") {
        if(!down) return true;
        const int cw[]={2,0,1,3}, ccw[]={1,0,2,3};
        for(int phase: action=="cw"?cw:ccw) {
            pins[ENCODER_CLK]=(phase&2)?HIGH:LOW; pins[ENCODER_DT]=(phase&1)?HIGH:LOW; inputStep();
        }
        return true;
    }
    const int pin=action=="encoder"?ENCODER_SW:action=="a"?BUTTON_A:action=="b"?BUTTON_B:-1;
    if(pin<0) return false;
    pins[pin]=down?LOW:HIGH; inputStep(); return true;
}
const uint16_t* pixels() { return tft.getBuffer(); }
std::string diagnostic() {
    std::ostringstream out;
    out<<"state="<<int(gameState)<<" creation="<<int(getCharacterCreationState())<<" menu="<<menuState.isOpen<<" class="<<int(player.characterClass)<<" hp="<<player.health.currentHP;
    auto* p=getActiveMapPlayer();
    if(p) out<<" x="<<int(p->x)<<" y="<<int(p->y)<<" entity_hp="<<p->character.health.currentHP;
    if(gameState==GAME_DUNGEON) out<<" room="<<int(dungeon.currentRoom)<<" dir="<<int(moveDirection);
    out<<" combat="<<combat.active<<" message="<<getGameMessage();
    return out.str();
}
}}
