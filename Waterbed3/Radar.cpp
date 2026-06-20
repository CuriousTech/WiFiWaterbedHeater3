// Note: "Headboard" and such are hard coded here

#include "Radar.h"
#include "Display.h"
#include "jsonString.h"
#include "Lights.h"
#include "eeMem.h"

#define RADAR_SERIAL Serial

extern void WsSend(String s);
extern void sendState(void);
extern void consolePrint(String s);

Radar::Radar()
{
  
}

void Radar::init()
{
  RADAR_SERIAL.begin(256000, SERIAL_8N1);

  delay(10);
}

void Radar::service()
{
  static bool bLastPres;
  m_bPresence = read();

  if(bLastPres == m_bPresence)
    return;

  if(m_bPresence) // entered room/got up
  {
    if(m_bInBed == false && m_bLightOn == false)
    {
      display.m_backlightTimer = ee.sleepTime; // wake on entry
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

#ifdef LD2450
bool Radar::read()
{
  bool new_data = false;
  static bool bPresence;

  const int16_t nOutofBedThreshold = 1600; // ~200cm from headboard to foot
  const int16_t nInBedThreshold = 700;

  static uint8_t nNewZone;
  static uint8_t nZoneCnt;

  while (RADAR_SERIAL.available())
  {
    circular_buffer[buffer_head] = RADAR_SERIAL.read();
    buffer_head = (buffer_head + 1) % LD2450_BUFFER_SIZE;

    if (buffer_head == buffer_tail) {
        buffer_tail = (buffer_tail + 1) % LD2450_BUFFER_SIZE;
    }

    new_data = true;
  }

  if(!new_data)
    return bPresence;

  while(buffer_tail != buffer_head)
  {
    uint8_t *p = circular_buffer + buffer_tail;
    if (p[0] == 0xAA && p[1] == 0xFF)
    {
      if (buffer_tail + 30 >= LD2450_BUFFER_SIZE)
      {
        buffer_tail = 0;
        break;
      }
      if( p[28] != 0x55 && p[29] != 0xCC) // not enough yet
        break;
      p += 4;
      memcpy(radTgt, p, sizeof(radTgt));
      for (int i = 0; i < 3; i++)
      {
        if (radTgt[i].x & 0x8000)
          radTgt[i].x = -(radTgt[i].x & 0x7FFF); // little-endien, but odd sign bit 0x8001 = -1 instead of 0xFFFF
        if( (radTgt[i].y & 0x8000) == 0) // potentially behind radar, ignore
          radTgt[i].resolution = 0;
        radTgt[i].y &= 0x7FFF; // always signed in positive direction
        if (radTgt[i].speed & 0x8000)
          radTgt[i].speed = -(radTgt[i].speed & 0x7FFF);
      }
      buffer_tail = (buffer_tail + 26) % LD2450_BUFFER_SIZE; // may skip one, don't care
      break;
    }
    else
      buffer_tail = (buffer_tail + 1) % LD2450_BUFFER_SIZE;
  }

  bool bRes = false;
  for(int i = 0; i < 3; i++)
  {
    if(radTgt[i].resolution)
    {
      if(blindCheck(radTgt[i].x, radTgt[i].y))
      {
        nDistance = radTgt[i].y; //sqrt(pow(radTgt[i].x, 2) +  pow(radTgt[i].y, 2)); // actual distance (stolen from a lib on the net)
        bRes = true;
      }
    }
  }

  static uint32_t lastSent = 0;
  if( millis() - lastSent > 1000) // 4 Hz
  {
    lastSent = millis();

    jsonString js("radar");
    js.Var("t", (uint32_t)time(nullptr) );
    js.Var("zone", nZone);
    js.Var("X", radTgt[0].x);
    js.Var("Y", radTgt[0].y);
    js.Var("pres", radTgt[0].resolution ? 1:0);
    WsSend(js.Close());
  }

  int32_t rect0[4];
  int32_t rect1[4];

  rect0[0] = (ee.radarPts[3][0]-ee.radarPts[0][0]) * (radTgt[0].y-ee.radarPts[0][1]) / (ee.radarPts[3][1]-ee.radarPts[0][1]) + ee.radarPts[0][0];
  rect0[1] = (ee.radarPts[1][1]-ee.radarPts[0][1]) * (radTgt[0].x-ee.radarPts[0][0]) / (ee.radarPts[1][0]-ee.radarPts[0][0]) + ee.radarPts[0][1];
  rect0[2] = (ee.radarPts[2][0]-ee.radarPts[1][0]) * (radTgt[0].y-ee.radarPts[1][1]) / (ee.radarPts[2][1]-ee.radarPts[1][1]) + ee.radarPts[1][0];
  rect0[3] = (ee.radarPts[2][1]-ee.radarPts[3][1]) * (radTgt[0].x-ee.radarPts[2][0]) / (ee.radarPts[2][0]-ee.radarPts[3][0]) + ee.radarPts[2][1];

  rect1[0] = (ee.radarPts[7][0]-ee.radarPts[4][0]) * (radTgt[0].y-ee.radarPts[7][1]) / (ee.radarPts[7][1]-ee.radarPts[4][1]) + ee.radarPts[4][0];
  rect1[1] = (ee.radarPts[5][1]-ee.radarPts[4][1]) * (radTgt[0].x-ee.radarPts[4][0]) / (ee.radarPts[5][0]-ee.radarPts[4][0]) + ee.radarPts[4][1];
  rect1[2] = (ee.radarPts[6][0]-ee.radarPts[5][0]) * (radTgt[0].y-ee.radarPts[5][1]) / (ee.radarPts[6][1]-ee.radarPts[5][1]) + ee.radarPts[5][0];
  rect1[3] = (ee.radarPts[6][1]-ee.radarPts[7][1]) * (radTgt[0].x-ee.radarPts[6][0]) / (ee.radarPts[6][0]-ee.radarPts[7][0]) + ee.radarPts[6][1];

  if( radTgt[0].x >= rect0[0] && radTgt[0].x <= rect0[2] && radTgt[0].y >= rect0[1] && radTgt[0].y <= rect0[3])
  {
    // latch zone
    if( radTgt[0].x >= rect1[0] && radTgt[0].x <= rect1[2] && radTgt[0].y >= rect1[1] && radTgt[0].y <= rect1[3])
    {
      if(nNewZone != 0)
        nZoneCnt = 0;
      nNewZone = 0;
    }
  }
  else if(bRes) // in room, out of zone
  {
    if(nNewZone != 1)
      nZoneCnt = 0;
    nNewZone = 1;
    if(m_bLightOn)
      display.m_backlightTimer = ee.sleepTime; // keep display on with lights
  }
  else
  {
    if(nNewZone != 2)
      nZoneCnt = 0;

    nNewZone = 2;
  }

  if(nNewZone == nZone) // no change
    return bPresence;

   // Delay changing zones. If too fast, increase
  if(nZoneCnt < 200)
    nZoneCnt++;

  if(nZoneCnt < 16) // 8 may feel too fast, 16 is about 1 second
  {
    return bPresence;
  }

  nZone = nNewZone;

  switch(nZone)
  {
    case 0: // in bed
      bPresence = true;
      m_bInBed = true;
      if(display.m_backlightTimer > 2)
        display.m_backlightTimer = 2; // go dark fast
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
        display.m_backlightTimer = ee.sleepTime; // got up
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

bool Radar::blindCheck(int32_t x, int32_t y)
{
  if(x >= ee.bound/2 || x <= -(ee.bound/2) || y >= ee.bound) // outside detection bounds (visible area on page)
    return false;

  x = constrain( (x + (ee.bound/2)) * 32 / ee.bound, 0, 31);
  y = constrain( y * 32 / ee.bound, 0, 31);

  return !(ee.blindBits[y] & (1 << x));
}

#endif // LD2450
