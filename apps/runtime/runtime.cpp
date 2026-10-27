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
void advance(uint32_t elapsed) {
    if(!initialized) return;
    // Bounded 10 ms ticks preserve production timers and input debounce.
    elapsed=std::min(elapsed,100u);
    while(elapsed) { const auto step=std::min(elapsed,10u); nowMs+=step; elapsed-=step; loop(); }
}
void releaseInputs() { pins[ENCODER_SW]=pins[BUTTON_A]=pins[BUTTON_B]=HIGH; }
bool input(const std::string& action,bool down) {
    if(!initialized) return false;
    if(action=="release") { releaseInputs(); loop(); return true; }
    if(action=="cw" || action=="ccw") {
        if(!down) return true;
        const int cw[]={2,0,1,3}, ccw[]={1,0,2,3};
        for(int phase: action=="cw"?cw:ccw) {
            pins[ENCODER_CLK]=(phase&2)?HIGH:LOW; pins[ENCODER_DT]=(phase&1)?HIGH:LOW; loop();
        }
        return true;
    }
    const int pin=action=="encoder"?ENCODER_SW:action=="a"?BUTTON_A:action=="b"?BUTTON_B:-1;
    if(pin<0) return false;
    pins[pin]=down?LOW:HIGH; loop(); return true;
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
