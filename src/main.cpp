/* Copyright (c) 2011, 2018 Dirk-Willem van Gulik, All Rights Reserved.
 * Modern PlatformIO/Core 3 port and provisioning/UI improvements:
 * Copyright (c) 2026 Phil Taylor.
 * Licensed under the Apache License, Version 2.0.
 */

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_arduino_version.h>
#include <esp_private/brownout.h>

#include "firmware_config.h"
#include "ltc_output.h"
#include "network_config.h"
#include "ntp_clock.h"
#include "ota_service.h"
#include "partition_migration.h"
#include "secrets.h"
#include "web_config.h"

const char *name = "smpte-clock";
String tz = NTP_DEFAULT_TZ;
float fiddleSeconds = 0.0F;
LtcTimecode ltcTime{0, 0x10, 0x20, 0x00};

static Preferences wifiPreferences;
static String wifiSsid;
static String wifiPassword;
static bool provisioningActive = false;
static bool mdnsActive = false;
static bool servicesStarted = false;
static unsigned long disconnectedSince = 0;
static unsigned long radioStartedAt = 0;
static bool startupBrownoutGuardDisabled = false;

static void loadWifiConfiguration() {
  wifiPreferences.begin("smpte-wifi", false);
  wifiSsid = wifiPreferences.isKey("ssid")
                 ? wifiPreferences.getString("ssid")
                 : String(WIFI_SSID);
  wifiPassword = wifiPreferences.isKey("password")
                     ? wifiPreferences.getString("password")
                     : String(WIFI_PASSWORD);
  wifiPreferences.end();
}

bool saveWifiConfiguration(const String &ssid, const String &password) {
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 ||
      (!password.isEmpty() && password.length() < 8)) {
    return false;
  }

  if (!wifiPreferences.begin("smpte-wifi", false)) {
    return false;
  }
  const bool ok = wifiPreferences.putString("ssid", ssid) > 0 &&
                  wifiPreferences.putString("password", password) > 0;
  wifiPreferences.end();
  if (!ok) {
    return false;
  }

  wifiSsid = ssid;
  wifiPassword = password;
  WiFi.disconnect();
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  disconnectedSince = millis();
  return true;
}

String configuredWifiSsid() {
  return wifiSsid;
}

String configuredWifiPassword() {
  return wifiPassword;
}

bool wifiProvisioningActive() {
  return provisioningActive;
}

static void startProvisioningAccessPoint() {
  if (provisioningActive) {
    return;
  }

  const uint64_t chipId = ESP.getEfuseMac();
  char apName[32];
  snprintf(apName, sizeof(apName), "smpte-setup-%06llx",
           static_cast<unsigned long long>(chipId & 0xffffffULL));

  WiFi.mode(WIFI_AP_STA);
  const bool secured = strlen(PROVISIONING_PASSWORD) >= 8;
  const bool started =
      secured ? WiFi.softAP(apName, PROVISIONING_PASSWORD) : WiFi.softAP(apName);
  if (started) {
    provisioningActive = true;
    Serial.printf("Provisioning AP: %s at %s%s\n", apName,
                  WiFi.softAPIP().toString().c_str(),
                  secured ? "" : " (open; set PROVISIONING_PASSWORD)");
  } else {
    Serial.println("Unable to start provisioning access point");
  }
}

static void startMdns() {
  if (mdnsActive) {
    MDNS.end();
    mdnsActive = false;
  }
  if (MDNS.begin(name)) {
    MDNS.addService("http", "tcp", 80);
    mdnsActive = true;
  } else {
    Serial.println("mDNS startup failed");
  }
}

static void startServices() {
  if (servicesStarted) {
    return;
  }
  web_setup();
  ota_setup();
  servicesStarted = true;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.printf("\n\nBooting SMPTEGenerator %s (%s %s)\n",
                FIRMWARE_VERSION, __DATE__, __TIME__);
  Serial.printf("Arduino-ESP32 %s, CPU %u MHz\n", ESP_ARDUINO_VERSION_STR,
                getCpuFrequencyMhz());

  pinMode(SENSE_PIN, INPUT_PULLUP);
  name = digitalRead(SENSE_PIN) ? "smpte-digital-clock"
                                : "smpte-analog-clock";
  Serial.printf("Detected model %s\n", name);

  pinMode(BLACK_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  digitalWrite(BLACK_PIN, LOW);
  digitalWrite(RED_PIN, LOW);

  loadWifiConfiguration();
  ntp_setup(15);

  // The LTC generator needs very little CPU. Running at 80 MHz materially
  // reduces supply-current peaks on older clock interface boards.
  setCpuFrequencyMhz(80);
  // Let the small on-board reservoir settle before radio calibration. The
  // original clock-derived 5 V supply is marginal during ESP32 RF startup.
  delay(2000);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  // This clock's internal 5 V auxiliary rail dips during RF calibration with
  // recent ESP-IDF releases. Mask brownout reset only for radio startup, then
  // restore protection as soon as the station is up (or after five seconds).
  esp_brownout_disable();
  startupBrownoutGuardDisabled = true;
  WiFi.mode(WIFI_STA);
  if (!WiFi.setHostname(name)) {
    Serial.println("Unable to set Wi-Fi hostname");
  }
  WiFi.setSleep(WIFI_PS_MAX_MODEM);
  WiFi.setTxPower(WIFI_POWER_2dBm);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  radioStartedAt = millis();
  disconnectedSince = millis();

  startServices();
  Serial.println("Waiting for Wi-Fi and NTP synchronization");
}

void wifi_loop() {
  static wl_status_t previousStatus = WL_NO_SHIELD;
  static unsigned long lastReconnectAttempt = 0;
  const wl_status_t status = WiFi.status();

  if (startupBrownoutGuardDisabled &&
      (status == WL_CONNECTED || millis() - radioStartedAt >= 5000)) {
    esp_brownout_init();
    startupBrownoutGuardDisabled = false;
    Serial.println("Brownout protection restored after radio startup");
  }

  if (status == WL_CONNECTED) {
    if (previousStatus != WL_CONNECTED) {
      Serial.printf("Wi-Fi connected: %s, RSSI %d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      startMdns();
    }
    disconnectedSince = 0;
  } else {
    if (previousStatus == WL_CONNECTED) {
      Serial.println("Wi-Fi connection lost");
      disconnectedSince = millis();
    } else if (disconnectedSince == 0) {
      disconnectedSince = millis();
    }

    if (!provisioningActive &&
        millis() - disconnectedSince >= WIFI_PROVISIONING_TIMEOUT) {
      startProvisioningAccessPoint();
    }

    if (millis() - lastReconnectAttempt >= WIFI_RECONNECT_RETRY_TIMEOUT) {
      lastReconnectAttempt = millis();
      Serial.println("Retrying Wi-Fi connection");
      WiFi.disconnect();
      WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    }
  }
  previousStatus = status;
}

void loop() {
  wifi_loop();
  ota_loop();
  web_loop();
#ifdef ENABLE_PARTITION_MIGRATION
  partitionMigrationLoop();
#endif
  rmt_loop();
  ntp_loop();
  delay(1);
}
