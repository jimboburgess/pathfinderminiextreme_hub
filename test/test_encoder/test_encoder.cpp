#include <Arduino.h>
#include <unity.h>

#include "../../src/input/encoderdecoder.h"

namespace
{
int8_t advance(
    QuadratureDecoderState& decoder,
    uint8_t phase)
{
    return updateQuadratureDecoder(
        decoder,
        (phase & 0x02) != 0,
        (phase & 0x01) != 0);
}

void resetAtHighDetent(QuadratureDecoderState& decoder)
{
    resetQuadratureDecoder(decoder, true, true);
}
}

void test_clockwise_detent_emits_exactly_one_step()
{
    QuadratureDecoderState decoder;
    resetAtHighDetent(decoder);

    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x02));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x00));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x01));
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_CLOCKWISE_STEP,
        advance(decoder, 0x03));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x03));
}

void test_counterclockwise_detent_emits_exactly_one_step()
{
    QuadratureDecoderState decoder;
    resetAtHighDetent(decoder);

    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x01));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x00));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x02));
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_COUNTERCLOCKWISE_STEP,
        advance(decoder, 0x03));
}

void test_reversed_contact_bounce_cancels_before_detent()
{
    QuadratureDecoderState decoder;
    resetAtHighDetent(decoder);

    // DT bounces low and high before the real clockwise sequence continues.
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x02));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x03));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x02));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x00));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x01));
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_CLOCKWISE_STEP,
        advance(decoder, 0x03));
}

void test_invalid_two_bit_transition_discards_partial_detent()
{
    QuadratureDecoderState decoder;
    resetAtHighDetent(decoder);

    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x02));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x00));

    // 00 -> 11 changes both bits and must clear the two accumulated steps.
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x03));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x02));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x00));
    TEST_ASSERT_EQUAL_INT8(QUADRATURE_NO_STEP, advance(decoder, 0x01));
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_CLOCKWISE_STEP,
        advance(decoder, 0x03));
}

void test_decoder_initialization_never_emits_a_phantom_step()
{
    QuadratureDecoderState decoder;

    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_NO_STEP,
        updateQuadratureDecoder(decoder, false, false));
    TEST_ASSERT_TRUE(decoder.initialized);
    TEST_ASSERT_EQUAL_UINT8(0x00, decoder.previousPhase);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_clockwise_detent_emits_exactly_one_step);
    RUN_TEST(test_counterclockwise_detent_emits_exactly_one_step);
    RUN_TEST(test_reversed_contact_bounce_cancels_before_detent);
    RUN_TEST(test_invalid_two_bit_transition_discards_partial_detent);
    RUN_TEST(test_decoder_initialization_never_emits_a_phantom_step);
    UNITY_END();
}

void loop()
{
}
