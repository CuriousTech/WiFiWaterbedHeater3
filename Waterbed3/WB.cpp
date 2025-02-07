#include "WB.h"
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include "display.h"
#include "eeMem.h"
#include "RunningMedian.h"
#include "TempArray.h"
#include "Radar.h"

//#define SDEBUG

extern void consolePrint(String s);

WB::WB()
{
}

void WB::init()
{
  pinMode(HEAT, OUTPUT);
  setHeat(); // Make sure it's off

  ds.begin(DS18B20);
  if( ds.search(ds_addr) )
  {
#ifdef SDEBUG
    ets_printf("OneWire device: "); // 28 22 92 29 7 0 0 6B
    for( int i = 0; i < 8; i++)
      ets_printf("%x ", ds_addr[i]);
    ets_printf("\r\n");
    if( OneWire::crc8( ds_addr, 7) != ds_addr[7])
      ets_printf("Invalid CRC\r\n");
#endif
  }
  else
  {
#ifdef SDEBUG
    ets_printf("No OneWire devices\r\n");
#endif
  }
}

void WB::setHeat()
{
  digitalWrite(HEAT, m_bHeater);
}

void WB::service() //  1 second
{
  static bool bInit = false;
  if(!bInit && year() > 2023)
  {
    bInit = true;
    getSeason();
    checkSched(true);  // initialize
  }
  
  static RunningMedian<uint16_t, 32> tempMedian;
  static RunningMedian<uint32_t, 8> heatTimeMedian;
  static RunningMedian<uint32_t, 8> coolTimeMedian;
  static uint8_t state;
  static uint8_t nErrCnt;
  static uint32_t onCounter;

  switch (state)
  {
    case 0: // start a conversion
      ds.reset();
      ds.select(ds_addr);
      ds.write(0x44, 0);   // start conversion, no parasite power on at the end
      state++;
      return;
    case 1:
      state++;
      break;
    case 2:
      state++;
      return;
    case 3:
      state = 0; // 2 second rest
      return;
  }

  uint8_t data[10];
  uint8_t present = ds.reset();
  IPAddress ip; // blank

  if (!present)     // safety
  {
    m_bHeater = false;
    setHeat();
    static String s = "WARNING\nDS18 not detected";
    consolePrint("DS18 not present");
    display.Notify((char *)s.c_str(), ip);
    return;
  }

  ds.select(ds_addr);
  ds.write(0xBE);         // Read Scratchpad

  for ( int8_t i = 0; i < 9; i++)          // we need 9 bytes
    data[i] = ds.read();
  
  if (OneWire::crc8( data, 8) != data[8]) // bad CRC
  {

    if(nErrCnt < 4) // Sometimes I get interference from a dimmer as it's ramping
    {
      nErrCnt++;
      return;
    }

    m_bHeater = false;
    setHeat();
    consolePrint("DS18 Invalid CRC");
    display.Notify("WARNING\nDS18 CRC error", ip);
    return;
  }

  uint16_t raw = (data[1] << 8) | data[0];

  if (raw > 630 || raw < 200)  // first reading is always 1360 (0x550)
  {
    consolePrint("DS18 Error");
    display.Notify("WARNING\nDS18 error", ip);
    return;
  }

  nErrCnt = 0;

  if (ee.bCF)
    tempMedian.add(( raw * 625) / 1000 );  // to 10x celcius
  else
    tempMedian.add( (raw * 1125) / 1000 + 320) ; // 10x fahrenheit

  float t;
  tempMedian.getAverage(2, t);
  uint16_t newTemp = t;
  static uint16_t oldHT;
  static bool bLastOn;

  if (m_currentTemp == 0 || oldHT == 0) // skip first read
  {
    m_currentTemp = newTemp;
    oldHT = m_hiTemp;
    return;
  }

  if (newTemp <= m_loTemp && m_bHeater == false)
  {
    m_bHeater = true;
    setHeat();
    ta.add();
  }
  else if(newTemp >= m_hiTemp && m_bHeater == true)
  {
    m_bHeater = false;
    setHeat();
    nHeatETA = 0;
    nOvershootCnt = 1;
    nOvershootStartTemp = newTemp;
    ta.add();
  }

  if(newTemp == m_currentTemp && m_hiTemp == oldHT)
    return;

  int16_t chg = newTemp - m_currentTemp;

  if(m_bHeater)
  {
    if(nHeatCnt > 120 && chg > 0)
    {
      if(!ee.bEco || !bBoost) // don't add slower heating
      {
        heatTimeMedian.add(nHeatCnt);
      }
      float fCnt;
      heatTimeMedian.getAverage(fCnt);
      uint32_t ct = fCnt;
      int16_t tDiff = m_hiTemp - newTemp;
      nHeatETA = ct * tDiff;
      int16_t ti = hour() * 60 + minute() + (nHeatETA / 60);

      int16_t tt = tempAtTime( ti ); // get real target temp
      tDiff = tt - newTemp;

      if (tDiff < 0) tDiff = 0;
      nHeatETA = ct * tDiff;
      nHeatCnt = 0;
    }
    else
    {
    }
  }
  else if (nCoolCnt > 120)
  {
    if (chg < 0)
    {
      int16_t tDiff = newTemp - m_loTemp;
      coolTimeMedian.add(nCoolCnt);
      float fCnt;
      coolTimeMedian.getAverage(fCnt);
      uint32_t ct = fCnt;
      nCoolETA = ct * tDiff;
      nCoolCnt = 0;
      if (nOvershootPeak)
      {
        nOvershootTime = nOvershootPeak;
        nOvershootTempDiff = nOvershootEndTemp - nOvershootStartTemp;

        if (ee.nOvershootTempDiff == 0) // fix for divide by 0
          ee.nOvershootTempDiff = 1;
        if (nOvershootTempDiff == 0) // fix for divide by 0
          nOvershootTempDiff = 1;
        if ((ee.nOvershootTime == 0) || (ee.nOvershootTime && (ee.nOvershootTime / ee.nOvershootTempDiff) < (nOvershootTime / nOvershootTempDiff)) ) // Try to eliminate slosh rise
        {
          ee.nOvershootTempDiff = nOvershootTempDiff;
          ee.nOvershootTime = nOvershootTime;
        }
      }
      nOvershootCnt = 0;
      nOvershootPeak = 0;
    }
    else if (nOvershootCnt) // increasing temp after heater off
    {
      nOvershootPeak = nOvershootCnt;
      nOvershootEndTemp = newTemp;
    }
  }

  m_currentTemp = newTemp;
  oldHT = m_hiTemp;

  static uint32_t ms;
  uint32_t ms2 = millis();

  if(ms2 - ms > 1000)
  {
    ms = ms2;
    if (m_bHeater)
      onCounter++;
    else if (onCounter)
    {
      ee.tSecsMon[month() - 1] += onCounter;
      onCounter = 0;
    }
    if (m_bHeater == false && nOvershootCnt)
      nOvershootCnt++;
    static uint16_t s = 1;
    if (ee.bEco && m_bHeater) // eco mode
    {
      bBoost = (m_currentTemp < m_loTemp - ee.pids[2]); // 0.5 deg diff
      if (bBoost == false)
      {
        if (--s == 0)
        {
          bool bOn = m_bHeater;
          s = (bOn) ? ee.pids[0] : ee.pids[1]; // off 60 secs /on 180 secs (75%)
          m_bHeater = !bOn;
          setHeat();
        }
      }
    }

    if (m_bHeater != bLastOn || onCounter > (60 * 60 * 12)) // total up when it turns off or before 32 bit carry error
    {
      if (bLastOn)
      {
        ee.update( false );
      }
      bLastOn = m_bHeater;
      ee.tSecsMon[month() - 1] += onCounter;
      onCounter = 0;
    }
    if (m_bHeater)
      nHeatCnt++;
    else
      nCoolCnt++;
    if (nHeatETA)
      nHeatETA--;
    if (nCoolETA)
      nCoolETA--;

#ifdef RADAR_H
    static uint8_t lastState;
    if( ((radar.m_bInBed<<1) | radar.m_bPresence) != lastState)
    {
      lastState = (radar.m_bInBed<<1) | radar.m_bPresence;
      if(lastState == 1) // log out of bed change
        ta.add();
    }
#endif
  }
}

