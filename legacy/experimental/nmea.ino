// Historical experimental sketch; excluded from the PlatformIO build.
/* Copyright (c) 2020 Phil Taylor All Rights Reserved.
                      phil(at)m0vse(dot)uk

   This file is licensed to you under the Apache License, Version 2.0
   (the "License"); you may not use this file except in compliance with
   the License.  You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

   See the License for the specific language governing permissions and
   limitations under the License.

*/
// Connect to NMEA stream over TCP, could also be adapted for serial use



#include <WiFi.h>
#include <WiFiClient.h>
#include <MicroNMEA.h>
#include "RemoteDebug.h"

extern void  _setTS(unsigned char _hour, unsigned char _min, unsigned char _sec, unsigned char _frame);
extern void  setTS(unsigned char _hour, unsigned char _min, unsigned char _sec);

WiFiClient nmeainput;
const char * nmeaHost = "192.168.99.12";
const int nmeaPort = 950;
volatile bool nmea_active=false;

char nmeaBuffer[100];
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));

void nmea_setup(){
  if (!nmeainput.connect(nmeaHost, nmeaPort)) {
        Serial.println("nmea connection failed");
        return;
    }
}

void nmea_loop(){

  static int isset=-1;

  if (nmeainput.available()) {
    xSemaphoreTake(xWiFi,0);
    nmea.process(nmeainput.read());
    xSemaphoreGive(xWiFi);
    nmea_active=true;
    if (nmea.isValid() && nmea.getMinute() !=isset) {
        debugI("GPS Valid: %s Date/time: %04d/%02d/%02d %02d:%02d:%02d.%02d",nmea.isValid() ? "yes" : "no",nmea.getYear(),nmea.getMonth(),nmea.getDay(), nmea.getHour(), nmea.getMinute(), nmea.getSecond(), nmea.getHundredths());
        UTC.setTime(nmea.getHour(),nmea.getMinute(),nmea.getSecond(),nmea.getDay(),nmea.getMonth(),nmea.getYear());
        _setTS(hour(), minute(), second(),(ms()*FPS)/1000);
        isset=nmea.getMinute();
    }
  }
}
