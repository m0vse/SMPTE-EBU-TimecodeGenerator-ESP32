#include <unity.h>
#include <initializer_list>
#include <math.h>

#include "LtcFrame.h"
#include "ClockDiscipline.h"

void setUp() {}
void tearDown() {}

namespace {

void test_bcd_round_trip() {
  for (unsigned int value = 0; value < 60; ++value) {
    TEST_ASSERT_EQUAL_UINT(value, fromBcd(toBcd(value)));
  }
}

void test_25fps_rollover() {
  LtcTimecode timecode{};
  setLtcTime(timecode, 23, 59, 59, 24);
  TEST_ASSERT_TRUE(advanceLtcTime(timecode, 25));
  TEST_ASSERT_EQUAL_HEX8(0x00, timecode.hours);
  TEST_ASSERT_EQUAL_HEX8(0x00, timecode.minutes);
  TEST_ASSERT_EQUAL_HEX8(0x00, timecode.seconds);
  TEST_ASSERT_EQUAL_HEX8(0x00, timecode.frame);
}

void test_30fps_rollover() {
  LtcTimecode timecode{};
  setLtcTime(timecode, 12, 34, 59, 29);
  TEST_ASSERT_FALSE(advanceLtcTime(timecode, 30));
  TEST_ASSERT_EQUAL_HEX8(0x12, timecode.hours);
  TEST_ASSERT_EQUAL_HEX8(0x35, timecode.minutes);
  TEST_ASSERT_EQUAL_HEX8(0x00, timecode.seconds);
  TEST_ASSERT_EQUAL_HEX8(0x00, timecode.frame);
}

void test_sync_word_and_even_parity() {
  constexpr uint8_t userBits[8] = {};
  for (unsigned int fps : {25U, 30U}) {
    LtcTimecode timecode{};
    setLtcTime(timecode, 1, 2, 3, 0);
    uint8_t block[10]{};
    buildNextLtcFrame(timecode, block, fps, userBits);
    TEST_ASSERT_EQUAL_HEX8(0xfc, block[8]);
    TEST_ASSERT_EQUAL_HEX8(0xbf, block[9]);

    unsigned int ones = 0;
    for (uint8_t byte : block) {
      for (unsigned int bit = 0; bit < 8; ++bit) {
        ones += (byte >> bit) & 1U;
      }
    }
    TEST_ASSERT_EQUAL_UINT(0, ones % 2U);
  }
}

void test_phase_error_wraps_at_midnight() {
  TEST_ASSERT_TRUE(fabs(wrapDayPhaseError(-86398.0) - 2.0) < 0.000001);
  TEST_ASSERT_TRUE(fabs(wrapDayPhaseError(86398.0) + 2.0) < 0.000001);
}

void test_clock_discipline_has_correct_sign_and_limits() {
  ClockDisciplineState state{};
  double correction = updateClockDiscipline(state, 1.0, 2.0);
  TEST_ASSERT_TRUE(correction > 0.0);
  TEST_ASSERT_TRUE(correction <= 500.0);

  resetClockDiscipline(state);
  correction = updateClockDiscipline(state, -1.0, 2.0);
  TEST_ASSERT_TRUE(correction < 0.0);
  TEST_ASSERT_TRUE(correction >= -500.0);
}

}  // namespace

int runTests() {
  UNITY_BEGIN();
  RUN_TEST(test_bcd_round_trip);
  RUN_TEST(test_25fps_rollover);
  RUN_TEST(test_30fps_rollover);
  RUN_TEST(test_sync_word_and_even_parity);
  RUN_TEST(test_phase_error_wraps_at_midnight);
  RUN_TEST(test_clock_discipline_has_correct_sign_and_limits);
  return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>

void setup() {
  delay(1000);
  runTests();
}

void loop() {}
#else
int main(int, char **) {
  return runTests();
}
#endif
