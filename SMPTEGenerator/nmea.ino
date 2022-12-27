// Connect to NMEA stream over TCP, could also be adapted for serial use

#include <WiFi.h>
#include <WiFiClient.h>
#include <MicroNMEA.h>

extern void  _setTS(unsigned char _hour, unsigned char _min, unsigned char _sec, unsigned char _frame);

WiFiClient nmeainput;
const char * nmeaHost = "192.168.99.12"; 
const int nmeaPort = 950;
bool nmea_active=false;

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
    nmea.process(nmeainput.read());
    nmea_active=true;
    if (nmea.isValid() && nmea.getMinute() !=isset) {
      //if (nmea.getHour() != UTC.hour() || nmea.getMinute() != UTC.minute() || nmea.getSecond() != UTC.second()) 
      //{
        time_t tm=makeTime(nmea.getHour(),nmea.getMinute(),nmea.getSecond(),nmea.getDay(),nmea.getMonth(),nmea.getYear());
        UTC.setTime(tm);
        _setTS(hour() ,minute(), second(), (nmea.getHundredths()*FPS)/100);
        nmea_output();
        isset=nmea.getMinute();
      //}
    }
  }
}

void nmea_output() 
{
     // Output GPS information from previous second
     Serial.print("Valid fix: ");
     Serial.println(nmea.isValid() ? "yes" : "no");
/*
     Serial.print("Nav. system: ");
     if (nmea.getNavSystem())
      Serial.println(nmea.getNavSystem());
     else
      Serial.println("none");

     Serial.print("Num. satellites: ");
     Serial.println(nmea.getNumSatellites());

     Serial.print("HDOP: ");
     Serial.println(nmea.getHDOP()/10., 1);
*/
     Serial.print("Date/time: ");
     Serial.print(nmea.getYear());
     Serial.print('-');
     Serial.print(int(nmea.getMonth()));
     Serial.print('-');
     Serial.print(int(nmea.getDay()));
     Serial.print('T');
     Serial.print(int(nmea.getHour()));
     Serial.print(':');
     Serial.print(int(nmea.getMinute()));
     Serial.print(':');
     Serial.print(int(nmea.getSecond()));
     Serial.print('.');
     Serial.println(int(nmea.getHundredths()));
/*
     long latitude_mdeg = nmea.getLatitude();
     long longitude_mdeg = nmea.getLongitude();
     Serial.print("Latitude (deg): ");
     Serial.println(latitude_mdeg / 1000000., 6);

     Serial.print("Longitude (deg): ");
     Serial.println(longitude_mdeg / 1000000., 6);

     long alt;
     Serial.print("Altitude (m): ");
     if (nmea.getAltitude(alt))
       Serial.println(alt / 1000., 3);
     else
       Serial.println("not available");

     Serial.print("Speed: ");
     Serial.println(nmea.getSpeed() / 1000., 3);
     Serial.print("Course: ");
     Serial.println(nmea.getCourse() / 1000., 3);
*/
  
}