void WB::changeTemp(int delta, bool bAll)
{
  if (ee.bVaca) return;

  if (bAll)
  {
    for (int i = 0; i < ee.schedCnt[m_season]; i++)
      ee.schedule[m_season][i].setTemp += delta;
  }
  else if (ee.bAvg) // bump both used in avg mode
  {
    ee.schedule[m_season][m_schInd].setTemp += delta;
    ee.schedule[m_season][ (m_schInd + 1) % ee.schedCnt[m_season]].setTemp += delta;
  }
  else
  {
    ee.schedule[m_season][m_schInd].setTemp += delta;
  }
  checkLimits();
  checkSched(false);     // update temp
}

void WB::checkLimits()
{
  for (int i = 0; i < ee.schedCnt[m_season]; i++)
  {
    if (ee.bCF)
      ee.schedule[m_season][i].setTemp = constrain(ee.schedule[m_season][i].setTemp, 155, 322); // sanity check (15.5~32.2)
    else
      ee.schedule[m_season][i].setTemp = constrain(ee.schedule[m_season][i].setTemp, 600, 900); // sanity check (60~90)
    ee.schedule[m_season][i].thresh = constrain(ee.schedule[m_season][i].thresh, 1, 100); // (1~10)
  }
}

void WB::checkSched(bool bUpdate)
{
  long timeNow = (hour() * 60) + minute();

  if (bUpdate)
  {
    m_schInd = ee.schedCnt[m_season] - 1;
    for (int i = 0; i < ee.schedCnt[m_season]; i++) // any time check
      if (timeNow >= ee.schedule[m_season][i].timeSch && timeNow < ee.schedule[m_season][i + 1].timeSch)
      {
        m_schInd = i;
        break;
      }
  }
  else for (int i = 0; i < ee.schedCnt[m_season]; i++) // on-time check
    {
      if (timeNow == ee.schedule[m_season][i].timeSch)
      {
        m_schInd = i;
        break;
      }
    }

  m_hiTemp = ee.bVaca ? ee.vacaTemp : ee.schedule[m_season][m_schInd].setTemp;
  m_loTemp = ee.bVaca ? (m_hiTemp - 10) : (m_hiTemp - ee.schedule[m_season][m_schInd].thresh);
  int thresh = ee.schedule[m_season][m_schInd].thresh;

  if (!ee.bVaca && ee.bAvg) // averageing mode
  {
    int start = ee.schedule[m_season][m_schInd].timeSch;
    int range;
    int s2;

    // Find minute range between schedules
    if (m_schInd == ee.schedCnt[m_season] - 1) // rollover
    {
      s2 = 0;
      range = ee.schedule[m_season][s2].timeSch + (24 * 60) - start;
    }
    else
    {
      s2 = m_schInd + 1;
      range = ee.schedule[m_season][s2].timeSch - start;
    }

    int m = (hour() * 60) + minute(); // current TOD in minutes

    if (m < start) // offset by start of current schedule
      m -= start - (24 * 60); // rollover
    else
      m -= start;

    m_hiTemp = tween(ee.schedule[m_season][m_schInd].setTemp, ee.schedule[m_season][s2].setTemp, m, range);
    thresh = tween(ee.schedule[m_season][m_schInd].thresh, ee.schedule[m_season][s2].thresh, m, range);
    m_loTemp = m_hiTemp - thresh;
  }
}

