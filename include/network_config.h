#pragma once

#include <Arduino.h>

bool saveWifiConfiguration(const String &ssid, const String &password);
String configuredWifiSsid();
String configuredWifiPassword();
bool wifiProvisioningActive();
void wifi_loop();
