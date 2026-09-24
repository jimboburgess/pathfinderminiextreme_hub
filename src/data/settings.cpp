#include "settings.h"

#include <Preferences.h>

#include "audio/audio.h"

namespace
{
constexpr const char* SETTINGS_NAMESPACE = "pfme-options";
constexpr const char* AUDIO_MUTED_KEY = "mute_audio";
constexpr const char* ENCODER_INVERTED_KEY = "invert_encoder";

bool invertEncoderRotation = false;
}

void loadGameSettings()
{
    bool audioMuted = false;
    invertEncoderRotation = false;

    Preferences preferences;
    if (preferences.begin(SETTINGS_NAMESPACE, false))
    {
        audioMuted = preferences.getBool(AUDIO_MUTED_KEY, false);
        invertEncoderRotation = preferences.getBool(
            ENCODER_INVERTED_KEY,
            false);
        preferences.end();
    }

    setAudioMuted(audioMuted);
}

void saveGameSettings()
{
    Preferences preferences;
    if (!preferences.begin(SETTINGS_NAMESPACE, false))
        return;

    preferences.putBool(AUDIO_MUTED_KEY, isAudioMuted());
    preferences.putBool(ENCODER_INVERTED_KEY, invertEncoderRotation);
    preferences.end();
}

bool isEncoderRotationInverted()
{
    return invertEncoderRotation;
}

void setEncoderRotationInverted(bool inverted)
{
    invertEncoderRotation = inverted;
}
