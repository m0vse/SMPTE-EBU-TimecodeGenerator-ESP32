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

#include "RemoteDebug.h"


#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <Preferences.h>

#include <MicroNMEA.h>


RemoteDebug Debug;
extern MicroNMEA nmea;

void telnet_setup(void)
{
  MDNS.addService("telnet", "tcp", 23);
  Debug.begin(name);
  Debug.setResetCmdEnabled(true); // Enable the reset command

  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors
  String helpCmd ="";
  helpCmd.concat("\nnmea - Toggle NMEA input");
  helpCmd.concat("\ngps - Display gps status");

  Debug.setHelpProjectsCmds(helpCmd);
  Debug.setCallBackProjectCmds(&processCmdRemoteDebug);
}

void telnet_loop(void)
{

  xSemaphoreTake(xWiFi,0);
  Debug.handle();
  xSemaphoreGive(xWiFi);
}


void processCmdRemoteDebug(void)
{
  String lastCmd = Debug.getLastCommand();
  if (lastCmd == "nmea") {
    debugI("Toggling NMEA");
  } else if (lastCmd == "gps") {
    debugI("GPS Valid: %s",nmea.isValid() ? "yes" : "no");
    debugI("Num satellites: %d HDOP: %0.1f",nmea.getNumSatellites(),nmea.getHDOP()/10.);
    debugI("Date/time: %04d/%02d/%02d %02d:%02d:%02d.%02d",nmea.getYear(),nmea.getMonth(),nmea.getDay(), nmea.getHour(), nmea.getMinute(), nmea.getSecond(), nmea.getHundredths());
    debugI("Latitude (deg): %0.6f Longitude (deg): %0.6f",nmea.getLatitude()/1000000.,nmea.getLongitude()/1000000.);
    long alt=0;
    nmea.getAltitude(alt);
    debugI("Altitude (m): %0.3f Speed: %0.3f Course: %0.1f",alt/1000.,nmea.getSpeed()/1000.,nmea.getCourse()/1000.);
  }
}
