#pragma once

#include <stdint.h>

void fillNextBlock(uint8_t block[10], unsigned int fps);
void setTS(unsigned int hours, unsigned int minutes, unsigned int seconds);
void setTSF(unsigned int hours, unsigned int minutes, unsigned int seconds,
            unsigned int frame);
