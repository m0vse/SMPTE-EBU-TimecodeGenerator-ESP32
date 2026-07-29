#pragma once

#include <Arduino.h>
#include <time.h>

void ntp_setup(unsigned int syncEveryMinutes);
bool ntp_loop();
int setAndWriteNtp(float adjustment, const String &timezone);

bool ntpHasSynchronized();
time_t ntpLastLtcSync();
time_t ntpLastNetworkSync();
uint32_t ntpNetworkSyncCount();
double ntpPhaseErrorSeconds();
const char *ntpSyncStatusText();
double ntpCurrentOutputSeconds();

void ntpOutputStopped();
void ntpRestartOutput();
