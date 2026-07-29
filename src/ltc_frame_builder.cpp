/* LTC frame assembly. Licensed under the Apache License, Version 2.0. */

#include <Arduino.h>

#include "LtcFrame.h"
#include "firmware_config.h"
#include "ltc_frame_builder.h"

static constexpr uint8_t userBits[8] = {};

void fillNextBlock(uint8_t block[10], unsigned int fps) {
  buildNextLtcFrame(ltcTime, block, fps, userBits);
}
void setTS(unsigned int hours, unsigned int minutes, unsigned int seconds) {
  setTSF(hours, minutes, seconds, fromBcd(ltcTime.frame));
}

void setTSF(unsigned int hours, unsigned int minutes, unsigned int seconds,
            unsigned int frame) {
  setLtcTime(ltcTime, hours, minutes, seconds, frame);
  Serial.printf(
      "LTC time %02u:%02u:%02u.%02u (buffer %.3f seconds, %u fps)\n",
      hours, minutes, seconds, frame, FIDDLE_BUFFER_DELAY, FPS);
}
