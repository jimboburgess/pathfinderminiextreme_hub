#pragma once
#include "encoderdecoder.h"

// One original handheld input decoder per controller. No Arduino/GPIO dependency.
// Platform wrappers supply pin levels and the shared clock; thresholds and state
// transitions are the production logic, shared by ESP32 and native players.
struct ButtonInputState
{
    QuadratureDecoderState encoderDecoder;

    bool selectLast = true;
    bool aLast = true;
    bool bLast = true;

    bool encoderSelectReady = true;
    bool encoderSelectSuppressed = false;
    unsigned long encoderSelectReleasedAt = 0;
    unsigned long encoderSelectSuppressedAt = 0;

    bool aSuppressed = false;
    bool bSuppressed = false;
    unsigned long aSuppressedAt = 0;
    unsigned long bSuppressedAt = 0;

    unsigned long encoderPressStart = 0;
    bool encoderLongPressHandled = false;

    static constexpr unsigned long LONG_PRESS_TIME = 750;
    static constexpr unsigned long ENCODER_SELECT_RELEASE_DEBOUNCE_MS = 25;
    static constexpr unsigned long MENU_INPUT_GUARD_MS = 300;

    void suppressEncoderSelectUntilRelease(bool selectHigh, unsigned long nowMs)
    {
        bool now = selectHigh;

        // Require a stable release before the select switch can be armed again.
        // This absorbs both a held click and its mechanical bounce.
        selectLast = now;
        encoderSelectReady = false;
        encoderSelectSuppressed = true;
        encoderSelectSuppressedAt = nowMs;

        if (now == true)
            encoderSelectReleasedAt = encoderSelectSuppressedAt;
    }

    bool encoderPressed(bool selectHigh, unsigned long nowMs)
    {
        bool now = selectHigh;
        unsigned long nowMillis = nowMs;

        if (now == true)
        {
            if (selectLast == false)
                encoderSelectReleasedAt = nowMillis;

            if (!encoderSelectReady &&
                nowMillis - encoderSelectReleasedAt >=
                    ENCODER_SELECT_RELEASE_DEBOUNCE_MS)
            {
                encoderSelectReady = true;
            }
        }

        if (encoderSelectSuppressed)
        {
            selectLast = now;

            if (now == true && encoderSelectReady &&
                nowMillis - encoderSelectSuppressedAt >=
                    MENU_INPUT_GUARD_MS)
            {
                encoderSelectSuppressed = false;
            }

            return false;
        }

        bool pressed = now == false && selectLast == true &&
                       encoderSelectReady;

        if (pressed)
            encoderSelectReady = false;

        selectLast = now;

        return pressed;
    }

    bool buttonAPressed(bool aHigh, unsigned long nowMs)
    {
        bool now = aHigh;

        if (aSuppressed)
        {
            aLast = now;

            if (now == true && nowMs - aSuppressedAt >=
                MENU_INPUT_GUARD_MS)
            {
                aSuppressed = false;
            }

            return false;
        }

        bool pressed = (now == false && aLast == true);

        aLast = now;

        return pressed;
    }

    bool buttonBPressed(bool bHigh, unsigned long nowMs)
    {
        bool now = bHigh;

        if (bSuppressed)
        {
            bLast = now;

            if (now == true && nowMs - bSuppressedAt >=
                MENU_INPUT_GUARD_MS)
            {
                bSuppressed = false;
            }

            return false;
        }

        bool pressed = (now == false && bLast == true);

        bLast = now;

        return pressed;
    }

    void suppressMenuInputUntilRelease(bool selectHigh, bool aHigh, bool bHigh, unsigned long nowMs)
    {
        suppressEncoderSelectUntilRelease(selectHigh, nowMs);

        const unsigned long now = nowMs;
        aLast = aHigh;
        bLast = bHigh;
        aSuppressed = true;
        bSuppressed = true;
        aSuppressedAt = now;
        bSuppressedAt = now;
    }

    bool encoderButtonLongPressed(bool selectHigh, unsigned long nowMs)
    {
        if (encoderSelectSuppressed)
        {
            encoderPressStart = 0;
            encoderLongPressHandled = false;
            return false;
        }

        bool pressed = (selectHigh == false);

        if (pressed)
        {
            if (encoderPressStart == 0)
            {
                encoderPressStart = nowMs;
                encoderLongPressHandled = false;
            }

            if (!encoderLongPressHandled &&
                nowMs - encoderPressStart >= LONG_PRESS_TIME)
            {
                encoderLongPressHandled = true;
                return true;
            }
        }
        else
        {
            encoderPressStart = 0;
            encoderLongPressHandled = false;
        }

        return false;
    }

    int8_t readEncoder(bool clkHigh, bool dtHigh, bool inverted)
    {
        const int8_t step = applyEncoderRotationInversion(
            updateQuadratureDecoder(
                encoderDecoder,
                clkHigh,
                dtHigh),
            inverted);

        return step;
    }
    void reset(bool clkHigh, bool dtHigh, bool selectHigh, bool aHigh, bool bHigh, unsigned long nowMs)
    {
        resetQuadratureDecoder(
            encoderDecoder,
            clkHigh,
            dtHigh);

        selectLast = selectHigh;
        aLast = aHigh;
        bLast = bHigh;

        encoderSelectReady = selectLast == true;
        encoderSelectReleasedAt = nowMs;
        encoderSelectSuppressed = false;
        encoderSelectSuppressedAt = 0;
        aSuppressed = false;
        bSuppressed = false;
        aSuppressedAt = 0;
        bSuppressedAt = 0;
    }

};
