/* Smooth SNTP and LTC clock discipline.
 * Licensed under the Apache License, Version 2.0.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <esp_sntp.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

#include "ClockDiscipline.h"
#include "firmware_config.h"
#include "ltc_frame_builder.h"
#include "ltc_output.h"
#include "ntp_clock.h"

static constexpr unsigned int DEFAULT_SYNC_MINS = 15;
static constexpr float MAX_FIDDLE_SECONDS = 3600.0F;
static constexpr uint32_t DISCIPLINE_INTERVAL_MS = 2000;
static constexpr double MAX_SLEW_PHASE_ERROR_SECONDS = 1.0;

static unsigned int syncMinutes = DEFAULT_SYNC_MINS;
static time_t lastLtcSync = 0;
static volatile time_t lastNetworkSync = 0;
static volatile uint32_t networkSyncCount = 0;
static portMUX_TYPE ntpMux = portMUX_INITIALIZER_UNLOCKED;
static bool clockStarted = false;
static bool systemTimeValid = false;
static double modeledLtcSeconds = 0.0;
static uint32_t lastModelMicros = 0;
static ClockDisciplineState discipline;
static unsigned long lastDisciplineMillis = 0;
static Preferences clockPreferences;

static void onNetworkTimeSynchronized(struct timeval *value) {
  portENTER_CRITICAL(&ntpMux);
  lastNetworkSync = value == nullptr ? 0 : value->tv_sec;
  networkSyncCount = networkSyncCount + 1U;
  portEXIT_CRITICAL(&ntpMux);
}

bool ntpHasSynchronized() {
  portENTER_CRITICAL(&ntpMux);
  const bool networkTimeReceived = networkSyncCount > 0;
  portEXIT_CRITICAL(&ntpMux);
  return systemTimeValid && networkTimeReceived;
}

time_t ntpLastLtcSync() {
  return lastLtcSync;
}

time_t ntpLastNetworkSync() {
  portENTER_CRITICAL(&ntpMux);
  const time_t value = lastNetworkSync;
  portEXIT_CRITICAL(&ntpMux);
  return value;
}

uint32_t ntpNetworkSyncCount() {
  portENTER_CRITICAL(&ntpMux);
  const uint32_t value = networkSyncCount;
  portEXIT_CRITICAL(&ntpMux);
  return value;
}

double ntpPhaseErrorSeconds() {
  return discipline.filtered_phase_seconds;
}

const char *ntpSyncStatusText() {
  switch (sntp_get_sync_status()) {
    case SNTP_SYNC_STATUS_COMPLETED:
      return "synchronized";
    case SNTP_SYNC_STATUS_IN_PROGRESS:
      return "slewing";
    case SNTP_SYNC_STATUS_RESET:
    default:
      return ntpLastNetworkSync() == 0 ? "waiting" : "polling";
  }
}

static void configureSmoothSntp() {
  configTzTime(tz.c_str(), NTP_SERVER);
  sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
  sntp_set_sync_interval(syncMinutes * 60UL * 1000UL);
  sntp_set_time_sync_notification_cb(onNetworkTimeSynchronized);
  sntp_restart();
}

int setNtp(float adjustment, const String &timezone) {
  if (!isfinite(adjustment) || fabsf(adjustment) > MAX_FIDDLE_SECONDS ||
      timezone.isEmpty() || timezone.length() > 128) {
    return -1;
  }

  tz = timezone;
  fiddleSeconds = adjustment;
  configureSmoothSntp();
  Serial.printf(
      "Clock config: adjustment %.3f s, TZ <%s>, server %s, smooth SNTP %u min\n",
      fiddleSeconds, tz.c_str(), NTP_SERVER, syncMinutes);
  return 0;
}

int setAndWriteNtp(float adjustment, const String &timezone) {
  if (setNtp(adjustment, timezone) != 0) {
    return -1;
  }

  if (!clockPreferences.begin("smpte-clock", false)) {
    return -1;
  }
  const bool ok =
      clockPreferences.putFloat("fiddle", adjustment) == sizeof(float) &&
      clockPreferences.putString("timezone", timezone) > 0;
  clockPreferences.end();
  return ok ? 0 : -1;
}

void ntp_setup(unsigned int syncEveryMinutes) {
  syncMinutes =
      syncEveryMinutes == 0 ? DEFAULT_SYNC_MINS : syncEveryMinutes;

  float adjustment = 0.0F;
  String timezone = NTP_DEFAULT_TZ;
  if (clockPreferences.begin("smpte-clock", false)) {
    if (clockPreferences.isKey("fiddle")) {
      adjustment = clockPreferences.getFloat("fiddle");
    }
    if (clockPreferences.isKey("timezone")) {
      timezone = clockPreferences.getString("timezone");
    }
    clockPreferences.end();
  }

  if (setNtp(adjustment, timezone) != 0) {
    setNtp(0.0F, NTP_DEFAULT_TZ);
  }
}

static double wrapDayValue(double value) {
  while (value >= 86400.0) {
    value -= 86400.0;
  }
  while (value < 0.0) {
    value += 86400.0;
  }
  return value;
}

static double ltcCursorSecondsOfDay() {
  return static_cast<double>(fromBcd(ltcTime.hours) * 3600U +
                             fromBcd(ltcTime.minutes) * 60U +
                             fromBcd(ltcTime.seconds)) +
         static_cast<double>(fromBcd(ltcTime.frame)) /
             static_cast<double>(FPS);
}

static void updateModeledLtcTime() {
  if (!clockStarted) {
    return;
  }
  const uint32_t nowMicros = micros();
  const double elapsed =
      static_cast<double>(nowMicros - lastModelMicros) / 1000000.0;
  modeledLtcSeconds = wrapDayValue(
      modeledLtcSeconds +
      elapsed * (1.0 + rmtFrequencyCorrectionPpm() / 1000000.0));
  lastModelMicros = nowMicros;
}

double ntpCurrentOutputSeconds() {
  updateModeledLtcTime();
  return modeledLtcSeconds;
}

static void setLtcFromEpoch(double epochSeconds) {
  double integralSeconds = 0.0;
  double fractionalSecond = modf(epochSeconds, &integralSeconds);
  if (fractionalSecond < 0.0) {
    fractionalSecond += 1.0;
    integralSeconds -= 1.0;
  }

  const time_t localEpoch = static_cast<time_t>(integralSeconds);
  tm localTime{};
  localtime_r(&localEpoch, &localTime);
  unsigned int frame =
      static_cast<unsigned int>(fractionalSecond * static_cast<double>(FPS));
  if (frame >= FPS) {
    frame = FPS - 1;
  }
  setTSF(localTime.tm_hour, localTime.tm_min, localTime.tm_sec, frame);
}

void ntpOutputStopped() {
  updateModeledLtcTime();
  clockStarted = false;
  resetClockDiscipline(discipline);
  rmtSetFrequencyCorrectionPpm(0.0);
}

void ntpRestartOutput() {
  timeval tv{};
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < 1609459200) {
    return;
  }
  const double systemEpoch =
      static_cast<double>(tv.tv_sec) +
      static_cast<double>(tv.tv_usec) / 1000000.0 +
      static_cast<double>(fiddleSeconds);
  rmtSetFrequencyCorrectionPpm(0.0);
  setLtcFromEpoch(systemEpoch);
  const double firstFrameSeconds =
      wrapDayValue(ltcCursorSecondsOfDay() + 1.0 / static_cast<double>(FPS));
  rmt_start();
  modeledLtcSeconds = firstFrameSeconds;
  lastModelMicros = rmtOutputStartMicros();
  lastLtcSync = tv.tv_sec;
  lastDisciplineMillis = millis();
  clockStarted = true;
}

bool ntp_loop() {
  timeval tv{};
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < 1609459200) {
    return false;
  }
  systemTimeValid = true;

  const double systemEpoch =
      static_cast<double>(tv.tv_sec) +
      static_cast<double>(tv.tv_usec) / 1000000.0 +
      static_cast<double>(fiddleSeconds);

  // An OTA reboot can retain a plausible but stale system timestamp. Do not
  // emit it as LTC until this boot has received an actual NTP response.
  if (!clockStarted && rmtOutputRequested() && ntpHasSynchronized()) {
    rmt_setup(RED_PIN);
    ntpRestartOutput();
  }
  if (!clockStarted) {
    return true;
  }

  const unsigned long nowMillis = millis();
  const unsigned long elapsedMillis = nowMillis - lastDisciplineMillis;
  if (elapsedMillis < DISCIPLINE_INTERVAL_MS) {
    return true;
  }
  lastDisciplineMillis = nowMillis;

  const time_t wholeEpoch = static_cast<time_t>(floor(systemEpoch));
  tm localTime{};
  localtime_r(&wholeEpoch, &localTime);
  const double localSeconds =
      static_cast<double>(localTime.tm_hour * 3600 + localTime.tm_min * 60 +
                          localTime.tm_sec) +
      (systemEpoch - floor(systemEpoch));

  updateModeledLtcTime();
  const double rawError =
      wrapDayPhaseError(localSeconds - modeledLtcSeconds);
  if (fabs(rawError) > MAX_SLEW_PHASE_ERROR_SECONDS) {
    // A retained pre-OTA timestamp, a large system-clock correction, or a
    // daylight-saving transition cannot be recovered sensibly at the bounded
    // slew rate. Reacquire current wall time explicitly; normal small NTP
    // corrections remain continuous and never skip or repeat frames.
    Serial.printf("Gross LTC phase error %.3f s; reacquiring current time\n",
                  rawError);
    rmtSetOutputEnabled(false);
    rmtSetOutputEnabled(true);
    return true;
  }
  const double elapsedSeconds =
      static_cast<double>(elapsedMillis) / 1000.0;
  const double correction =
      updateClockDiscipline(discipline, rawError, elapsedSeconds);
  rmtSetFrequencyCorrectionPpm(correction);
  lastLtcSync = tv.tv_sec;
  return true;
}
