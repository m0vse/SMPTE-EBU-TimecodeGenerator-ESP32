#pragma once

#include <Arduino.h>

#include "LtcFrame.h"

#ifndef LTC_FPS
#define LTC_FPS 25
#endif

static_assert(LTC_FPS == 25 || LTC_FPS == 30,
              "Only 25 and 30 frames per second are supported");

inline constexpr char FIRMWARE_VERSION[] = "3.00";
inline constexpr unsigned int FPS = LTC_FPS;
inline constexpr size_t LTC_FRAMES_PER_BUFFER = 8;
inline constexpr float FIDDLE_BUFFER_DELAY =
    (2.0F * static_cast<float>(LTC_FRAMES_PER_BUFFER)) /
    static_cast<float>(FPS);

inline constexpr gpio_num_t RED_PIN = GPIO_NUM_13;
inline constexpr gpio_num_t BLACK_PIN = GPIO_NUM_12;
inline constexpr gpio_num_t SENSE_PIN = GPIO_NUM_14;

#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif

#ifndef NTP_DEFAULT_TZ
#define NTP_DEFAULT_TZ "GMT0BST,M3.5.0/1,M10.5.0/2"
#endif

#ifndef WIFI_RECONNECT_RETRY_TIMEOUT
#define WIFI_RECONNECT_RETRY_TIMEOUT (60UL * 1000UL)
#endif

#ifndef WIFI_PROVISIONING_TIMEOUT
#define WIFI_PROVISIONING_TIMEOUT (20UL * 1000UL)
#endif

extern const char *name;
extern String tz;
extern float fiddleSeconds;
extern LtcTimecode ltcTime;
