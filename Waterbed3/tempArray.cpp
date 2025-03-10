#include "display.h"
#include "tempArray.h"
#include "eeMem.h"
#include "jsonstring.h"
#include <TimeLib.h>
#include <TFT_eSPI.h> // TFT_eSPI library from library manager?
#include "WB.h"
#include "THSensor.h"

extern TFT_eSPI tft;
extern TFT_eSprite sprite;

extern void consolePrint(String s);

void TempArray::add()
{
  int iPos = hour() * 12 + minute() / 5;

  m_log[iPos].min = (hour() * 60) + minute();
  m_log[iPos].temp = wb.m_currentTemp;
  m_log[iPos].state = wb.m_bHeater;
#ifdef RADAR_H
  m_log[iPos].state |= (radar.m_bInBed << 1) | (radar.m_bPresence << 2);
#endif
  m_log[iPos].rm = ths.m_temp;
  m_log[iPos].rh = ths.m_rh;

  iPos++;
  if(iPos == 288)
    iPos = 0;

  m_log[iPos].state = 8;  // use 8 as a break between old and new
}

String TempArray::get()
{
  String s = "[";
  bool bSent = false;

  for (int ent = 0; ent < LOG_CNT-1; ++ent)
  {
    if(ent) s += ",";
    if(m_log[ent].state == 8) // now
    {
      m_log[ent].min = (hour()*60) + minute();
      m_log[ent].temp = wb.m_currentTemp;
      m_log[ent].rm = ths.m_temp;
      m_log[ent].rh = ths.m_rh;
      m_log[ent].state = wb.m_bHeater;
    }
    s += "[";
    s += m_log[ent].temp; s += ",";
    s += m_log[ent].state; s += ",";
    s += m_log[ent].rm;
    s += ","; s += m_log[ent].rh;
    s += "]";
  }
  s += "]";
  jsonString js("tdata");
  js.VarNoQ("temp", s);
  return js.Close();
}

int16_t TempArray::t2y(uint16_t t, uint16_t h) // temp to y position
{
  return h - ((t-mn) * h / (mx-mn));
}

int16_t TempArray::tm2x(uint16_t t, uint16_t w) // time to x position
{
  return t * w / (60*24);
}

uint16_t TempArray::tween(uint16_t t1, uint16_t t2, int m, int r)
{
  uint16_t t = (t2 - t1) * (m * 100 / r ) / 100;
  return t + t1;
}

void TempArray::draw(int16_t xPos, int16_t yPos, uint16_t w, uint16_t h)
{
  mn = 1000; // get range
  mx = 0;
  for(uint8_t i = 0; i < ee.schedCnt[wb.m_season]; i++)
  {
    if(mn > ee.schedule[wb.m_season][i].setTemp)
      mn = ee.schedule[wb.m_season][i].setTemp;
    if(mx < ee.schedule[wb.m_season][i].setTemp)
      mx = ee.schedule[wb.m_season][i].setTemp;
  }

  for(uint16_t i = 0; i < LOG_CNT-2; i++)
  {
    if(m_log[i].temp)
    {
      if(mn > m_log[i].temp)
        mn = m_log[i].temp;
      if(mx < m_log[i].temp)
        mx = m_log[i].temp;
    }
    if(m_log[i].rm)
    {
      if(mn > m_log[i].rm)
        mn = m_log[i].rm;
      if(mx < m_log[i].rm)
        mx = m_log[i].rm;
    }
  }

  mn /= 10; mn *= 10; // floor
  mx += (10-(mx%10)); // ciel
  sprite.setTextColor(TFT_CYAN, TFT_BLACK);
  sprite.setFreeFont(&FreeSans7pt7b);
  sprite.drawString( String(mx / 10), xPos + w + 10, yPos + 10 ); // draw min/max no dec
  sprite.drawString( String(mn / 10), xPos + w + 10, yPos + h - 10 );

  uint16_t m = w - tm2x(ee.schedule[ee.schedCnt[wb.m_season]-1][0].timeSch, w); // wrap line
  uint16_t r = m + tm2x(ee.schedule[wb.m_season][0].timeSch, w);
  uint16_t ttl = tween(ee.schedule[wb.m_season][ee.schedCnt[wb.m_season]-1].setTemp, ee.schedule[wb.m_season][0].setTemp, m, r); // get y of midnight

  uint16_t x2, y2;
  uint16_t y = yPos + t2y(ttl, h);
  uint16_t x = xPos;

  for(uint8_t i = 0; i < ee.schedCnt[wb.m_season]; i++) // schedule points
  {
    x2 = tm2x(ee.schedule[wb.m_season][i].timeSch, w) + xPos;
    y2 = t2y(ee.schedule[wb.m_season][i].setTemp, h) + yPos;
    sprite.drawLine(x, y, x2, y2, TFT_ORANGE);
    x = x2;
    y = y2;
  }
  sprite.drawLine(x, y, xPos + w, yPos + t2y(ttl, h), TFT_ORANGE); // ending midnight point

  int iPosStart = (hour() * 12 + minute() / 5) - 1;
  if(iPosStart < 0) iPosStart = LOG_CNT-3;
  int iPos = iPosStart - 1;
  uint16_t i;

  // room temp
  for(i = 0; iPos != iPosStart; iPos--, i++)
  {
    if(iPos < 0)
    {
      iPos = LOG_CNT-3;
      continue;
    }

    if(m_log[iPos].temp == 0)
      break;

    x2 = tm2x(m_log[iPos].min, w) + xPos;
    y2 = t2y(m_log[iPos].rm, h) + yPos;
    if(i)
      sprite.drawLine(x, y, x2, y2, TFT_BLUE);
    x = x2;
    y = y2;
  }

  iPos = iPosStart - 1;
  // rh
  for(i = 0; iPos != iPosStart; iPos--, i++)
  {
    if(iPos < 0)
    {
      iPos = LOG_CNT-3;
      continue;
    }

    if(m_log[iPos].temp == 0)
      break;

    x2 = tm2x(m_log[iPos].min, w) + xPos;

    y2 = (h - (m_log[iPos].rh * h)) / 100 + yPos;
    if(i)
      sprite.drawLine(x, y, x2, y2, TFT_BLUE);
    x = x2;
    y = y2;
  }

  iPos = iPosStart - 1;
  // bed temp
  for(i = 0; iPos != iPosStart; iPos--, i++)
  {
    if(iPos < 0)
    {
      iPos = LOG_CNT-3;
      continue;
    }

    if(m_log[iPos].temp == 0)
      break;

    x2 = tm2x(m_log[iPos].min, w) + xPos;
    y2 = t2y(m_log[iPos].temp, h) + yPos;
    if(i)
      sprite.drawLine(x, y, x2, y2, (m_log[iPos].state&1) ? TFT_RED : TFT_LIGHTGREY);
    x = x2;
    y = y2;
  }

  x = tm2x( hour() * 60 + minute(), w ) + xPos;
  y = t2y(wb.m_currentTemp, h) + yPos;
  sprite.drawSpot(x, y, 3, TFT_GREEN); // current time/temp
}
