/**The MIT License (MIT)

Copyright (c) 2024 by Greg Cunningham, CuriousTech

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Build with Arduino IDE 1.8.57.0
//  ESP32: (2.0.14) ESP32S3 Dev Module, QIO, CPU Freq: 240MHz for mp3 playback
//  Flash: 16MB
//  Partition: 8M with SPIFFS or 16 MB with FatFS (change INTERNAL_FS in Media.h to match)
//  PSRAM: OPI PSRAM
//  In TFT_eSPI/User_Setup_Select.h use #include <User_Setups/Setup303_Waveshare_ESP32S3_ST7789.h> (custom, included in repo)

#include <WiFi.h>
#include <time.h>
#include "eeMem.h"
#include "WB.h"
#include "TempArray.h"
#include "THSensor.h"
#include "Media.h"

#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include <ESPAsyncWebServer.h> // https://github.com/ESP32Async/ESPAsyncWebServer (3.7.2)
#include <JsonParse.h> // https://github.com/CuriousTech/ESP-HVAC/tree/master/Libraries/JsonParse
#include "jsonString.h"
#include "pages.h"

AsyncWebServer server( 80 );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
int WsClientID;
int WsPcClientID;
void jsonCallback(int16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
bool bKeyGood;
IPAddress lastIP;
int nWrongPass;
TempArray ta;
THSensor ths;
Media media;
tm gLTime;

#define TZ  "EST5EDT,M3.2.0,M11.1.0"  // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

#include "display.h"
#include "Lights.h" // Uses ~3KB

#ifdef RADAR_H // defined in WB.h
Radar radar;
#endif

Lights lights;
const char *hostName = "WaterbedM"; // Device and OTA name

bool bConfigDone = false; // EspTouch done or creds set
bool bStarted = false; // first start
bool bRestarted = false; // consecutive restarts

Display display;
eeMem ee;
WB wb;

void consolePrint(String s)
{
  jsonString js("print");
  js.Var("text", s);
  ws.textAll(js.Close());
}

void WsSend(String s)
{
  ws.textAll(s);
}

bool secondsWiFi() // called once per second
{
  bool bConn = false;

  if(!bConfigDone)
  {
    if( WiFi.smartConfigDone())
    {
      bConfigDone = true;
    }
  }
  if(bConfigDone)
  {
    if(WiFi.status() == WL_CONNECTED)
    {
      if(!bStarted)
      {
        MDNS.begin( hostName );
        bStarted = true;
        MDNS.addService("iot", "tcp", 80);
        WiFi.SSID().toCharArray(ee.szSSID, sizeof(ee.szSSID)); // Get the SSID from SmartConfig or last used
        WiFi.psk().toCharArray(ee.szSSIDPassword, sizeof(ee.szSSIDPassword) );
        lights.init();
        bConn = true;
      }
      if(!bRestarted) // wakeup work if needed
      {
        bRestarted = true;
// todo: reconnect to host
      }
    }
    else if(WiFi.status() == WL_CONNECT_FAILED) // failed to connect for some reason
    {
      WiFi.mode(WIFI_AP_STA);
      WiFi.beginSmartConfig();
      bConfigDone = false;
      bStarted = false;
    }
  }

  if(WiFi.status() != WL_CONNECTED)
    return bConn;

  return bConn;
}

// Currently using PC to relay
bool sendStatCmd( uint16_t *pCode)
{
  display.RingIndicator(2);
  jsonString js("STAT");
  js.Var("value", pCode[0]);
  WsSend(js.Close());
  return true;
}

const char *jsonListCmd[] = {
  "key",
  "TZ",
  "avg",
  "cnt",
  "tadj",
  "ppkwh",
  "vaca",
  "vacatemp",
  "I",
  "N",
  "S", // 10
  "T",
  "H",
  "watts",
  "save",
  "aadj",
  "eco",
  "outtemp",
  "outrh",
  "notif",
  "notifCancel", // 20
  "rate",
  "hostip",
  "lightip",
  "music",
  "restart",
  "ST",
  "STATTEMP",
  "STATSETTEMP",
  "STATFAN",
  "setfs", // 30
  "cd",
  "delf",
  "createdir",
  "radarpts",
  "blindbits",
  "bound",
  NULL
};

// this is for the web page, and needs to be seperated from the PC client
void jsonCallback(int16_t iName, int iValue, char *psValue)
{
  if (!bKeyGood && iName)
  {
    if (nWrongPass == 0)
      nWrongPass = 10;
    else if ((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    return; // only allow for key
  }

  char *p, *p2;
  static int item = 0;

  switch (iName)
  {
    case 0: // key
      if (!strcmp(psValue, ee.iotPassword)) // first item must be key
        bKeyGood = true;
      break;
    case 1: // TZ
      break;
    case 2: // avg
      ee.bAvg = iValue;
      break;
    case 3: // cnt
      ee.schedCnt[wb.m_season] = constrain(iValue, 1, 8);
      ws.textAll(setupJson()); // update all the entries
      break;
    case 4: // tadj
      wb.changeTemp(iValue, false);
      ws.textAll(setupJson()); // update all the entries
      break;
    case 5: // ppkw
      ee.ppkwh = iValue;
      break;
    case 6: // vaca
      ee.bVaca = iValue;
      break;
    case 7: // vacatemp
      if (ee.bCF)
        ee.vacaTemp = constrain( (int)(atof(psValue) * 10), 100, 290); // 10-29C
      else
        ee.vacaTemp = constrain( (int)(atof(psValue) * 10), 600, 840); // 60-84F
      break;
    case 8: // I
      item = iValue;
      break;
    case 9: // N
      break;
    case 10: // S
      p = strtok(psValue, ":");
      p2 = strtok(NULL, "");
      if (p && p2) {
        iValue *= 60;
        iValue += atoi(p2);
      }
      ee.schedule[wb.m_season][item].timeSch = iValue;
      break;
    case 11: // T
      ee.schedule[wb.m_season][item].setTemp = atof(psValue) * 10;
      break;
    case 12: // H
      ee.schedule[wb.m_season][item].thresh = (int)(atof(psValue) * 10);
      wb.checkLimits();      // constrain and check new values
      wb.checkSched(true);   // reconfigure to new schedule
      break;
    case 13: // watts
      ee.watts = iValue;
      break;
    case 14: // save
      ee.update(false);
      break;
    case 15: // aadj
      wb.changeTemp(iValue, true);
      ws.textAll(setupJson()); // update all the entries
      break;
    case 16: // eco
      ee.bEco = iValue ? true : false;
      wb.setHeat();
      break;
    case 17: // outtemp
      display.m_outTemp = iValue;
      break;
    case 18: // outrh
      display.m_outRh = iValue;
      break;
    case 19: // notif
      display.Notify(psValue, lastIP);
      break;
    case 20: // notifCancel
      display.NotificationCancel(lastIP);
      break;
    case 21: // rate
      if (iValue == 0)
        break; // don't allow 0
      ee.rate = iValue;
      sendState();
      break;
    case 22: // host IP / port  (call from host with ?hostip=80)
      ee.hostIp[0] = lastIP[0];
      ee.hostIp[1] = lastIP[1];
      ee.hostIp[2] = lastIP[2];
      ee.hostIp[3] = lastIP[3];
      ee.hostPort = iValue ? iValue : 80;
      break;
    case 23: // lightIP
      {
        IPAddress ip;
        ip.fromString(psValue);
        ee.lights[0].ip[0] = ip[0];
        ee.lights[0].ip[1] = ip[1];
        ee.lights[0].ip[2] = ip[2];
        ee.lights[0].ip[3] = ip[3];
      }
      break;
    case 24: // music
      media.Play("/", psValue);
      break;
    case 25: // restart
      ESP.restart();
      break;
    case 26: // ST
      ee.sleepTime = iValue;
      break;
    case 27: // STATTEMP
      display.m_statTemp = iValue;
      break;
    case 28: // STATSETTEMP
      display.m_statSetTemp = iValue;
      break;
    case 29: // STATFAN
      display.m_bStatFan = iValue;
      break;

    case 30: // setfs
      media.setFS(psValue);
      ws.textAll(setupJson()); // update FS and disk free
      break;
    case 31: // cd
      media.setPath(psValue);
      break;
    case 32: // delf
      media.deleteFile(psValue);
      ws.textAll(setupJson()); // update disk free
      break;
    case 33: // createdir
      media.createDir(psValue);
      ws.textAll(setupJson()); // update disk free
      break;
    case 34: // radarpts
      parseRadarPoints(psValue);
      break;
    case 35: // blindbits
      parseBlindbits(psValue);
      break;
    case 36: // bound
      ee.bound = constrain(iValue, 1000, 12000);
      break;
  }
}

void parseRadarPoints(char *p)
{
  char *token = strtok(p, ",");

  for(uint8_t n = 0; token && n < 8; n++)
  {
    ee.radarPts[n][0] = atoi(token);
    token = strtok(NULL, ",");
    if(token)
      ee.radarPts[n][1] = atoi(token);
    token = strtok(NULL, ",");
  }
}

void parseBlindbits(char *p)
{
  char *token = strtok(p, ",");

  for(uint8_t n = 0; token && n < 32; n++)
  {
    ee.blindBits[n] = atoi(token);
    token = strtok(NULL, ",");
  }
}

void parseParams(AsyncWebServerRequest *request)
{
  int16_t val;

  if (nWrongPass && request->client()->remoteIP() != lastIP) // if different IP drop it down
    nWrongPass = 10;

  lastIP = request->client()->remoteIP();

  for ( uint8_t i = 0; i < request->params(); i++ )
  {
    const AsyncWebParameter* p = request->getParam(i);
    String s = request->urlDecode(p->value());

    uint8_t idx;
    for (idx = 0; jsonListCmd[idx]; idx++)
      if ( p->name().equals(jsonListCmd[idx]) )
        break;
    if (jsonListCmd[idx] == NULL)
      return;
    int iValue = s.toInt();
    if (s == "true") iValue = 1;

    jsonCallback(idx, iValue, (char *)s.c_str());
  }
}

void startWiFi()
{
  if(WiFi.status() == WL_CONNECTED)
    return;

  WiFi.setSleep(false);

  WiFi.hostname(hostName);
  WiFi.mode(WIFI_STA);

  if ( ee.szSSID[0] )
  {
    WiFi.begin(ee.szSSID, ee.szSSIDPassword);
    WiFi.setHostname(hostName);
    bConfigDone = true;
  }
  else
  {
    IPAddress ip;
    WiFi.beginSmartConfig();
    display.Notify("Use EspTouch to configure\nWiFi connection", ip);
  }

  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    IPAddress ip;
    display.toast("OTA Update");
    ee.update(false);
    consolePrint("OTA Update Started");
    delay(100);
  });

  static bool bInit = false;
  if(!bInit)
  {
    bInit = true;
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
      request->send_P( 200, "text/html", "Nope");
    });

    server.on ( "/iot", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest * request)
    {
      parseParams(request);
      if(INTERNAL_FS.exists("/index.html")) // override the hard coded page
          request->send(INTERNAL_FS, "/index.html");
      else
        request->send_P( 200, "text/html", index_page);
    });

    server.on ( "/fm.html", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest * request)
    {
      request->send_P( 200, "text/html", fileman);
    });

    server.on ( "/upload", HTTP_POST, [](AsyncWebServerRequest * request)
    {
      request->send( 200);
      ws.textAll(setupJson()); // update free space
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
     {
      if(!index)
        request->_tempFile = media.createFile(filename);
      if(len)
       request->_tempFile.write((byte*)data, len);
      if(final)
        request->_tempFile.close();
     }
    );

    server.on ( "/s", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest * request)
    {
      parseParams(request); // no repsonse, just take params
    });

    server.onNotFound([](AsyncWebServerRequest * request) { // make it quiet to the outside world
//      request->send(404);
    });

    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest * request) { // just for checking
      request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });
  
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    });

    server.on("/del-btn.png", HTTP_GET, [](AsyncWebServerRequest *request){
      AsyncWebServerResponse *response = request->beginResponse_P(200, "image/png", delbtn_png, sizeof(delbtn_png));
      request->send(response);
    });

    server.serveStatic("/", INTERNAL_FS, "/");
    server.serveStatic("/sd", SD_MMC, "/");

    server.begin();
    jsonParse.setList(jsonListCmd);
  }
}

void serviceWiFi()
{
// Handle OTA server.
  ArduinoOTA.handle();
}

String dataJson()
{
  jsonString js("state");
  js.Var("t", (long)time(nullptr) );

  js.Var("waterTemp", String((float)wb.m_currentTemp / 10, 1) );
  js.Var("setTemp", String((float)ee.schedule[wb.m_season][wb.m_schInd].setTemp / 10, 1) );
  js.Var("hiTemp",  String((float)wb.m_hiTemp / 10, 1) );
  js.Var("loTemp",  String((float)wb.m_loTemp / 10, 1) );
  js.Var("on",   wb.m_bHeater);
  js.Var("temp", String((float)ths.m_temp / 10, 1) );
  js.Var("rh",   String((float)ths.m_rh / 10, 1) );
  js.Var("c",    String((ee.bCF) ? "C" : "F"));
#ifdef RADAR_H
  js.Var("zone", radar.nZone);
  js.Var("dist", radar.nDistance);
  js.Var("ener", radar.nEnergy);
#endif
  js.Var("eta",  wb.nHeatETA);
  js.Var("cooleta",  wb.nCoolETA);

  return js.Close();
}

void remoteNotifCancelled()
{
  if(!display.m_nWsConnected)
    return;

  jsonString js("notif");
  js.Var("data", 0 );
  ws.textAll( js.Close() );
}

uint8_t ssCnt = 26;

void sendState()
{
  if (display.m_nWsConnected)
    ws.textAll( dataJson() );
  ssCnt = 58;
}

String setupJson()
{
  jsonString js("settings");
  js.Var("st", ee.sleepTime);
  js.Var("vt", String((float)ee.vacaTemp / 10, 1) );
  js.Var("avg", ee.bAvg);
  js.Var("ppkwh", ee.ppkwh);
  js.Var("vo",  ee.bVaca);
  js.Var("idx", wb.m_schInd);
  js.Array("cnt", ee.schedCnt, 4);
  js.Var("w",  ee.watts);
  js.Var("r",  ee.rate);
  js.Var("e",  ee.bEco);
  js.Var("season", wb.m_season);
  js.Array("item", ee.schedule, 4);
  js.Array("seasonDays", ee.scheduleDays, 4);
  js.Array("ts", ee.tSecsMon, 12);
  js.Var("diskfree",  media.freeSpace() );
  js.Var("sdavail",  media.SDCardAvailable() );
  js.Var("currfs", media.currFS());
  js.ArrayPts("radarpts", ee.radarPts, 8);
  js.Array("blindbits", ee.blindBits, 32);
  js.Var("bound", ee.bound);
  return js.Close();
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{ //Handle WebSocket event

  switch (type)
  {
    case WS_EVT_CONNECT:      //client connected
      client->text( setupJson() );
      client->text( dataJson() );
      client->text( ta.get() );
      display.m_nWsConnected++;
      WsClientID = client->id();
      media.setPath("/"); // trigger file list send
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      if (display.m_nWsConnected)
        display.m_nWsConnected--;
      WsClientID = 0;
      if(client->id() == WsPcClientID)
        WsPcClientID = 0;
      break;
    case WS_EVT_ERROR:    //error was received from the other end
      break;
    case WS_EVT_PONG:    //pong message was received (in response to a ping request maybe)
      break;
    case WS_EVT_DATA:  //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len) {
        //the whole message is in a single frame and we got all of it's data
        if (info->opcode == WS_TEXT) {
          data[len] = 0;
          jsonParse.process((char *)data);
        }
      }
      break;
  }
}

void setup()
{
//  delay(1000);
//  ets_printf("\nStarting\n"); // print over USB (sets baud rate to 115200, radar needs 256K)

  ee.init();  // Load EE
  display.init(); // start display
  wb.init(); // start WB
  ths.init(); // Start temp sensor
  startWiFi();
#ifdef RADAR_H
  radar.init();
#endif
  media.init();
}

void loop()
{
  static uint8_t hour_save, min_save = 255, mon_save = 255;

  display.service();  // check for touch, etc.
  media.service();
  lights.service();

#ifdef RADAR_H
  radar.service();
#endif

  serviceWiFi(); // handles OTA

  static uint32_t lastMS;

  if(millis() - lastMS > 1000) // only do stuff once per second
  {
    getLocalTime(&gLTime); // this is used globally
    
    lastMS = millis();
    if(secondsWiFi()) // once per second stuff, returns true once on connect
    {
      configTime(0, 0, "pool.ntp.org");
      setenv("TZ", TZ, 1);
      tzset();
    }

    display.oneSec();

    if(gLTime.tm_year > 124)
    {
      if(min_save != gLTime.tm_min) // only do stuff once per minute
      {
        min_save = gLTime.tm_min;
        wb.checkSched(false);     // check every minute for next schedule
  
        if(hour_save != gLTime.tm_hour) // update our IP and time daily (at 2AM for DST)
        {
          hour_save = gLTime.tm_hour;
          if(hour_save == 2 && WiFi.status() == WL_CONNECTED)
          {
            configTime(0, 0, "pool.ntp.org");
            setenv("TZ", TZ, 1);
            tzset();
          }
  
          ee.update(false);
  
          if ( mon_save != gLTime.tm_mon )
          {
            if (mon_save <= 12) // restart check
              ee.tSecsMon[gLTime.tm_mon - 1] = 0;
            mon_save = gLTime.tm_mon;
          }
          wb.getSeason();
        }

      }
      if( (min_save % 5) == 0)
        ta.add(); // 5 minute log
    }

    if (--ssCnt == 0) // keepalive
      sendState();

    if (nWrongPass)
      nWrongPass--;

    wb.service();
    ths.service();
  }
}
