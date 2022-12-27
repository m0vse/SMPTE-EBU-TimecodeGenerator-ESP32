/* Copyright (c) 2011, 2018 Dirk-Willem van Gulik, All Rights Reserved.
                      dirkx(at)webweaving(dot)org

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

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <Preferences.h>
//#include "FS.h"
//#include "SPIFFS.h"

WiFiServer server(80);
String HTTP_req;
String networks;

bool smpterunning=true;

void listNetworks(void) {
    networks=String("");
    String current;
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    networks=networks+String("<select id=\"wifi\">");

    if (n == 0) {
        networks=networks+String("<option value=\"\" selected>No networks found<option>");
    } else {
        networks=networks+String("<option value=\"\">Please select<option>");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            if (WiFi.SSID() == WiFi.SSID(i))
              current=String("selected");
            else
              current=String("");
              
            networks=networks+String("<option value=\"")+WiFi.SSID(i)+String("\" ")+ current + String(">")+WiFi.SSID(i)+String(" (")+WiFi.RSSI(i)+String(")</option>");
        }
    }
    networks=networks+String("</select>");
}

void web_setup(void)
{
  listNetworks();

  MDNS.begin(name);
  server.begin();
  MDNS.addService("http", "tcp", 80);

  /*if (!SPIFFS.begin())
    SPIFFS.format();

  File file = SPIFFS.open("/fiddle.txt", "r");
  if (!file)
    return;
  String s = file.readString();
  if (s)
    fiddleSeconds = s.toInt();
  file.close(); */
}



