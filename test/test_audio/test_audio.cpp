#include <Arduino.h>
#include <unity.h>

// Compile the production engine directly, as the existing embedded suites do,
// but replace the LEDC writes with its built-in test hardware shim. Tests use
// deterministic timestamps and never need a piezo or an upload-time port.
#define AUDIO_ENGINE_TEST
#include "../../src/audio/audio.cpp"

static void resetAudioForTest()
{
    playback = AudioPlaybackState{};
    audioHardwareInit();
}

void test_every_sound_effect_has_a_command_sequence()
{
    TEST_ASSERT_NULL(getSoundSequence(SoundEffect::NONE));

    for (uint8_t value = 1;
         value < static_cast<uint8_t>(SoundEffect::COUNT);
         value++)
    {
        TEST_ASSERT_NOT_NULL(
            getSoundSequence(static_cast<SoundEffect>(value)));
    }
}

void test_play_sound_selects_and_starts_expected_sequence()
{
    resetAudioForTest();
    playSoundAt(SoundEffect::MENU_MOVE, 100);

    TEST_ASSERT_TRUE(isSoundPlaying());
    TEST_ASSERT_EQUAL_PTR(menuMoveSound, playback.sequence);
    TEST_ASSERT_EQUAL_UINT8(0, playback.commandIndex);
    TEST_ASSERT_EQUAL_UINT16(1200, playback.currentFrequency);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AudioDuty::DUTY_25),
        static_cast<uint8_t>(playback.currentDuty));
}

void test_commands_advance_and_pause_is_silent()
{
    resetAudioForTest();
    playSoundAt(SoundEffect::MENU_SELECT, 100);

    TEST_ASSERT_EQUAL_UINT16(1400, playback.currentFrequency);

    updateAudioAt(125);
    TEST_ASSERT_TRUE(isSoundPlaying());
    TEST_ASSERT_EQUAL_UINT8(1, playback.commandIndex);
    TEST_ASSERT_EQUAL_UINT16(0, playback.currentFrequency);

    updateAudioAt(133);
    TEST_ASSERT_EQUAL_UINT8(2, playback.commandIndex);
    TEST_ASSERT_EQUAL_UINT16(1800, playback.currentFrequency);

    updateAudioAt(183);
    TEST_ASSERT_FALSE(isSoundPlaying());
    TEST_ASSERT_EQUAL_UINT16(0, playback.currentFrequency);
}

void test_ascending_and_descending_sweeps_move_correctly()
{
    const AudioCommand ascending =
        audioSweep(300, 1200, 180, AudioDuty::DUTY_50);
    const AudioCommand descending =
        audioSweep(1600, 200, 250, AudioDuty::DUTY_25);

    TEST_ASSERT_EQUAL_UINT16(300, calculateSweepFrequency(ascending, 0));
    TEST_ASSERT_EQUAL_UINT16(750, calculateSweepFrequency(ascending, 90));
    TEST_ASSERT_EQUAL_UINT16(1200, calculateSweepFrequency(ascending, 180));

    TEST_ASSERT_EQUAL_UINT16(1600, calculateSweepFrequency(descending, 0));
    TEST_ASSERT_TRUE(
        calculateSweepFrequency(descending, 125) < 1600);
    TEST_ASSERT_TRUE(
        calculateSweepFrequency(descending, 125) > 200);
    TEST_ASSERT_EQUAL_UINT16(200, calculateSweepFrequency(descending, 250));
}

void test_vibrato_stays_centered_within_requested_depth()
{
    const AudioCommand vibrato =
        audioVibrato(800, 60, 8, 300, AudioDuty::DUTY_50);

    bool observedAboveCenter = false;
    bool observedBelowCenter = false;

    for (uint16_t elapsed = 0; elapsed < 250; elapsed += 5)
    {
        const uint16_t frequency =
            calculateVibratoFrequency(vibrato, elapsed);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT16(740, frequency);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(860, frequency);

        if (frequency > 800)
            observedAboveCenter = true;
        if (frequency < 800)
            observedBelowCenter = true;
    }

    TEST_ASSERT_TRUE(observedAboveCenter);
    TEST_ASSERT_TRUE(observedBelowCenter);
}

void test_noise_stays_inside_requested_frequency_range()
{
    resetAudioForTest();
    const AudioCommand noise =
        audioNoise(200, 1200, 4, 200, AudioDuty::DUTY_12_5);

    bool observedChange = false;
    uint16_t previous = nextNoiseFrequency(noise);

    for (uint8_t sample = 0; sample < 100; sample++)
    {
        const uint16_t frequency = nextNoiseFrequency(noise);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT16(200, frequency);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(1200, frequency);
        if (frequency != previous)
            observedChange = true;
        previous = frequency;
    }

    TEST_ASSERT_TRUE(observedChange);
}

void test_end_command_stops_playback()
{
    resetAudioForTest();
    playSoundAt(SoundEffect::MENU_MOVE, 500);
    TEST_ASSERT_TRUE(isSoundPlaying());

    updateAudioAt(525);
    TEST_ASSERT_FALSE(isSoundPlaying());
    TEST_ASSERT_NULL(playback.sequence);
    TEST_ASSERT_EQUAL_UINT16(0, playback.currentFrequency);
}

void test_new_sound_interrupts_current_sequence()
{
    resetAudioForTest();
    playSoundAt(SoundEffect::TITLE_THEME, 1000);
    TEST_ASSERT_TRUE(isSoundPlaying());
    TEST_ASSERT_EQUAL_PTR(titleTheme, playback.sequence);

    playSoundAt(SoundEffect::MENU_MOVE, 1010);
    TEST_ASSERT_TRUE(isSoundPlaying());
    TEST_ASSERT_EQUAL_PTR(menuMoveSound, playback.sequence);
    TEST_ASSERT_EQUAL_UINT8(0, playback.commandIndex);
    TEST_ASSERT_EQUAL_UINT16(1200, playback.currentFrequency);
}

void test_stop_sound_is_immediate_and_idempotent()
{
    resetAudioForTest();
    playSoundAt(SoundEffect::SPELL_CAST, 100);
    TEST_ASSERT_TRUE(isSoundPlaying());

    stopSound();
    TEST_ASSERT_FALSE(isSoundPlaying());
    TEST_ASSERT_EQUAL_UINT16(0, playback.currentFrequency);

    stopSound();
    TEST_ASSERT_FALSE(isSoundPlaying());
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_every_sound_effect_has_a_command_sequence);
    RUN_TEST(test_play_sound_selects_and_starts_expected_sequence);
    RUN_TEST(test_commands_advance_and_pause_is_silent);
    RUN_TEST(test_ascending_and_descending_sweeps_move_correctly);
    RUN_TEST(test_vibrato_stays_centered_within_requested_depth);
    RUN_TEST(test_noise_stays_inside_requested_frequency_range);
    RUN_TEST(test_end_command_stops_playback);
    RUN_TEST(test_new_sound_interrupts_current_sequence);
    RUN_TEST(test_stop_sound_is_immediate_and_idempotent);
    UNITY_END();
}

void loop()
{
}
