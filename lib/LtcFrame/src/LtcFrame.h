#pragma once

#include <stddef.h>
#include <stdint.h>

struct LtcTimecode {
  uint8_t frame;
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;
};

uint8_t toBcd(unsigned int value);
unsigned int fromBcd(uint8_t value);
void setLtcTime(LtcTimecode &timecode, unsigned int hours,
                unsigned int minutes, unsigned int seconds,
                unsigned int frame);
bool advanceLtcTime(LtcTimecode &timecode, unsigned int framesPerSecond);
void buildNextLtcFrame(LtcTimecode &timecode, uint8_t block[10],
                       unsigned int framesPerSecond,
                       const uint8_t userBits[8]);
