#pragma once
#include <cstdint>
#include "controller_capacity.h"
#include <string>
#include <vector>
namespace pfme { namespace runtime {
struct ControllerState;
struct BrowserMessage { uint64_t sequence=0; std::string text; };
struct BrowserCharacterView {
    std::string name, characterClass, conditions, room;
    std::string meleeWeapon, rangedWeapon, armor, shield;
    int level=0, hp=0, maxHp=0, mp=0, maxMp=0, armorClass=0, speed=0, movementRemaining=0;
    int strength=0, dexterity=0, constitution=0, intelligence=0, wisdom=0, charisma=0;
    bool inCombat=false;
};
struct BrowserPartyMemberView {
    std::string name, characterClass, conditions, connection, room;
    int level=0, hp=0, maxHp=0, mp=0, maxMp=0;
};
struct BrowserPresentation {
    BrowserCharacterView character;
    std::vector<BrowserPartyMemberView> party;
    std::vector<BrowserMessage> messages;
    uint64_t messageCursor=0;
    bool messageGap=false;
};
// Internal migration API. The authenticated Service still retains its guard
// until gameplay, visibility and combat routing are verified end to end.
void initializeControllers(uint32_t seed,bool socialParties=false);
bool bindController(uint8_t id,ControllerState& state);
void setControllerConnected(uint8_t id,bool connected);
bool unbindController(uint8_t id);
bool selectController(uint8_t id);
bool controllerInput(uint8_t id,const std::string& action,bool down);
const uint16_t* controllerPixels(uint8_t id);
uint32_t worldTime();
uint8_t nativePartyPlayerID(uint8_t);
bool controllerBound(uint8_t id);
bool controllerConnected(uint8_t id);
bool browserPresentation(uint8_t id,uint64_t since,BrowserPresentation& out);
void recordBrowserMessage(uint8_t id,const char* text);
const char* browserConfirmAction();

// Original handheld globals permit one runtime per process, one owning session.
void initialize(uint32_t seed);
void advance(uint32_t milliseconds);
bool input(const std::string& action, bool down);
bool textInputActive();
void releaseInputs();
const uint16_t* pixels();
std::string diagnostic(); // Local tests only; never sent to other participants.
}}
