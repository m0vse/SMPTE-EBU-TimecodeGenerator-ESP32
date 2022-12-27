/* Copyright (c) 2011, 2018 Dirk-Willem van Gulik, All Rights Reserved.
 *                    dirkx(at)webweaving(dot)org
 *
 * This file is licensed to you under the Apache License, Version 2.0 
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <ezTime.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

extern void  _setTS(unsigned char _hour, unsigned char _min, unsigned char _sec, unsigned char _frame);

#define VERSION "2.07"

#define FPS (30)

// #define WIFI_NETWORK "my network name"
// #define WIFI_PASSWD  "my password"
// #define NTP_SERVER "0.countryname.pool.ntp.org"
#define NTP_SERVER "uk.pool.ntp.org"

#ifndef NTP_SERVER
#define NTP_SERVER "time.nist.gov"
#warning "Using the USA based NIST timeserver - you propably do not want that."
#endif

// The 'red' pin is wired through a 2k2 resistor to the base of an NPN
// transistor. The latter its C is pulled up by a 1k resistor to the 5V taken
// from the internal expansion connector. And this 5 vpp-ish signal goes to
// the old red wire. The emitter of the transistor is to the ground.
//
#define RED_PIN     (GPIO_NUM_13)
#define BLACK_PIN   (GPIO_NUM_12)

// We've got a 680 Ohm resitor to ground on the analog clock boards (Leitch 5100 series); and none on
// the board for the digital versions (Leitch 5212).
//
#define SENSE_PIN   (GPIO_NUM_14)

const char * name = "none-set";
const char * prefs_section = "Default";

bool dst = true; // Summertime europe
int tz = 1; // hours CET
int fiddleSeconds = 0;

unsigned char   frameCount = 0, secsCount = 0x10, minsCount = 0x20, hourCount = 0x30;

extern void ota_setup();
extern void web_setup();
extern void rmt_setup(gpio_num_t pin);
extern void nmea_setup();

extern bool nmea_active;
bool ntp_active=false;
extern bool smpterunning;


Timezone TZ;

// define tasks
void TaskOTA( void *pvParameters );
void TaskWEB( void *pvParameters );
void TaskRMT( void *pvParameters );
void TaskNMEA( void *pvParameters );
void TaskNTP( void *pvParameters );

TaskHandle_t xRMTHandle = NULL;
TaskHandle_t xOTAHandle = NULL;
TaskHandle_t xWEBHandle = NULL;
TaskHandle_t xNMEAHandle = NULL;
TaskHandle_t xNTPHandle = NULL;

Preferences prefs;
String wifi_network;
String wifi_passwd;


void setup() {
  Serial.begin(115200);
  Serial.print("Booting ");
  Serial.println(__FILE__);
  Serial.println(__DATE__ " " __TIME__);

  pinMode(SENSE_PIN, INPUT_PULLUP);

  if (digitalRead(SENSE_PIN))
    name = "smpte-digital-clock";
  else
    name = "smpte-analog-clock";

  Serial.printf("Detected model %s\n", name);

  pinMode(BLACK_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);

  digitalWrite(BLACK_PIN, LOW);

  // Find the stored WiFi SSID/Key
  prefs.begin(prefs_section,false);
  wifi_network=prefs.getString("wifi_network","notfound");
  wifi_passwd=prefs.getString("wifi_passwd","");
  prefs.end();
  
  delay(500);

  /* As multiple tasks rely on network access, start that first */
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_network.c_str(), wifi_passwd.c_str());
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Setting Soft-AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(name,"");
    
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ota_setup();
  web_setup();
  rmt_setup(RED_PIN); 
  nmea_setup();

  setDebug(INFO);
  setInterval(60);
  waitForSync(5); // Only wait for 5 seconds as we might be using another method to get time.
  //setInterval(0);
  //setServer(NTP_SERVER)  Serial.println("UTC: " + UTC.dateTime());
  if (!TZ.setCache(prefs_section,"TimeZone")) TZ.setLocation(F("Europe/London"));
  TZ.setDefault();
  Serial.println("LOCAL: " + TZ.dateTime());
  
  // Even if we haven't got a synced time, still update it.  
  _setTS(hour(), minute(), second(),(ms()*FPS)/1000);
  xTaskCreate(TaskRMT, "TaskRMT", 4096, NULL, 2, &xRMTHandle);
  xTaskCreate(TaskOTA, "TaskOTA", 2048, NULL, 0, &xOTAHandle);
  xTaskCreate(TaskWEB, "TaskWEB", 4096, NULL, 0, &xWEBHandle);
  xTaskCreate(TaskNMEA, "TaskNMEA", 2048, NULL, 2, &xNMEAHandle);
  xTaskCreate(TaskNTP, "TaskNTP", 2048, NULL, 1, &xNTPHandle);
  
}

void loop() { //Not used
}

extern void  rmt_loop();
void TaskRMT( void *pvParameters )
{
  (void) pvParameters;  

  for (;;) // A Task shall never return or exit.
  {
    rmt_loop();
    vTaskDelay(2);  // one tick delay (15ms) in between reads for stability
  }
}

extern void  ota_loop();
void TaskOTA( void *pvParameters )
{
  (void) pvParameters;


  for (;;) // A Task shall never return or exit.
  {
    ota_loop();
    vTaskDelay(10);  // one tick delay (15ms) in between reads for stability
  }
}

extern void  web_loop();
void TaskWEB( void *pvParameters )
{
  (void) pvParameters;  


  for (;;) // A Task shall never return or exit.
  {
    web_loop();
    vTaskDelay(5);  // one tick delay (15ms) in between reads for stability
  }
}

extern void nmea_output(void);

void TaskNTP( void *pvParameters )
{
  (void) pvParameters;  

  for (;;) // A Task shall never return or exit.
  {
    events();
    if (!ntp_active && timeStatus()==timeSet){
      ntp_active=true;
      setInterval(0);
    }
    if (!nmea_active && minuteChanged())
      _setTS(hour(), minute(), second(),(ms()*FPS)/1000);

    vTaskDelay(10);  // one tick delay (15ms) in between reads for stability
  }
}

extern void  nmea_loop();
void TaskNMEA( void *pvParameters )
{
  (void) pvParameters;  

  for (;;) // A Task shall never return or exit.
  {
    nmea_loop();
    if (nmea_active && !ntp_active && minuteChanged())
      _setTS(hour(), minute(), second(),(ms()*FPS)/1000);
    
    vTaskDelay(2);  // one tick delay (15ms) in between reads for stability
  }
}
