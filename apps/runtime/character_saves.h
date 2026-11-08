#pragma once
#include <cstdint>
struct Character;
namespace pfme { namespace runtime {
// Internal API: controller and account provenance are established by Service.
bool accountCharactersEnabled();
const char* saveAccountCharacter(uint8_t slot);
const char* loadAccountCharacter(uint8_t slot,Character& loaded);
const char* newAccountCharacter();
bool flushAccountCharacter(uint8_t controller);
void autosaveAccountCharacters(uint32_t elapsed);
void resetAccountCharacters();
}}
