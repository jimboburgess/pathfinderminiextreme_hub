#include <Arduino.h>
#include <unity.h>

#include "../../src/input/encoderdecoder.h"
#include "../../src/input/buttonstate.h"

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

void test_rotation_inversion_preserves_default_direction()
{
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_CLOCKWISE_STEP,
        applyEncoderRotationInversion(
            QUADRATURE_CLOCKWISE_STEP,
            false));
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_COUNTERCLOCKWISE_STEP,
        applyEncoderRotationInversion(
            QUADRATURE_COUNTERCLOCKWISE_STEP,
            false));
}

void test_rotation_inversion_reverses_only_movement()
{
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_COUNTERCLOCKWISE_STEP,
        applyEncoderRotationInversion(
            QUADRATURE_CLOCKWISE_STEP,
            true));
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_CLOCKWISE_STEP,
        applyEncoderRotationInversion(
            QUADRATURE_COUNTERCLOCKWISE_STEP,
            true));
    TEST_ASSERT_EQUAL_INT8(
        QUADRATURE_NO_STEP,
        applyEncoderRotationInversion(QUADRATURE_NO_STEP, true));
}

void test_controller_inputs_do_not_share_edges_or_long_press()
{
    ButtonInputState first, second;
    first.reset(true,true,true,true,true,1);
    second.reset(true,true,true,true,true,1);
    TEST_ASSERT_TRUE(first.buttonAPressed(false,10));
    TEST_ASSERT_TRUE(second.buttonAPressed(false,10));
    TEST_ASSERT_FALSE(first.buttonAPressed(false,11));
    TEST_ASSERT_FALSE(first.encoderButtonLongPressed(false,100));
    TEST_ASSERT_FALSE(second.encoderButtonLongPressed(true,849));
    TEST_ASSERT_TRUE(first.encoderButtonLongPressed(false,850));
    TEST_ASSERT_FALSE(second.encoderButtonLongPressed(false,850));
    TEST_ASSERT_FALSE(first.encoderButtonLongPressed(false,851));
    first.suppressMenuInputUntilRelease(false,false,false,900);
    TEST_ASSERT_TRUE(second.buttonBPressed(false,901));
    TEST_ASSERT_FALSE(first.buttonBPressed(false,901));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_clockwise_detent_emits_exactly_one_step);
    RUN_TEST(test_counterclockwise_detent_emits_exactly_one_step);
    RUN_TEST(test_reversed_contact_bounce_cancels_before_detent);
    RUN_TEST(test_invalid_two_bit_transition_discards_partial_detent);
    RUN_TEST(test_decoder_initialization_never_emits_a_phantom_step);
    RUN_TEST(test_rotation_inversion_preserves_default_direction);
    RUN_TEST(test_rotation_inversion_reverses_only_movement);
    RUN_TEST(test_controller_inputs_do_not_share_edges_or_long_press);
    UNITY_END();
}

void loop()
{
}