uint16_t WB::tempAtTime(uint16_t timeTo) // in minutes
{
  uint8_t idx = ee.schedCnt[m_season] - 1;
  uint16_t temp;

  if (ee.bVaca)
  {
    return ee.vacaTemp;
  }

  timeTo %= (24 * 60);

  for (int i = 0; i < ee.schedCnt[m_season]; i++) // any time check
    if (timeTo >= ee.schedule[m_season][i].timeSch && timeTo < ee.schedule[m_season][i + 1].timeSch)
    {
      idx = i;
      break;
    }

  if (!ee.bAvg) // not averageing mode
  {
    return ee.schedule[m_season][idx].setTemp;
  }

  int start = ee.schedule[m_season][idx].timeSch;
  int range;
  int s2;

  // Find minute range between schedules
  if (idx == ee.schedCnt[m_season] - 1) // rollover
  {
    s2 = 0;
    range = ee.schedule[m_season][s2].timeSch + (24 * 60) - start;
  }
  else
  {
    s2 = idx + 1;
    range = ee.schedule[m_season][s2].timeSch - start;
  }

  if (timeTo < start) // offset by start of current schedule
    timeTo -= start - (24 * 60); // rollover
  else
    timeTo -= start;
  return tween(ee.schedule[m_season][idx].setTemp, ee.schedule[m_season][s2].setTemp, timeTo, range);
}

// avarge value at current minute between times
int WB::tween(int t1, int t2, int m, int range)
{
  if (range == 0) range = 1; // div by zero check
  int t = (t2 - t1) * (m * 100 / range) / 100;
  return t + t1;
}

void WB::getSeason()
{
    tmElements_t tm;
    breakTime(now(), tm);
    tm.Month = 1; // set to first day of the year
    tm.Day = 1;
    time_t boy = makeTime(tm);
    uint16_t doy = (now()- boy) / 60 / 60 / 24; // divide seconds into days

    if(doy < ee.scheduleDays[0] || doy > ee.scheduleDays[3]) // winter
      m_season = 3;
    else if(doy < ee.scheduleDays[1]) // spring
      m_season = 0;
    else if(doy < ee.scheduleDays[2]) // summer
      m_season = 1;
    else
      m_season = 2;
}
