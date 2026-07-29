#pragma once

#include <driver/gpio.h>
#include <stdint.h>

void rmt_setup(gpio_num_t pin);
void rmt_start();
void rmt_loop();

bool rmtOutputEnabled();
bool rmtOutputRequested();
void rmtSetOutputEnabled(bool enabled);
void rmtSetFrequencyCorrectionPpm(double correctionPpm);
double rmtFrequencyCorrectionPpm();
uint32_t rmtUnderrunCount();
uint32_t rmtOutputStartMicros();
