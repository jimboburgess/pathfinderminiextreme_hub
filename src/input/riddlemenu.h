#ifndef PATHFINDERMINIEXTREME_025_RIDDLEMENU_H
#define PATHFINDERMINIEXTREME_025_RIDDLEMENU_H

#include <stdint.h>

struct Entity;

// Opens the persistent Bertram riddle assigned to the current room.
bool openBertramRiddle(const Entity& npc);

// Handles one of the four displayed (already shuffled) answer rows.
void answerActiveBertramRiddle(uint8_t displayedAnswerIndex);

// Retry-menu actions used after an incorrect answer.
void payBertramRiddleRetry();
void startBertramCatRetry();

#endif