void SendPage(WiFiClient client)
{
   String smptenext = "Enable";
   if (smpterunning)
    smptenext=String("Disable");
   client.print( String("<html>\n\
      <head>\n\
      <script>\n\
      function getTime()\n\
      {\n\
                var nocache = \"&nocache=\" + Math.random() * 10000000;\n\
                var request = new XMLHttpRequest();\n\
                request.onreadystatechange = function()\n\
                  {\n\
                    if (this.readyState == 4) {\n\
                      if (this.status == 200) {\n\
                        if (this.responseText != null) {\n\
                          document.getElementById(\"time_txt\").innerHTML = this.responseText;\n\
                        }\n\
                      }\n\
                    }\n\
                  }\n\
                  request.open(\"GET\", \"get_time\" + nocache, true);\n\
                  request.send(null);\n\
                  setTimeout('getTime()',1000);\n\
      }\n\
      function smpte()\n\
      {\n\
                var nocache = \"&nocache=\" + Math.random() * 10000000;\n\
                var request = new XMLHttpRequest();\n\
                request.onreadystatechange = function()\n\
                  {\n\
                    if (this.readyState == 4) {\n\
                      if (this.status == 200) {\n\
                        if (this.responseText != null) {\n\
                          document.getElementById(\"smpte\").innerHTML = this.responseText;\n\
                        }\n\
                      }\n\
                    }\n\
                  }\n\
                  request.open(\"GET\", \"smpte\" + nocache, true);\n\
                  request.send(null);\n\
      }\n\
      function wifi()\n\
              \n{\
                var nocache = \"&nocache=\" + Math.random() * 10000000;\n\
                var request = new XMLHttpRequest();\n\
                var opt = document.getElementById(\"wifi\");\n\
                var psk = document.getElementById(\"key\");\n\
                request.open(\"GET\", \"wifi=\" + opt.options[opt.selectedIndex].value + \"&key=\" + wifikey.value + nocache, true);\n\
                request.send(null);\n\
      }\n\              
      function reboot()\n\
              \n{\
                var nocache = \"&nocache=\" + Math.random() * 10000000;\n\
                var request = new XMLHttpRequest();\n\
                request.open(\"GET\", \"reboot\" + nocache, true);\n\
                request.send(null);\n\
      }\n\              
      function timezone()\n\
              \n{\
                var nocache = \"&nocache=\" + Math.random() * 10000000;\n\
                var request = new XMLHttpRequest();\n\
                request.onreadystatechange = function()\n\
                  {\n\
                    if (this.readyState == 4) {\n\
                      if (this.status == 200) {\n\
                        if (this.responseText != null) {\n\
                          document.getElementById(\"tz_txt\").value = this.responseText;\n\
                        }\n\
                      }\n\
                    }\n\
                  }\n\
                  request.open(\"GET\", \"timezone=\" + document.getElementById(\"tz_txt\").value + nocache, true);\n\
                  request.send(null);\n\
      }\n\
      </script>\n\        
      </head>\n\
      <body onload=\"getTime()\">\n") +  
                 String("<h1>" + String(name) + " :: (30fps) <hr></h1>") +
                 String("<p id=\"time_txt\">BCD time: </p>") +
                 String("<p> WiFi: ") + networks + String(" Key: <input type=\"password\" id=\"wifikey\" value=\"") + String(WiFi.psk()) + String("\"> <button id=wifi onclick=\"wifi()\">Update</button></p>") +         
                 String("Timezone: <input type=\"text\" id=\"tz_txt\" value=\"") + String(TZ.getTimezoneName()) + String("\"></input> <button id=timezone onclick=\"timezone()\">Submit</button></p>") +
                 String("<form method=post>Extra adjustment: <input name='fiddle' value='") + String(fiddleSeconds) + String("'> seconds <input type=submit value=OK></form>") +
                 String("<button id=smpte onclick=\"smpte()\">") + smptenext + String(" SMPTE</button>") +
                 String(" <button id=reboot onclick=\"reboot()\">Reboot</button>") +
                 String("<pre>\n\n\n</pre><hr><font size=-3 color=gray>" VERSION)
                ); 
}


unsigned char h2int(char c)
{
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}

String urldecode(String str)
{
    
    String encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++){
        c=str.charAt(i);
      if (c == '+'){
        encodedString+=' ';  
      }else if (c == '%') {
        i++;
        code0=str.charAt(i);
        i++;
        code1=str.charAt(i);
        c = (h2int(code0) << 4) | h2int(code1);
        encodedString+=c;
      } else{
        
        encodedString+=c;  
      }
      
      yield();
    }
    
   return encodedString;
}

void web_loop(void)
{
  WiFiClient client = server.available();
  if (client) {
    boolean isBlank=true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();   
        HTTP_req += c;

        if (c == '\n' && isBlank) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: keep-alive");
          client.println();
          if (HTTP_req.indexOf("get_time") > -1) {
            client.println(String("BCD time: ") + String(hourCount, HEX) + ":" + String(minsCount, HEX) + ":" + String(secsCount, HEX));
          } else if (HTTP_req.indexOf("wifi") > -1) {
            int startwifi=HTTP_req.indexOf("wifi=")+5;
            int endwifi=HTTP_req.indexOf("&key=");
            int startkey=endwifi+5;
            int endkey=HTTP_req.indexOf("&nocache");
            String wifi = HTTP_req.substring(startwifi,endwifi);
            String key = HTTP_req.substring(startkey,endkey);
            wifi=urldecode(wifi);
            key=urldecode(key);
            Serial.println("Wifi: "+wifi+" Key: "+key);
            prefs.begin(prefs_section,false);
            prefs.putString("wifi_network",wifi);
            prefs.putString("wifi_passwd",key);
            prefs.end();
            delay(500);
            ESP.restart();
          } else if (HTTP_req.indexOf("smpte") > -1) {
            if (smpterunning)
            {
              Serial.println("Disabling SMPTE");
              vTaskSuspend(xRMTHandle);         
              client.println("Enable SMPTE");
            } else {
              Serial.println("Enabling SMPTE");
              vTaskResume(xRMTHandle);
              client.println("Disable SMPTE");
            }
            smpterunning=!smpterunning;
          } else if (HTTP_req.indexOf("reboot") > -1) {
            delay(1000);
            ESP.restart();
          } else if (HTTP_req.indexOf("timezone") > -1) {
            int starttz=HTTP_req.indexOf("timezone=")+9;
            int endtz=HTTP_req.indexOf("&nocache");
            String tz = HTTP_req.substring(starttz,endtz);
            Serial.println(tz);
            TZ.setLocation(tz);
            TZ.setDefault();
            setTS(hour() ,minute(), second()); // Force a time update to reflect tz change
            client.println(TZ.getTimezoneName());
          } else {  // HTTP request for web page
            SendPage(client);
          }
          HTTP_req = "";            // finished with request, empty string
          isBlank=true;
          return;
        }
        if (c == '\n') {
          // last character on line of received text
          // starting new line with next character read
          isBlank = true;
        } else if (c != '\r') {
          // a text character was received from client
          isBlank = false;
        }
      } // end if client.available()
    } // end while client.connected(
    delay(1); 
    client.stop();
  } // end if (client)


/*(  
  if (!client) {
    return;
  }

  String req = client.readStringUntil('\r');

  client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");

  req.toUpperCase();

  if (req.startsWith("HEAD ")) {
    return;
  } else if (req.startsWith("GET "))
  {
    SendPage(client);
  } else if (req.startsWith("POST "))
  {
    SendPage(client);
    if (client.find("disable")) {
      client.print("<p>Disabling</p>");      
    } else if (client.find("enable")) {
      client.print("<p>Enabling</p>");
    } else if (client.find("fiddle=")) {
      int f   = client.parseInt();
      File file = SPIFFS.open("/fiddle.txt", "w");
      if (!file) {
        client.print("Can't do that dave (write file)");
        return;
      };
      file.println(f);
      file.close();
      client.print("Fiddle factor stored.");
      fiddleSeconds = f;
    }
  } else {
    client.print("<h3>Invalid request</h3>");
    client.print("<p>");
    client.print(req);
    client.print("</p>");
  } */
}
