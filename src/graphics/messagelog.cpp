//
// Created by james on 7/25/2026.
//

#include "messagelog.h"
#include <cstring>
#include "messageLog.h"
#include "data/game.h"

constexpr uint8_t MESSAGE_LINE_MAX_CHARS = 39;
constexpr unsigned long MESSAGE_LINE_PAUSE_MS = 1400;

static char pendingMessage[128] = "";
static char currentMessage[MESSAGE_LINE_MAX_CHARS + 1] = "";
static uint8_t messagePosition = 0;
static unsigned long lineStartTime = 0;

static bool hasMoreMessageText()
{
    uint8_t nextPosition = messagePosition;

    while (pendingMessage[nextPosition] == ' ')
        nextPosition++;

    return pendingMessage[nextPosition] != '\0';
}

static void showNextMessageLine()
{
    currentMessage[0] = '\0';

    while (pendingMessage[messagePosition] == ' ')
        messagePosition++;

    uint8_t lineLength = 0;

    while (pendingMessage[messagePosition] != '\0')
    {
        uint8_t wordStart = messagePosition;

        while (pendingMessage[messagePosition] != '\0' &&
               pendingMessage[messagePosition] != ' ')
        {
            messagePosition++;
        }

        uint8_t wordLength = messagePosition - wordStart;
        uint8_t requiredLength = wordLength + (lineLength > 0 ? 1 : 0);

        if (lineLength > 0 &&
            lineLength + requiredLength > MESSAGE_LINE_MAX_CHARS)
        {
            messagePosition = wordStart;
            break;
        }

        if (lineLength == 0 && wordLength > MESSAGE_LINE_MAX_CHARS)
        {
            strncpy(currentMessage,
                    &pendingMessage[wordStart],
                    MESSAGE_LINE_MAX_CHARS);
            currentMessage[MESSAGE_LINE_MAX_CHARS] = '\0';
            messagePosition = wordStart + MESSAGE_LINE_MAX_CHARS;
            break;
        }

        if (lineLength > 0)
            currentMessage[lineLength++] = ' ';

        strncpy(&currentMessage[lineLength],
                &pendingMessage[wordStart],
                wordLength);
        lineLength += wordLength;
        currentMessage[lineLength] = '\0';

        while (pendingMessage[messagePosition] == ' ')
            messagePosition++;
    }

    lineStartTime = millis();
}

void setGameMessage(const char* message)
{
    strncpy(pendingMessage, message, sizeof(pendingMessage) - 1);
    pendingMessage[sizeof(pendingMessage) - 1] = '\0';
    messagePosition = 0;
    showNextMessageLine();
    needsRedraw = true;
}

const char* getGameMessage()
{
    return currentMessage;
}

void updateGameMessage()
{
    if (!hasMoreMessageText() ||
        millis() - lineStartTime < MESSAGE_LINE_PAUSE_MS)
    {
        return;
    }

    showNextMessageLine();
    needsRedraw = true;
}

bool isGameMessageComplete()
{
    return !hasMoreMessageText() &&
           millis() - lineStartTime >= MESSAGE_LINE_PAUSE_MS;
}

void clearGameMessage()
{
    pendingMessage[0] = '\0';
    currentMessage[0] = '\0';
    messagePosition = 0;
}
