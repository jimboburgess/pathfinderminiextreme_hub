//
// Created by james on 7/12/2026.
//

#include <Arduino.h>
#include "audio/audio.h"
#include "config.h"

namespace
{
// ---------------------------------------------------------------------------
// Sound definitions
// ---------------------------------------------------------------------------

constexpr AudioCommand menuMoveSound[] =
{
    audioTone(1200, 25, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand menuSelectSound[] =
{
    audioTone(1400, 25, AudioDuty::DUTY_25),
    audioPause(8),
    audioTone(1800, 50, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand menuBackSound[] =
{
    audioTone(1800, 25, AudioDuty::DUTY_25),
    audioTone(1300, 40, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand errorSound[] =
{
    audioSweep(320, 150, 180, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand bumpSound[] =
{
    audioSweep(260, 120, 70, AudioDuty::DUTY_75),
    audioEnd()
};

// Music remains intentionally monophonic. Notes and pauses are represented by
// the same command stream as sound effects, making future articulation changes
// data-only rather than new playback code.
    constexpr AudioCommand titleTheme[] =
    {
        audioPause(500),

        audioTone(523, 375, AudioDuty::DUTY_25),
        audioTone(523, 125, AudioDuty::DUTY_25),

        // Snare
        audioNoise(500, 2200, 20, 2),
        audioTone(659, 230, AudioDuty::DUTY_25),

        audioTone(698, 500, AudioDuty::DUTY_25),

        // Snare
        audioNoise(500, 2200, 20, 2),
        audioTone(784, 230, AudioDuty::DUTY_25),

        audioTone(784, 125, AudioDuty::DUTY_25),
        audioPause(125),

        audioTone(987, 250, AudioDuty::DUTY_25),
        audioTone(880, 250, AudioDuty::DUTY_25),

        // Snare
        audioNoise(500, 2200, 20, 2),
        audioTone(880, 105, AudioDuty::DUTY_25),

        audioPause(60),

        // Hi-hat
        audioNoise(2500, 5000, 12, 1),
        audioTone(880, 113, AudioDuty::DUTY_25),

        audioTone(698, 125, AudioDuty::DUTY_25),
        audioPause(60),

        audioTone(698, 200, AudioDuty::DUTY_25),
        audioPause(60),

        // Snare
        audioNoise(500, 2200, 20, 2),
        audioTone(784, 480, AudioDuty::DUTY_25),

        audioEnd()
    };

constexpr AudioCommand townTheme[] =
{
    audioTone(784, 120, AudioDuty::DUTY_25),
    audioTone(880, 120, AudioDuty::DUTY_25),
    audioTone(988, 180, AudioDuty::DUTY_25),
    audioTone(880, 120, AudioDuty::DUTY_25),
    audioTone(784, 250, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand dungeonTheme[] =
{
    audioTone(49, 300, AudioDuty::DUTY_50),
    audioPause(40),
    audioTone(44, 300, AudioDuty::DUTY_50),
    audioPause(50),
    audioTone(40, 450, AudioDuty::DUTY_50),
    audioPause(10),
    audioTone(40, 450, AudioDuty::DUTY_50),
    audioPause(10),
    audioTone(40, 450, AudioDuty::DUTY_50),
    audioPause(10),
    audioTone(40, 450, AudioDuty::DUTY_50),
    audioPause(10),
    audioTone(49, 300, AudioDuty::DUTY_50),
    audioPause(40),
    audioEnd()
};

constexpr AudioCommand forestTheme[] =
{
    audioTone(523, 140, AudioDuty::DUTY_25),
    audioTone(659, 140, AudioDuty::DUTY_25),
    audioTone(587, 140, AudioDuty::DUTY_25),
    audioTone(784, 180, AudioDuty::DUTY_25),
    audioTone(659, 220, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand combatTheme[] =
{
    audioTone(440, 100, AudioDuty::DUTY_25),
    audioTone(523, 100, AudioDuty::DUTY_25),
    audioTone(659, 100, AudioDuty::DUTY_25),
    audioTone(523, 100, AudioDuty::DUTY_25),
    audioTone(784, 180, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand bossTheme[] =
{
    audioTone(220, 180, AudioDuty::DUTY_50),
    audioTone(294, 180, AudioDuty::DUTY_50),
    audioTone(196, 180, AudioDuty::DUTY_50),
    audioTone(330, 250, AudioDuty::DUTY_50),
    audioTone(147, 350, AudioDuty::DUTY_50),
    audioEnd()
};

constexpr AudioCommand victoryTheme[] =
{
    audioTone(523, 100, AudioDuty::DUTY_25),
    audioTone(659, 100, AudioDuty::DUTY_25),
    audioTone(784, 120, AudioDuty::DUTY_25),
    audioTone(1046, 200, AudioDuty::DUTY_25),
    audioTone(1318, 350, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand walkSound[] =
{
    audioNoise(180, 320, 3, 18, AudioDuty::DUTY_12_5),
    audioEnd()
};

constexpr AudioCommand doorOpenSound[] =
{
    audioSweep(260, 780, 120, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand doorLockedSound[] =
{
    audioTone(280, 45, AudioDuty::DUTY_75),
    audioPause(35),
    audioTone(240, 65, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand chestOpenSound[] =
{
    audioTone(600, 40, AudioDuty::DUTY_25),
    audioPause(12),
    audioTone(800, 40, AudioDuty::DUTY_25),
    audioPause(12),
    audioTone(1000, 80, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand itemPickupSound[] =
{
    audioSweep(900, 1600, 80, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand potionSound[] =
{
    audioNoise(450, 900, 18, 90, AudioDuty::DUTY_25),
    audioVibrato(1050, 120, 10, 170, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand defendSound[] =
{
    audioNoise(1800, 3000, 2, 25, AudioDuty::DUTY_25),
    audioTone(1800, 60, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand attackSound[] =
{
    audioSweep(1900, 500, 70, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand bowFireSound[] =
{
    audioSweep(2200, 750, 65, AudioDuty::DUTY_12_5),
    audioEnd()
};

constexpr AudioCommand missSound[] =
{
    audioSweep(950, 350, 100, AudioDuty::DUTY_12_5),
    audioEnd()
};

constexpr AudioCommand critSound[] =
{
    audioNoise(1500, 3200, 2, 35, AudioDuty::DUTY_25),
    audioTone(1800, 35, AudioDuty::DUTY_25),
    audioTone(2400, 35, AudioDuty::DUTY_25),
    audioTone(3200, 90, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand critFailSound[] =
{
    audioVibrato(260, 90, 12, 260, AudioDuty::DUTY_75),
    audioSweep(260, 110, 320, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand blockSound[] =
{
    audioNoise(1700, 3200, 2, 30, AudioDuty::DUTY_25),
    audioTone(2000, 45, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand dodgeSound[] =
{
    audioSweep(700, 2200, 70, AudioDuty::DUTY_12_5),
    audioEnd()
};

constexpr AudioCommand spellCastSound[] =
{
    audioSweep(600, 1500, 140, AudioDuty::DUTY_25),
    audioVibrato(1600, 90, 12, 90, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand spellHitSound[] =
{
    audioNoise(1200, 2600, 2, 55, AudioDuty::DUTY_25),
    audioSweep(900, 240, 70, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand spellHealSound[] =
{
    audioTone(600, 40, AudioDuty::DUTY_25),
    audioTone(800, 40, AudioDuty::DUTY_25),
    audioTone(1000, 40, AudioDuty::DUTY_25),
    audioVibrato(1200, 45, 7, 100, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand spellFailSound[] =
{
    audioVibrato(450, 80, 10, 160, AudioDuty::DUTY_75),
    audioSweep(420, 180, 100, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand goblinAlertSound[] =
{
    audioTone(450, 35, AudioDuty::DUTY_75),
    audioTone(600, 35, AudioDuty::DUTY_75),
    audioTone(430, 60, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand goblinAttackSound[] =
{
    audioNoise(250, 900, 3, 70, AudioDuty::DUTY_75),
    audioSweep(650, 200, 70, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand enemyHitSound[] =
{
    audioNoise(300, 1000, 2, 45, AudioDuty::DUTY_75),
    audioTone(300, 35, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand enemyDieSound[] =
{
    audioSweep(1000, 180, 260, AudioDuty::DUTY_75),
    audioNoise(100, 500, 10, 100, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand playerHitSound[] =
{
    audioNoise(250, 800, 3, 45, AudioDuty::DUTY_75),
    audioSweep(650, 300, 70, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand playerDieSound[] =
{
    audioTone(500, 120, AudioDuty::DUTY_50),
    audioTone(400, 150, AudioDuty::DUTY_50),
    audioTone(300, 200, AudioDuty::DUTY_50),
    audioSweep(250, 90, 320, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand levelUpSound[] =
{
    audioTone(523, 80, AudioDuty::DUTY_25),
    audioPause(12),
    audioTone(659, 80, AudioDuty::DUTY_25),
    audioPause(12),
    audioTone(784, 80, AudioDuty::DUTY_25),
    audioPause(12),
    audioTone(1046, 200, AudioDuty::DUTY_25),
    audioTone(1318, 300, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand trapSound[] =
{
    audioNoise(900, 2400, 2, 45, AudioDuty::DUTY_25),
    audioSweep(1800, 300, 110, AudioDuty::DUTY_75),
    audioEnd()
};

constexpr AudioCommand secretFoundSound[] =
{
    audioTone(900, 40, AudioDuty::DUTY_25),
    audioTone(1100, 40, AudioDuty::DUTY_25),
    audioTone(1400, 80, AudioDuty::DUTY_25),
    audioVibrato(1700, 50, 8, 120, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand questCompleteSound[] =
{
    audioTone(523, 80, AudioDuty::DUTY_25),
    audioTone(659, 80, AudioDuty::DUTY_25),
    audioTone(784, 80, AudioDuty::DUTY_25),
    audioTone(988, 120, AudioDuty::DUTY_25),
    audioTone(1318, 220, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand victorySound[] =
{
    audioTone(523, 120, AudioDuty::DUTY_25),
    audioTone(659, 120, AudioDuty::DUTY_25),
    audioTone(784, 180, AudioDuty::DUTY_25),
    audioTone(1046, 350, AudioDuty::DUTY_25),
    audioEnd()
};

constexpr AudioCommand gameOverSound[] =
{
    audioTone(784, 150, AudioDuty::DUTY_50),
    audioTone(698, 150, AudioDuty::DUTY_50),
    audioTone(587, 200, AudioDuty::DUTY_50),
    audioVibrato(523, 35, 5, 180, AudioDuty::DUTY_50),
    audioSweep(500, 160, 300, AudioDuty::DUTY_75),
    audioEnd()
};

const AudioCommand* getSoundSequence(SoundEffect sound)
{
    switch (sound)
    {
        case SoundEffect::NONE:
            return nullptr;
        case SoundEffect::MENU_MOVE:
            return menuMoveSound;
        case SoundEffect::MENU_SELECT:
            return menuSelectSound;
        case SoundEffect::MENU_BACK:
            return menuBackSound;
        case SoundEffect::ERROR:
            return errorSound;
        case SoundEffect::BUMP:
            return bumpSound;
        case SoundEffect::TITLE_THEME:
            return titleTheme;
        case SoundEffect::TOWN_THEME:
            return townTheme;
        case SoundEffect::DUNGEON_THEME:
            return dungeonTheme;
        case SoundEffect::FOREST_THEME:
            return forestTheme;
        case SoundEffect::COMBAT_THEME:
            return combatTheme;
        case SoundEffect::BOSS_THEME:
            return bossTheme;
        case SoundEffect::VICTORY_THEME:
            return victoryTheme;
        case SoundEffect::WALK:
            return walkSound;
        case SoundEffect::DOOR_OPEN:
            return doorOpenSound;
        case SoundEffect::DOOR_LOCKED:
            return doorLockedSound;
        case SoundEffect::CHEST_OPEN:
            return chestOpenSound;
        case SoundEffect::ITEM_PICKUP:
            return itemPickupSound;
        case SoundEffect::POTION:
            return potionSound;
        case SoundEffect::DEFEND:
            return defendSound;
        case SoundEffect::ATTACK:
            return attackSound;
        case SoundEffect::BOW_FIRE:
            return bowFireSound;
        case SoundEffect::MISS:
            return missSound;
        case SoundEffect::CRIT:
            return critSound;
        case SoundEffect::CRIT_FAIL:
            return critFailSound;
        case SoundEffect::BLOCK:
            return blockSound;
        case SoundEffect::DODGE:
            return dodgeSound;
        case SoundEffect::SPELL_CAST:
            return spellCastSound;
        case SoundEffect::SPELL_HIT:
            return spellHitSound;
        case SoundEffect::SPELL_HEAL:
            return spellHealSound;
        case SoundEffect::SPELL_FAIL:
            return spellFailSound;
        case SoundEffect::GOBLIN_ALERT:
            return goblinAlertSound;
        case SoundEffect::GOBLIN_ATTACK:
            return goblinAttackSound;
        case SoundEffect::ENEMY_HIT:
            return enemyHitSound;
        case SoundEffect::ENEMY_DIE:
            return enemyDieSound;
        case SoundEffect::PLAYER_HIT:
            return playerHitSound;
        case SoundEffect::PLAYER_DIE:
            return playerDieSound;
        case SoundEffect::LEVEL_UP:
            return levelUpSound;
        case SoundEffect::TRAP:
            return trapSound;
        case SoundEffect::SECRET_FOUND:
            return secretFoundSound;
        case SoundEffect::QUEST_COMPLETE:
            return questCompleteSound;
        case SoundEffect::VICTORY:
            return victorySound;
        case SoundEffect::GAME_OVER:
            return gameOverSound;
        case SoundEffect::COUNT:
            return nullptr;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// Playback state
// ---------------------------------------------------------------------------

constexpr uint8_t AUDIO_LEDC_CHANNEL = 0;
constexpr uint8_t AUDIO_LEDC_RESOLUTION = 10;
constexpr uint16_t AUDIO_INITIAL_FREQUENCY = 1000;
constexpr uint16_t MODULATION_INTERVAL_MS = 8;
constexpr uint8_t MAX_COMMANDS_PER_UPDATE = 64;

struct AudioPlaybackState
{
    const AudioCommand* sequence = nullptr;
    uint8_t commandIndex = 0;
    uint32_t commandStartMs = 0;
    uint32_t lastModulationMs = 0;
    uint16_t currentFrequency = 0;
    uint16_t noiseLfsr = 0xACE1u;
    AudioDuty currentDuty = AudioDuty::DUTY_50;
    bool commandStarted = false;
    bool active = false;
};

AudioPlaybackState playback;
uint16_t hardwareFrequency = 0;
AudioDuty hardwareDuty = AudioDuty::DUTY_50;
bool audioMuted = false;

uint16_t getDutyValue(AudioDuty duty)
{
    switch (duty)
    {
        case AudioDuty::DUTY_12_5:
            return 128;
        case AudioDuty::DUTY_25:
            return 256;
        case AudioDuty::DUTY_50:
            return 512;
        case AudioDuty::DUTY_75:
            return 768;
    }

    return 512;
}

// ---------------------------------------------------------------------------
// ESP32 LEDC hardware layer
// ---------------------------------------------------------------------------

void audioHardwareInit()
{
#if !defined(AUDIO_ENGINE_TEST)
    ledcSetup(
        AUDIO_LEDC_CHANNEL,
        AUDIO_INITIAL_FREQUENCY,
        AUDIO_LEDC_RESOLUTION);
    ledcAttachPin(PIEZO_PIN, AUDIO_LEDC_CHANNEL);
    ledcWrite(AUDIO_LEDC_CHANNEL, 0);
#endif

    hardwareFrequency = 0;
    hardwareDuty = AudioDuty::DUTY_50;
}

void audioSilence()
{
#if !defined(AUDIO_ENGINE_TEST)
    ledcWrite(AUDIO_LEDC_CHANNEL, 0);
#endif

    hardwareFrequency = 0;
    playback.currentFrequency = 0;
}

void audioSetFrequency(uint16_t frequency, AudioDuty duty)
{
    if (frequency == 0)
    {
        audioSilence();
        return;
    }

    if (frequency != hardwareFrequency)
    {
#if !defined(AUDIO_ENGINE_TEST)
        // Arduino-ESP32 2.x exposes the channel-based LEDC API. Calling
        // ledcWriteTone() changes the timer frequency and restores 50% duty;
        // ledcWrite() immediately applies the requested retro duty cycle.
        ledcWriteTone(AUDIO_LEDC_CHANNEL, frequency);
#endif
        hardwareFrequency = frequency;
    }

    if (frequency != playback.currentFrequency || duty != hardwareDuty)
    {
#if !defined(AUDIO_ENGINE_TEST)
        ledcWrite(AUDIO_LEDC_CHANNEL, getDutyValue(duty));
#endif
        hardwareDuty = duty;
    }

    playback.currentFrequency = frequency;
    playback.currentDuty = duty;
}

// ---------------------------------------------------------------------------
// Integer modulation helpers
// ---------------------------------------------------------------------------

uint16_t calculateSweepFrequency(
    const AudioCommand& command,
    uint32_t elapsedMs)
{
    if (command.durationMs == 0 || elapsedMs >= command.durationMs)
        return command.frequency2;

    const int32_t delta =
        static_cast<int32_t>(command.frequency2) - command.frequency;
    const int32_t offset = static_cast<int32_t>(
        (static_cast<int64_t>(delta) * elapsedMs) / command.durationMs);
    const int32_t frequency =
        static_cast<int32_t>(command.frequency) + offset;

    if (frequency < 1)
        return 1;
    if (frequency > 65535)
        return 65535;
    return static_cast<uint16_t>(frequency);
}

uint16_t calculateVibratoFrequency(
    const AudioCommand& command,
    uint32_t elapsedMs)
{
    const uint16_t center = command.frequency;
    const uint16_t depth = command.frequency2;
    const uint16_t rateHz = command.parameter;

    if (depth == 0 || rateHz == 0)
        return center;

    uint32_t periodMs = 1000u / rateHz;
    if (periodMs == 0)
        periodMs = 1;

    const uint32_t phase =
        ((elapsedMs % periodMs) * 1024u) / periodMs;
    int32_t offset = 0;

    if (phase < 256u)
    {
        offset = static_cast<int32_t>(depth) * phase / 256;
    }
    else if (phase < 512u)
    {
        offset = static_cast<int32_t>(depth) * (512u - phase) / 256;
    }
    else if (phase < 768u)
    {
        offset = -static_cast<int32_t>(depth) * (phase - 512u) / 256;
    }
    else
    {
        offset = -static_cast<int32_t>(depth) * (1024u - phase) / 256;
    }

    const int32_t frequency = static_cast<int32_t>(center) + offset;
    if (frequency < 1)
        return 1;
    if (frequency > 65535)
        return 65535;
    return static_cast<uint16_t>(frequency);
}

uint16_t nextNoiseFrequency(const AudioCommand& command)
{
    // 16-bit Galois LFSR. A non-zero state is retained between noise
    // commands so repeated effects do not begin with an identical texture.
    const uint16_t bit = playback.noiseLfsr & 1u;
    playback.noiseLfsr >>= 1u;
    if (bit != 0)
        playback.noiseLfsr ^= 0xB400u;
    if (playback.noiseLfsr == 0)
        playback.noiseLfsr = 0xACE1u;

    uint16_t minimum = command.frequency;
    uint16_t maximum = command.frequency2;
    if (minimum > maximum)
    {
        const uint16_t swap = minimum;
        minimum = maximum;
        maximum = swap;
    }

    const uint32_t range =
        static_cast<uint32_t>(maximum) - minimum + 1u;
    return static_cast<uint16_t>(
        minimum + (static_cast<uint32_t>(playback.noiseLfsr) % range));
}

// ---------------------------------------------------------------------------
// Command playback engine
// ---------------------------------------------------------------------------

void finishPlayback()
{
    audioSilence();
    playback.sequence = nullptr;
    playback.commandIndex = 0;
    playback.commandStarted = false;
    playback.active = false;
}

void beginCurrentCommand(uint32_t startMs)
{
    const AudioCommand& command =
        playback.sequence[playback.commandIndex];

    playback.commandStartMs = startMs;
    playback.lastModulationMs = startMs;
    playback.commandStarted = true;

    switch (command.type)
    {
        case AudioCommandType::TONE:
            audioSetFrequency(command.frequency, command.duty);
            break;

        case AudioCommandType::PAUSE:
            audioSilence();
            break;

        case AudioCommandType::SWEEP:
            audioSetFrequency(command.frequency, command.duty);
            break;

        case AudioCommandType::VIBRATO:
            audioSetFrequency(command.frequency, command.duty);
            break;

        case AudioCommandType::NOISE:
            audioSetFrequency(nextNoiseFrequency(command), command.duty);
            break;

        case AudioCommandType::END:
            finishPlayback();
            break;
    }
}

void updateCurrentModulation(
    const AudioCommand& command,
    uint32_t now,
    uint32_t elapsedMs)
{
    uint16_t intervalMs = MODULATION_INTERVAL_MS;
    if (command.type == AudioCommandType::NOISE)
        intervalMs = command.parameter == 0 ? 1 : command.parameter;

    if (now - playback.lastModulationMs < intervalMs)
        return;

    playback.lastModulationMs = now;

    switch (command.type)
    {
        case AudioCommandType::SWEEP:
            audioSetFrequency(
                calculateSweepFrequency(command, elapsedMs),
                command.duty);
            break;

        case AudioCommandType::VIBRATO:
            audioSetFrequency(
                calculateVibratoFrequency(command, elapsedMs),
                command.duty);
            break;

        case AudioCommandType::NOISE:
            audioSetFrequency(nextNoiseFrequency(command), command.duty);
            break;

        default:
            break;
    }
}

void updateAudioAt(uint32_t now)
{
    if (!playback.active || playback.sequence == nullptr)
        return;

    uint32_t nextCommandStart = now;

    // Catch up cleanly if one frame spans several very short commands. The
    // guard also makes malformed command data fail silent instead of looping.
    for (uint8_t commandCount = 0;
         commandCount < MAX_COMMANDS_PER_UPDATE && playback.active;
         commandCount++)
    {
        if (!playback.commandStarted)
            beginCurrentCommand(nextCommandStart);

        if (!playback.active)
            return;

        const AudioCommand& command =
            playback.sequence[playback.commandIndex];
        const uint32_t elapsedMs = now - playback.commandStartMs;

        if (command.durationMs == 0 || elapsedMs >= command.durationMs)
        {
            nextCommandStart = playback.commandStartMs + command.durationMs;
            playback.commandIndex++;
            playback.commandStarted = false;
            continue;
        }

        updateCurrentModulation(command, now, elapsedMs);
        return;
    }

    if (playback.active)
        finishPlayback();
}

void playSoundAt(SoundEffect sound, uint32_t now)
{
    // A new sound always interrupts the previous single-channel sequence.
    finishPlayback();

    const AudioCommand* sequence = getSoundSequence(sound);
    if (sequence == nullptr)
        return;

    playback.sequence = sequence;
    playback.commandIndex = 0;
    playback.commandStarted = false;
    playback.active = true;
    updateAudioAt(now);
}
} // namespace

void initAudio()
{
    playback = AudioPlaybackState{};
    audioMuted = false;
    audioHardwareInit();
}

void playSound(SoundEffect sound)
{
    if (audioMuted)
        return;

    playSoundAt(sound, millis());
}

void updateAudio()
{
    updateAudioAt(millis());
}

void stopSound()
{
    finishPlayback();
}

bool isSoundPlaying()
{
    return playback.active;
}

void setAudioMuted(bool muted)
{
    audioMuted = muted;

    if (audioMuted)
        stopSound();
}

bool isAudioMuted()
{
    return audioMuted;
}

void toggleAudioMuted()
{
    setAudioMuted(!audioMuted);
}
