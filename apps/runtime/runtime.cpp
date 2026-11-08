#include "performance.h"
#include "native_audio.h"
#include "dungeon/bellpuzzle.h"
#include "character_saves.h"
#include "native_party.h"
#include "controller_slots.h"
#include "context_fields.h"
#include "native_rooms.h"
#include "input/buttons.h"
#include "input/inventorymenu.h"
#include "graphics/display.h"
#include "map/awareness.h"
#include "map/monsteridle.h"
#include "graphics/elementalvisual.h"
#include "graphics/actionpose.h"
#include "town/town.h"
#include "audio/audio.h"
#include "map/mapeffects.h"
#include "characters/sheet.h"
#include <limits>
#include "runtime.h"
#include "Arduino.h"
#include "SPI.h"
#include "Preferences.h"
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
using pfme::runtime::NATIVE_CONTROLLER_CAPACITY;
std::mt19937 rng;
std::mt19937 worldRng[pfme::runtime::NATIVE_WORLD_CAPACITY];
uint32_t nowMs=1;
uint32_t audioMs=1;
std::array<int,64> pins;
bool initialized=false;
bool multiController=false;
uint8_t selected=INVALID_PLAYER_ID;
bool worldEvent=false;
struct WorldEventScope {bool previous=worldEvent;explicit WorldEventScope(bool enabled=true){worldEvent=enabled;}~WorldEventScope(){worldEvent=previous;}};
struct TargetingView {
    CombatAttackType attack=COMBAT_ATTACK_NONE;
    int8_t target=-1;bool opening=false;
    AbilityID ability=ABILITY_NONE;bool scroll=false;
    ItemInstance scrollItem={ITEM_NONE,0,WEAPON_PROPERTY_NONE};
    int8_t x=-1,y=-1;Direction direction=DIR_NORTH;DamageType damage=DAMAGE_NONE;
};
bool combatViews[pfme::runtime::NATIVE_WORLD_CAPACITY]{};
#define hadCombat combatViews[nativeWorldIndex()]
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
BoundController bound[NATIVE_CONTROLLER_CAPACITY];
void tickAudioClock(uint32_t elapsed) {
    for(uint32_t tick=0;tick<elapsed;++tick) {
        ++nowMs;++audioMs;
        for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)
            if(bound[id].state)pfme::runtime::updateControllerAudio(id,audioMs);
    }
}
}
unsigned long millis() { return nowMs; }
// Native display delays are virtualized for gameplay. The audio hardware
// timeline advances only from outer elapsed ticks, never once per drawing call.
void delay(unsigned long elapsed) {nowMs+=static_cast<uint32_t>(elapsed);}
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
        for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(nativeControllerWorld(id)==nativeWorldIndex())bound[id].targeting=TargetingView{};
        hadCombat=combat.active;
    }
}
void renderController() {
    updateCurrentBellVisuals();
    pfme::perf::Scope measured(pfme::perf::metrics.render[selected<NATIVE_CONTROLLER_CAPACITY?selected:0]);
    // Other actors dirty the shared world, not this observer's private tile
    // cache. Refresh each live map at 20 Hz; input/UI redraws remain immediate.
    if(multiController && isValidController(selected) && (gameState==GAME_DUNGEON || gameState==GAME_FOREST) &&
       !menuState.isOpen &&
       uint32_t(nowMs-bound[selected].lastWorldDraw)>=50) {
        bound[selected].lastWorldDraw=nowMs;
        backgroundNeedsRedraw=true;redrawType=REDRAW_FULL;needsRedraw=true;
    }
    refreshNativePartyMenu();
    updateMonsterVisibility();updateElementalVisualEffect();updateEntityActionPoses();updateGameMessage();updateTownRest();
    if(needsRedraw)refreshDisplay();
    if(gameState==GAME_START && !menuState.isOpen)drawStartAnimation();
}
void inputStep() {
    if(!multiController) {loop();return;}
    const auto owner=selected;
    WorldEventScope scope((gameState==GAME_DUNGEON || gameState==GAME_FOREST) && !menuState.isOpen && !isCharacterSheetVisible() && !isInventoryMenuOpen() && !combat.inspecting);
    handleButtons();synchronizeCombatViews();
    if(selected!=owner)selectController(owner);
    renderController();
}
void storeController() {
    if(!isValidController(selected) || !bound[selected].state)return;
    auto* actor=nativeActor(selected);
    bound[selected].state->character=actor && (gameState==GAME_DUNGEON || gameState==GAME_FOREST)?actor->character:player;
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
void switchNativeWorldRandom(unsigned previous,unsigned next){worldRng[previous]=rng;rng=worldRng[next];}
uint32_t audioTime() {return multiController?audioMs:nowMs;}
void initializeControllers(uint32_t seed,bool socialParties) {
    initialize(seed);multiController=true;resetAccountCharacters();
    if(socialParties){for(unsigned i=0;i<NATIVE_WORLD_CAPACITY;++i)worldRng[i].seed(seed^(0x9e3779b9u*i));initializeNativeParties(seed);}
}
bool selectController(uint8_t id) {
    if(!multiController || !isValidController(id) || !bound[id].state)return false;
    if(selected==id)return true;
    storeController();
    saveNativeRoomView(dungeon);
    selectNativeWorld(nativeControllerWorld(id));
    if(isValidController(selected)) tft.exchangeSurface(bound[selected].state->surface);
    if(isValidController(selected))switchPresentation(selected,id);
    else loadPresentation(id);
    selected=id;selectAudioController(id);player=bound[id].state->character;pins=bound[id].pins;
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
    if(!multiController || !isValidController(id))return false;
    if(bound[id].state && bound[id].state!=&state)return false;
    bound[id].state=&state;bound[id].connected=true;
    if(!selectController(id))return false;
    renderController();return true;
}
void setControllerConnected(uint8_t id,bool connected) {
    if(!selectController(id))return;
    releaseInputs();productionButtonState()=ButtonInputState{};resetButtonStates();
    bound[id].connected=connected;nativePartyConnection(id,connected);
    if(connected && (!combat.active || getCurrentCombatant()!=nativeActor(id))) {bound[id].disconnectGraceUsed=false;bound[id].graceStart=0;}
    storeController();
}
bool unbindController(uint8_t id) {
    if(!selectController(id) || combat.active || !flushAccountCharacter(id))return false;
    if(!retireNativePartyMember(id))return false;
    if(nativeActor(id) && !leaveNativeRoom(dungeon))return false;
    storeController();tft.exchangeSurface(bound[id].state->surface);
    resetNativePartyCharacter(id);Preferences::retireOwner(id);bound[id]=BoundController{};selected=INVALID_PLAYER_ID;
    resetPresentation(id);retireNativeObserver(id);resetControllerAudio(id);resetAudioStream(id);selectAudioController(INVALID_PLAYER_ID);return true;
}
bool controllerInput(uint8_t id,const std::string& action,bool down) {
    if(!isValidController(id) || !bound[id].connected || !selectController(id))return false;
    const bool result=input(action,down);storeController();return result;
}
const uint16_t* controllerPixels(uint8_t id) {
    if(!isValidController(id) || !bound[id].state)return nullptr;
    return id==selected?tft.getBuffer():bound[id].state->surface.getBuffer();
}
namespace {bool broadcastingAudio=false;}
bool broadcastNativeSound(SoundEffect sound) {
    if(!multiController || broadcastingAudio || !isValidController(selected))return false;
    // Encoder/menu feedback belongs to its controller, even when the input
    // also runs inside an authoritative world-event scope.
    if(sound>=SoundEffect::MENU_MOVE && sound<=SoundEffect::BUMP)return false;
    const bool roomSound=worldEvent || (gameState==GAME_DUNGEON &&
        (sound>=SoundEffect::WALK || sound==SoundEffect::DUNGEON_THEME || sound==SoundEffect::COMBAT_THEME ||
         sound==SoundEffect::BOSS_THEME || sound==SoundEffect::VICTORY_THEME));
    const auto room=nativeActorRoom(selected);
    if(!roomSound || room==NO_ROOM)return false;
    broadcastingAudio=true;
    for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(bound[id].state && nativeActorRoom(id)==room) {
        selectAudioController(id);playSound(sound);
    }
    selectAudioController(selected);broadcastingAudio=false;return true;
}
bool broadcastNativeTones(const uint16_t* frequencies,uint8_t count,uint16_t duration,uint16_t pause,AudioDuty duty,bool& result) {
    if(!multiController || broadcastingAudio || !isValidController(selected) || nativeActorRoom(selected)==NO_ROOM)return false;
    if(!frequencies || !count || count>MAX_RUNTIME_TONE_SEQUENCE || !duration)return false;
    std::array<uint16_t,MAX_RUNTIME_TONE_SEQUENCE> notes{};
    for(unsigned i=0;i<count;++i)notes[i]=frequencies[i];
    const auto room=nativeActorRoom(selected);broadcastingAudio=true;result=false;
    for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(bound[id].state && nativeActorRoom(id)==room) {
        selectAudioController(id);const bool accepted=playToneSequence(notes.data(),count,duration,pause,duty);if(id==selected)result=accepted;
    }
    selectAudioController(selected);broadcastingAudio=false;return true;
}
bool isNativeWorldEvent() {return multiController && worldEvent;}
bool broadcastNativeMessage(const char* text) {
    if(!isNativeWorldEvent() || !isValidController(selected))return false;
    const std::string message=text?text:"";const auto owner=selected,room=nativeActorRoom(owner);
    worldEvent=false;
    for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)
        if(bound[id].state && nativeControllerWorld(id)==nativeWorldIndex() &&
           (combat.active || nativeActorRoom(id)==room) && room!=NO_ROOM) {
            selectController(id);setGameMessage(message.c_str());
        }
    selectController(owner);worldEvent=true;return true;
}
namespace {
PlayerID combatController(Entity* entity) {
    if(!entity)return INVALID_PLAYER_ID;
    if(entity->type==ENTITY_PLAYER)return entity->nativeControllerID;
    auto* pending=combat.monsterAttackTarget;
    if(pending && pending->type==ENTITY_PLAYER && nativeEntityRoom(pending)==nativeEntityRoom(entity))return pending->nativeControllerID;
    const auto room=nativeEntityRoom(entity);int distance=std::numeric_limits<int>::max();PlayerID result=INVALID_PLAYER_ID;
    for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id) {
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
    if(!isValidController(id)) {
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
    if(!multiController || clearing || !isValidController(selected))return false;
    const auto owner=selected;clearing=true;bool found=false;
    if(gameState==GAME_FOREST){clearMapEffects();clearing=false;return true;}
    for(uint8_t room=0;room<dungeon.roomCount;++room)if(bindNativeWorldRoom(dungeon,room)) {
        found=true;clearMapEffects();updateCurrentDungeonRoomCompletion(dungeon);
    }
    if(!found)clearMapEffects();
    bindNativeRoom(dungeon,owner);clearing=false;return true;
}
void scanNativeRoomAwareness() {
    const auto owner=selected,room=nativeActorRoom(selected);
    for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id) {
        auto* actor=nativeActor(id);
        if(!bound[id].state || !actor || nativeActorRoom(id)!=room || actor->character.state!=STATE_ALIVE)continue;
        selectController(id);uint8_t count=0;auto* entities=getActiveMapEntities(count);
        if(scanPlayerMonsterAwareness(actor,entities,count))break;
    }
    selectController(owner);
}
void prepareNativeCombatant(Entity* entity) {
    const auto id=combatController(entity);if(isValidController(id))selectController(id);
}
void tickNativeCombatRooms() {
    const auto owner=selected;bool visited[NATIVE_WORLD_CAPACITY][MAX_ROOMS+1]{};
    for(unsigned i=0;i<combat.combatantCount;++i) {
        const auto room=nativeEntityRoom(combat.initiativeOrder[i]);
        if(room==NO_ROOM || visited[nativeWorldIndex()][room])continue;visited[nativeWorldIndex()][room]=true;
        bool controllerFound=false;
        for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(bound[id].state && nativeActorRoom(id)==room) {
            selectController(id);controllerFound=true;break;
        }
        if(!controllerFound && room<MAX_ROOMS)bindNativeWorldRoom(dungeon,room);
        tickMapEffects();updateElementalTrapCharges();checkReinforcementAwareness();
    }
    selectController(owner);bindNativeRoom(dungeon,owner);
}
void advance(uint32_t elapsed) {
    if(!initialized) return;
    // Bounded 10 ms ticks preserve production timers and input debounce.
    elapsed=std::min(elapsed,100u);
    while(elapsed) { const auto step=std::min(elapsed,10u); if(multiController)tickAudioClock(step);else nowMs+=step; elapsed-=step;
        if(!multiController)loop();
        else {
            const auto original=selected;
            // One clock increment per world tick, independent of client count.
            bool visited[NATIVE_WORLD_CAPACITY][MAX_ROOMS+1]{};
            for(uint8_t id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(bound[id].state) {
                selectController(id);
                if(bound[id].connected){WorldEventScope inputScope((gameState==GAME_DUNGEON || gameState==GAME_FOREST) && !menuState.isOpen && !isCharacterSheetVisible() && !isInventoryMenuOpen() && !combat.inspecting);handleButtons();}
                const auto room=nativeActorRoom(id);
                if((gameState==GAME_DUNGEON || gameState==GAME_FOREST) && nativeActor(id) && nativeActor(id)->character.state==STATE_ALIVE && room<=MAX_ROOMS && !visited[nativeWorldIndex()][room]) {
                    visited[nativeWorldIndex()][room]=true;
                    WorldEventScope scope;
                    updateAwareness();updateElementalTrapCharges();updateMonsterIdleBehavior();
                    synchronizeCombatViews();
                }
                renderController();storeController();
            }
            bool combatUpdated[NATIVE_WORLD_CAPACITY]{};
            for(PlayerID id=0;id<NATIVE_CONTROLLER_CAPACITY;++id)if(bound[id].state && !combatUpdated[nativeControllerWorld(id)]) {
                selectController(id);combatUpdated[nativeWorldIndex()]=true;updateNativeCombat();
            }
            if(isValidController(original))selectController(original);
        }
     }
}
void releaseInputs() { pins[ENCODER_SW]=pins[BUTTON_A]=pins[BUTTON_B]=HIGH; }
bool input(const std::string& action,bool down) {
    if(!initialized) return false;
    if(action.size()==5 && action.compare(0,4,"key:")==0) {
        if(!isSupportedCharacterNameCharacter(action[4]))return false;
        // A valid key can arrive after the name fills or its UI closes. Treat
        // that as a harmless no-op, never a failed gameplay action/disconnect.
        if(down && textInputActive())appendCharacterNameCharacter(action[4]);
        return true;
    }
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
bool textInputActive() { return gameState==GAME_CHARACTER_CREATION && isCharacterNameEntryActive(); }
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
