// Note: "Headboard" and such are hard coded here

#include "Radar.h"
#include "Display.h"
#include "jsonString.h"
#include <TimeLib.h>
#include "Lights.h"

#define RADAR_SERIAL Serial

extern void WsSend(String s);
extern void sendState(void);

Radar::Radar()
{
  
}

void Radar::init()
{
  RADAR_SERIAL.begin(256000, SERIAL_8N1);

  delay(10);

  if(m_ld.begin(RADAR_SERIAL))
  {
    if(!m_ld.requestCurrentConfiguration())
    {
//      ets_printf("requestConfig failed\r\n");
    }
    else
      radarConnected = true;
  }
//  else
//    ets_printf("radar.init failed\r\n");
}

void Radar::service()
{
//  if(radarConnected == false)
//    return;

  static bool bLastPres;
  m_bPresence = read();

  if(bLastPres == m_bPresence)
    return;

  if(m_bPresence) // entered room/got up
  {
    if(m_bInBed == false && m_bLightOn == false)
    {
      display.m_backlightTimer = ee.sleepTime; // reset timer for any touch
      display.m_brightness = ee.brightLevel;
      lights.setSwitch("Dresser", 1, 0);
      lights.setSwitch("Headboard", 1, 0);
      m_bLightOn = true;
      display.checkNotif();
    }
  }
  sendState();
  bLastPres = m_bPresence;
}

bool Radar::read()
{
  const uint16_t nOutofBedThreshold = 200; // ~200cm from headboard to foot
  const uint16_t nInBedThreshold = 32;

  static uint8_t nZone = 2;
  static uint8_t nNewZone;
  static uint8_t nZoneCnt;

  static uint32_t lastReading = 0;
  static uint32_t lastSent = 0;

  static bool bPresence;
  static uint16_t nDistance = 400; // start out of zones
  static uint16_t nEnergy;

  static uint16_t rads[16];

  m_ld.read();
  if(!m_ld.isConnected() || millis() - lastReading < (150*3) ) // About the same as the radar / 3 - radar is 150ms
    return bPresence;

  lastReading = millis();

 /*
  if( millis() - lastSent > 499) // 2 Hz
  {
    lastSent = millis();

    jsonString js("radar");
    js.Var("t", (uint32_t)now());
    js.Var("bits", m_ld.presenceDetected() | (nZone<<1) | (m_bInBed<<3) | m_ld.stationaryTargetDetected()<<4 | m_ld.movingTargetDistance()<<5);
    js.Var("dist", m_ld.stationaryTargetDistance() );
    js.Var("distM", m_ld.movingTargetDistance());
    js.Var("en", m_ld.stationaryTargetEnergy());
    js.Var("enM", m_ld.movingTargetEnergy());
    WsSend(js.Close());
  }
*/

  nDistance = m_ld.stationaryTargetDistance();
  nEnergy = m_ld.stationaryTargetEnergy();

  if(m_ld.movingTargetDetected())
  {
    nDistance = m_ld.movingTargetDistance();
    nEnergy = m_ld.movingTargetEnergy();
  }

  for(uint8_t i = 0; i < 15; i++)
    rads[i] = rads[i+1];
  rads[15] = nDistance;

  if(nDistance < nInBedThreshold)
  {
    if(nNewZone != 0)
      nZoneCnt = 0;
    nNewZone = 0;
    bPresence = true;
  }
  else if( (nDistance > nOutofBedThreshold - 30 || nDistance == 30) && (nEnergy < 10 || m_ld.presenceDetected() == false )) // out of bed/room no energy
  {
    if(nNewZone != 2)
    {
/*      String s = "R";
      for(uint8_t i = 0; i < 16; i++)
      {
        s += " ";
        s += rads[i];
      }
      WsSend(s);*/
      nZoneCnt = 0;
    }
    nNewZone = 2;
  }
  else if(nDistance > nOutofBedThreshold)
  {
    if(nNewZone != 1)
      nZoneCnt = 0;
    nNewZone = 1;
  }

  if(nNewZone == nZone) // no change
    return bPresence;

 // give it time to settle
  if(nZoneCnt < 200)
    nZoneCnt++;
  if(nZoneCnt < 3)
  {
    if(nZone == 0 && nNewZone == 1 && nDistance < nOutofBedThreshold) // jump rejection
    {
      nZoneCnt = 0;
      nNewZone = 0;
    }
    return bPresence;
  }

  nZone = nNewZone;

  switch(nZone)
  {
    case 0: // in bed
      bPresence = true;
      m_bInBed = true;
      if(m_bLightOn)
      {
        lights.clearQueue();
        lights.setSwitch("Dresser", 0, 0);
        lights.setSwitch("Headboard", 0, 0);
        lights.setSwitch("BRFan", 1, 0);
        m_bLightOn = false;
      }
      break;
    case 1: // in room, out of bed
      bPresence = true;
      m_bInBed = false;
      if(m_bLightOn == false)
      {
        display.m_backlightTimer = ee.sleepTime; // reset timer for any touch
        display.m_brightness = ee.brightLevel;
        lights.clearQueue();
        lights.setSwitch("Dresser", 1, 0);
        lights.setSwitch("Headboard", 1, 0);
        lights.setSwitch("BRFan", 0, 0);
        m_bLightOn = true;
      }
      break;
    case 2: // out of room
      bPresence = false;
      m_bInBed = false;
      if(m_bLightOn)
      {
        lights.clearQueue();
        lights.setSwitch("Dresser", 0, 0);
        lights.setSwitch("Headboard", 0, 0);
        lights.setSwitch("BRFan", 0, 0);
        m_bLightOn = false;
      }
      break;
  }

  return bPresence;
}
