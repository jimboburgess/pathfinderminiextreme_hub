#ifndef PATHFINDERMINIEXTREME_025_ENCODERDECODER_H
#define PATHFINDERMINIEXTREME_025_ENCODERDECODER_H

#include <stdint.h>

// A mechanical rotary encoder produces a two-bit Gray-code sequence. Each
// valid transition contributes one quarter-step; an event is emitted only
// after a complete detent. Reversed contact bounce cancels itself instead of
// becoming another turn.
struct QuadratureDecoderState
{
    uint8_t previousPhase = 0;
    int8_t transitionTotal = 0;
    bool initialized = false;
};

constexpr int8_t QUADRATURE_NO_STEP = 0;
constexpr int8_t QUADRATURE_CLOCKWISE_STEP = 1;
constexpr int8_t QUADRATURE_COUNTERCLOCKWISE_STEP = -1;
constexpr int8_t QUADRATURE_TRANSITIONS_PER_DETENT = 4;

inline uint8_t getQuadraturePhase(bool clkHigh, bool dtHigh)
{
    return static_cast<uint8_t>(
        (clkHigh ? 0x02 : 0x00) |
        (dtHigh ? 0x01 : 0x00));
}

inline void resetQuadratureDecoder(
    QuadratureDecoderState& state,
    bool clkHigh,
    bool dtHigh)
{
    state.previousPhase = getQuadraturePhase(clkHigh, dtHigh);
    state.transitionTotal = 0;
    state.initialized = true;
}

inline int8_t updateQuadratureDecoder(
    QuadratureDecoderState& state,
    bool clkHigh,
    bool dtHigh)
{
    // The sign preserves the project's existing direction convention: when
    // DT leads CLK, the completed detent is clockwise.
    static constexpr int8_t transitionTable[16] =
    {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0
    };

    const uint8_t currentPhase = getQuadraturePhase(clkHigh, dtHigh);

    if (!state.initialized)
    {
        resetQuadratureDecoder(state, clkHigh, dtHigh);
        return QUADRATURE_NO_STEP;
    }

    const uint8_t previousPhase = state.previousPhase;
    const int8_t transition = transitionTable[
        static_cast<uint8_t>((previousPhase << 2) | currentPhase)];

    state.previousPhase = currentPhase;

    if (currentPhase != previousPhase && transition == 0)
    {
        // Both bits changed between samples. That is not a valid quadrature
        // transition, so discard the partial detent rather than combining it
        // with later movement.
        state.transitionTotal = 0;
        return QUADRATURE_NO_STEP;
    }

    state.transitionTotal = static_cast<int8_t>(
        state.transitionTotal + transition);

    if (state.transitionTotal >= QUADRATURE_TRANSITIONS_PER_DETENT)
    {
        state.transitionTotal = 0;
        return QUADRATURE_CLOCKWISE_STEP;
    }

    if (state.transitionTotal <= -QUADRATURE_TRANSITIONS_PER_DETENT)
    {
        state.transitionTotal = 0;
        return QUADRATURE_COUNTERCLOCKWISE_STEP;
    }

    return QUADRATURE_NO_STEP;
}

#endif // PATHFINDERMINIEXTREME_025_ENCODERDECODER_H
