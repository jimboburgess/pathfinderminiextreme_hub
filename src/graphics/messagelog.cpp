//
// Created by james on 7/25/2026.
//

#include "messagelog.h"
#include <cstring>
#include "messageLog.h"

static char currentMessage[64] = "";

void setGameMessage(const char* message)
{
    strncpy(currentMessage, message, sizeof(currentMessage) - 1);
    currentMessage[sizeof(currentMessage) - 1] = '\0';
}

const char* getGameMessage()
{
    return currentMessage;
}

void clearGameMessage()
{
    currentMessage[0] = '\0';
}