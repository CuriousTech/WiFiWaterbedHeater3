#ifndef WB_H
#define WB_H

#include <Arduino.h>
#include <OneWire.h>

#define HEAT 18
#define DS18B20 15

class WB
{
public:
  WB();
  void init(void);
  void service(void);
  void changeTemp(int delta, bool bAll);
  void checkLimits(void);
  void checkSched(bool bUpdate);
  void setHeat(void);
  void getSeason(void);

private:
  uint16_t tempAtTime(uint16_t timeTo);
  int tween(int t1, int t2, int m, int range);

  OneWire ds;
  byte ds_addr[8];

  uint32_t nHeatCnt;
  uint32_t nCoolCnt;
  uint32_t nOvershootCnt;
  uint32_t nOvershootPeak;
  uint16_t nOvershootStartTemp;
  uint16_t nOvershootEndTemp;
  int16_t  nOvershootTempDiff;
  uint32_t nOvershootTime;
  bool     bBoost;

public:
  uint32_t nHeatETA;
  uint32_t nCoolETA;
  int16_t  m_currentTemp;
  uint16_t m_hiTemp;
  uint16_t m_loTemp;
  uint8_t  m_season;
  bool     m_bHeater;
  uint8_t  m_schInd;

};

extern WB wb;

#endif // WB_H
