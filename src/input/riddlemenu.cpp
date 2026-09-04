#include "input/riddlemenu.h"

#include <stdio.h>

#include <Arduino.h>

#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/dungeon.h"
#include "dungeon/npcs.h"
#include "dungeon/riddles.h"
#include "dungeon/riddlepuzzle.h"
#include "graphics/messagelog.h"
#include "input/menu.h"

namespace
{
constexpr uint8_t RIDDLE_ANSWER_COUNT = 4;
constexpr uint8_t RIDDLE_BODY_HEIGHT = 72;

RiddleState* activeRiddle = nullptr;
MenuItem answerItems[RIDDLE_ANSWER_COUNT] = {};
Menu riddleMenu = {"Bertram's Riddle", answerItems, RIDDLE_ANSWER_COUNT,
                   nullptr, nullptr, nullptr, RIDDLE_BODY_HEIGHT};
char retryPayTitle[24] = {};
MenuItem retryItems[3] =
{
    {retryPayTitle, "Pay for one more guess.", MENU_RIDDLE_RETRY_PAY,
     nullptr, MENU_CLASS_ALL, ABILITY_NONE},
    {"Catch my cat", "Earn one more guess.", MENU_RIDDLE_RETRY_CAT,
     nullptr, MENU_CLASS_ALL, ABILITY_NONE},
    {"Leave", "Return to the dungeon.", MENU_RIDDLE_RETRY_LEAVE,
     nullptr, MENU_CLASS_ALL, ABILITY_NONE}
};
Menu retryMenu = {"Bertram's Terms", retryItems, 3,
                  nullptr, nullptr, nullptr, 0};

RiddleState* getRoomRiddle(const Entity& npc)
{
    if (gameState != GAME_DUNGEON || npc.type != ENTITY_NPC ||
        npc.npcID != NPC_BERTRAM_RIDDLEMAN || dungeon.currentRoom >= dungeon.roomCount)
        return nullptr;

    DungeonNPCSpawn& spawn = dungeon.rooms[dungeon.currentRoom].npcSpawn;
    if (spawn.id != npc.npcID || spawn.x != npc.x || spawn.y != npc.y)
        return nullptr;
    return &spawn.riddle;
}
}

bool openBertramRiddle(const Entity& npc)
{
    RiddleState* state = getRoomRiddle(npc);
    const RiddleDefinition* definition = state != nullptr
        ? getRiddleDefinition(state->id) : nullptr;
    if (definition == nullptr || !isValidRiddleAnswerOrder(*state)) return false;

    DungeonNPCSpawn& spawn = dungeon.rooms[dungeon.currentRoom].npcSpawn;
    if (spawn.puzzleState == RIDDLE_ROOM_KEY_PRESENTED)
    {
        setGameMessage("The key is yours. Now move along.");
        return true;
    }
    if (spawn.puzzleState == RIDDLE_ROOM_COMPLETE &&
        spawn.riddle.result != RIDDLE_ANSWERED_CORRECT)
    {
        setGameMessage("Bertram pointedly ignores the open door.");
        return true;
    }
    if (spawn.puzzleState == RIDDLE_ROOM_KEY_COLLECTED ||
        spawn.puzzleState == RIDDLE_ROOM_COMPLETE)
    {
        setGameMessage("You have answered my riddle. Next time perhaps?");
        return true;
    }

    if ((spawn.riddleFlags & RIDDLE_FLAG_CAT_JUST_CAUGHT) != 0)
    {
        spawn.riddleFlags &= static_cast<uint8_t>(~RIDDLE_FLAG_CAT_JUST_CAUGHT);
        setGameMessage("Fine. Another guess.");
        return true;
    }

    if ((spawn.riddleFlags & RIDDLE_FLAG_RETRY_REQUIRED) != 0)
    {
        if (spawn.riddleAttemptsMade >= MAX_RIDDLE_ATTEMPTS)
        {
            setGameMessage("No guesses remain.");
            return true;
        }
        const Entity* player = getPlayerEntity(
            dungeon.entities, dungeon.entityCount);
        const uint8_t level = player != nullptr ? player->character.level : 1;
        const uint16_t cost = getRiddleRetryCost(
            spawn.riddleAttemptsMade, level);
        snprintf(retryPayTitle, sizeof(retryPayTitle), "Pay %u GP", cost);
        openMenu(&retryMenu);
        return true;
    }

    if (spawn.riddleAttemptsMade >= MAX_RIDDLE_ATTEMPTS)
    {
        setGameMessage("No guesses remain.");
        return true;
    }

    if (state->result == RIDDLE_ANSWERED_CORRECT)
    {
        setGameMessage("Bertram smiles. Correct!");
        return true;
    }
    if (state->result == RIDDLE_ANSWERED_INCORRECT)
    {
        setGameMessage("Bertram shakes his head. Incorrect.");
        return true;
    }

    activeRiddle = state;
    riddleMenu.bodyText = definition->question;
    for (uint8_t index = 0; index < RIDDLE_ANSWER_COUNT; ++index)
    {
        answerItems[index] = {getDisplayedRiddleAnswer(*state, index),
                              "Choose this answer.", MENU_RIDDLE_ANSWER,
                              nullptr, MENU_CLASS_ALL, ABILITY_NONE};
    }
    openMenu(&riddleMenu);
    return true;
}

void answerActiveBertramRiddle(uint8_t displayedAnswerIndex)
{
    if (activeRiddle == nullptr || !answerRiddle(*activeRiddle, displayedAnswerIndex))
        return;

    const bool correct = activeRiddle->result == RIDDLE_ANSWERED_CORRECT;
    activeRiddle = nullptr;
    closeMenu();
    if (!handleCurrentBertramRiddleResult(correct))
        setGameMessage(correct ? "Bertram smiles. Correct!"
                               : "Bertram shakes his head. Incorrect.");
}

void payBertramRiddleRetry()
{
    Entity* player = getPlayerEntity(dungeon.entities, dungeon.entityCount);
    const RiddleRetryPaymentResult result = player != nullptr
        ? payForCurrentRiddleRetry(*player) : RIDDLE_RETRY_PAYMENT_INVALID;
    closeMenu();
    if (result == RIDDLE_RETRY_PAYMENT_GRANTED)
        setGameMessage("Fine. Another guess.");
    else if (result == RIDDLE_RETRY_PAYMENT_INSUFFICIENT_GOLD)
        setGameMessage("You do not have enough gold.");
    else
        setGameMessage("No retry is available.");
}

void startBertramCatRetry()
{
    if (dungeon.currentRoom >= dungeon.roomCount)
    {
        closeMenu();
        return;
    }
    const DungeonRoom& room = dungeon.rooms[dungeon.currentRoom];
    if ((room.npcSpawn.riddleFlags & RIDDLE_FLAG_CAT_CHASE_ACTIVE) != 0)
    {
        closeMenu();
        setGameMessage("The cat is already loose.");
        return;
    }
    const bool started = startCurrentRiddleCatChase(
        static_cast<uint8_t>(random(256)));
    closeMenu();
    setGameMessage(started
        ? "I'll give you another guess if you catch my cat."
        : "There is nowhere for the cat to run.");
}
