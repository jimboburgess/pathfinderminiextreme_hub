#include <unity.h>

#include "../../src/dungeon/riddles.cpp"

void setUp() {}
void tearDown() {}

void test_all_twenty_riddles_are_defined()
{
    TEST_ASSERT_EQUAL_UINT8(20, RIDDLE_COUNT);
    for (uint8_t index = 0; index < RIDDLE_COUNT; ++index)
    {
        const RiddleDefinition* definition =
            getRiddleDefinition(static_cast<RiddleID>(index));
        TEST_ASSERT_NOT_NULL(definition);
        TEST_ASSERT_NOT_NULL(definition->question);
        TEST_ASSERT_EQUAL_UINT8(0, definition->correctAnswerIndex);
        for (const char* answer : definition->answers) TEST_ASSERT_NOT_NULL(answer);
    }
}

void test_answer_shuffle_is_a_stable_permutation()
{
    RiddleState state;
    const uint8_t rolls[3] = {0, 1, 0};
    initializeRiddleState(state, RIDDLE_MOUNTAIN, rolls);
    TEST_ASSERT_TRUE(isValidRiddleAnswerOrder(state));
    TEST_ASSERT_EQUAL_STRING("Mountain", getDisplayedRiddleAnswer(state, 3));
}

void test_correct_and_incorrect_results_are_persistent()
{
    const uint8_t rolls[3] = {0, 1, 0};
    RiddleState correct;
    initializeRiddleState(correct, RIDDLE_MOUNTAIN, rolls);
    TEST_ASSERT_TRUE(answerRiddle(correct, 3));
    TEST_ASSERT_EQUAL_UINT8(RIDDLE_ANSWERED_CORRECT, correct.result);
    TEST_ASSERT_FALSE(answerRiddle(correct, 0));

    RiddleState incorrect;
    initializeRiddleState(incorrect, RIDDLE_FIRE, rolls);
    TEST_ASSERT_TRUE(answerRiddle(incorrect, 0));
    TEST_ASSERT_EQUAL_UINT8(RIDDLE_ANSWERED_INCORRECT, incorrect.result);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_all_twenty_riddles_are_defined);
    RUN_TEST(test_answer_shuffle_is_a_stable_permutation);
    RUN_TEST(test_correct_and_incorrect_results_are_persistent);
    UNITY_END();
}

void loop() {}
