#include "LtcFrame.h"

#include <string.h>

uint8_t toBcd(unsigned int value) {
  return static_cast<uint8_t>(((value / 10U) << 4U) | (value % 10U));
}

unsigned int fromBcd(uint8_t value) {
  return static_cast<unsigned int>(((value >> 4U) * 10U) + (value & 0x0fU));
}

void setLtcTime(LtcTimecode &timecode, unsigned int hours,
                unsigned int minutes, unsigned int seconds,
                unsigned int frame) {
  timecode.hours = toBcd(hours % 24U);
  timecode.minutes = toBcd(minutes % 60U);
  timecode.seconds = toBcd(seconds % 60U);
  timecode.frame = toBcd(frame);
}

bool advanceLtcTime(LtcTimecode &timecode, unsigned int framesPerSecond) {
  unsigned int frame = fromBcd(timecode.frame) + 1U;
  if (frame < framesPerSecond) {
    timecode.frame = toBcd(frame);
    return false;
  }
  timecode.frame = 0;

  unsigned int seconds = fromBcd(timecode.seconds) + 1U;
  if (seconds < 60U) {
    timecode.seconds = toBcd(seconds);
    return false;
  }
  timecode.seconds = 0;

  unsigned int minutes = fromBcd(timecode.minutes) + 1U;
  if (minutes < 60U) {
    timecode.minutes = toBcd(minutes);
    return false;
  }
  timecode.minutes = 0;

  unsigned int hours = fromBcd(timecode.hours) + 1U;
  if (hours < 24U) {
    timecode.hours = toBcd(hours);
    return false;
  }
  timecode.hours = 0;
  return true;
}

void buildNextLtcFrame(LtcTimecode &timecode, uint8_t block[10],
                       unsigned int framesPerSecond,
                       const uint8_t userBits[8]) {
  advanceLtcTime(timecode, framesPerSecond);
  memset(block, 0, 10);

  block[0] = static_cast<uint8_t>((userBits[0] << 4U) | (timecode.frame & 0x0fU));
  block[1] = static_cast<uint8_t>((userBits[1] << 4U) | (timecode.frame >> 4U));
  block[2] = static_cast<uint8_t>((userBits[2] << 4U) | (timecode.seconds & 0x0fU));
  block[3] = static_cast<uint8_t>((userBits[3] << 4U) | (timecode.seconds >> 4U));
  block[4] = static_cast<uint8_t>((userBits[4] << 4U) | (timecode.minutes & 0x0fU));
  block[5] = static_cast<uint8_t>((userBits[5] << 4U) | (timecode.minutes >> 4U));
  block[6] = static_cast<uint8_t>((userBits[6] << 4U) | (timecode.hours & 0x0fU));
  block[7] = static_cast<uint8_t>((userBits[7] << 4U) | (timecode.hours >> 4U));
  block[8] = 0xfc;
  block[9] = 0xbf;

  uint8_t parity = 1;
  for (size_t i = 0; i < 8; ++i) {
    parity ^= block[i];
  }
  parity ^= parity >> 4U;
  parity ^= parity >> 2U;
  parity ^= parity >> 1U;

  if ((parity & 1U) != 0U) {
    block[(framesPerSecond == 30U) ? 3U : 7U] |= 0x08U;
  }
}
