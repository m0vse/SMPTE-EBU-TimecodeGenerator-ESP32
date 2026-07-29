/* OTA support. Licensed under the Apache License, Version 2.0. */

#include <Arduino.h>
#include <ArduinoOTA.h>

#include "firmware_config.h"
#include "ltc_output.h"
#include "ota_service.h"
#include "secrets.h"

static bool resumeOutputAfterOta = false;

void ota_setup() {
  ArduinoOTA.setHostname(name);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  } else {
    Serial.println("Warning: OTA authentication is disabled");
  }

  ArduinoOTA.onStart([]() {
    resumeOutputAfterOta = rmtOutputEnabled();
    if (resumeOutputAfterOta) {
      rmtSetOutputEnabled(false);
    }
    Serial.println(ArduinoOTA.getCommand() == U_FLASH
                       ? "Starting firmware update"
                       : "Starting filesystem update");
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA update complete"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    const unsigned int percent =
        total == 0 ? 0 : static_cast<unsigned int>(
                               (static_cast<uint64_t>(progress) * 100U) / total);
    Serial.printf("OTA progress: %u%%\r", percent);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error %u\n", error);
    if (resumeOutputAfterOta) {
      rmtSetOutputEnabled(true);
    }
  });
  ArduinoOTA.begin();
}

void ota_loop() {
  ArduinoOTA.handle();
}
