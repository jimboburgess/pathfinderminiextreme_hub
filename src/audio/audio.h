//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_AUDIO_H
#define PATHFINDERMINIEXTREME_025_AUDIO_H

#include <Arduino.h>

// Existing game code should continue to use:
// playSound(SoundEffect::SOUND_NAME);
enum class SoundEffect : uint8_t
{
    NONE,

    // UI
    MENU_MOVE,
    MENU_SELECT,
    MENU_BACK,
    ERROR,
    BUMP,

    // Music
    TITLE_THEME,
    TOWN_THEME,
    DUNGEON_THEME,
    FOREST_THEME,
    COMBAT_THEME,
    BOSS_THEME,
    VICTORY_THEME,

    // Player Actions
    WALK,
    DOOR_OPEN,
    DOOR_LOCKED,
    CHEST_OPEN,
    ITEM_PICKUP,
    POTION,
    DEFEND,

    // Combat
    ATTACK,
    BOW_FIRE,
    MISS,
    CRIT,
    CRIT_FAIL,
    BLOCK,
    DODGE,

    // Magic
    SPELL_CAST,
    SPELL_HIT,
    SPELL_HEAL,
    SPELL_FAIL,

    // Monsters
    GOBLIN_ALERT,
    GOBLIN_ATTACK,
    ENEMY_HIT,
    ENEMY_DIE,

    // Character
    PLAYER_HIT,
    PLAYER_DIE,
    LEVEL_UP,

    // World
    TRAP,
    SECRET_FOUND,
    QUEST_COMPLETE,

    // End Game
    VICTORY,
    GAME_OVER,

    COUNT
};

enum class AudioCommandType : uint8_t
{
    TONE,
    PAUSE,
    SWEEP,
    VIBRATO,
    NOISE,
    END
};

enum class AudioDuty : uint8_t
{
    DUTY_12_5,
    DUTY_25,
    DUTY_50,
    DUTY_75
};

// Compact, fixed-size description interpreted by the single-channel playback
// engine. The meaning of frequency2 and parameter depends on the command:
//
// SWEEP:  frequency -> frequency2, parameter unused
// VIBRATO: frequency=center, frequency2=depth, parameter=rate in Hz
// NOISE:  frequency=min, frequency2=max, parameter=update interval in ms
struct AudioCommand
{
    AudioCommandType type;
    AudioDuty duty;
    uint16_t durationMs;
    uint16_t frequency;
    uint16_t frequency2;
    uint16_t parameter;
};

static_assert(sizeof(AudioCommand) == 10,
              "AudioCommand should remain compact for ESP32 flash storage.");

constexpr AudioCommand audioTone(
    uint16_t frequency,
    uint16_t durationMs,
    AudioDuty duty = AudioDuty::DUTY_50)
{
    return {AudioCommandType::TONE, duty, durationMs, frequency, 0, 0};
}

constexpr AudioCommand audioPause(uint16_t durationMs)
{
    return {
        AudioCommandType::PAUSE,
        AudioDuty::DUTY_50,
        durationMs,
        0,
        0,
        0};
}

constexpr AudioCommand audioSweep(
    uint16_t startFrequency,
    uint16_t endFrequency,
    uint16_t durationMs,
    AudioDuty duty = AudioDuty::DUTY_50)
{
    return {
        AudioCommandType::SWEEP,
        duty,
        durationMs,
        startFrequency,
        endFrequency,
        0};
}

constexpr AudioCommand audioVibrato(
    uint16_t centerFrequency,
    uint16_t depth,
    uint16_t rateHz,
    uint16_t durationMs,
    AudioDuty duty = AudioDuty::DUTY_50)
{
    return {
        AudioCommandType::VIBRATO,
        duty,
        durationMs,
        centerFrequency,
        depth,
        rateHz};
}

constexpr AudioCommand audioNoise(
    uint16_t minFrequency,
    uint16_t maxFrequency,
    uint16_t updateIntervalMs,
    uint16_t durationMs,
    AudioDuty duty = AudioDuty::DUTY_50)
{
    return {
        AudioCommandType::NOISE,
        duty,
        durationMs,
        minFrequency,
        maxFrequency,
        updateIntervalMs};
}

constexpr AudioCommand audioEnd()
{
    return {
        AudioCommandType::END,
        AudioDuty::DUTY_50,
        0,
        0,
        0,
        0};
}

void initAudio();
void playSound(SoundEffect sound);
void updateAudio();
void stopSound();
bool isSoundPlaying();

// Runtime-wide audio setting. Muting immediately stops any active sound and
// suppresses later effects until audio is enabled again.
void setAudioMuted(bool muted);
bool isAudioMuted();
void toggleAudioMuted();

#endif // PATHFINDERMINIEXTREME_025_AUDIO_H
