#include "THSensor.h"
#include "Display.h"
#include "eeMem.h"
#include <Wire.h>

extern void WsSend(String s);
extern void sendState(void);

THSensor::THSensor()
{ 
}

void THSensor::init()
{
  Wire.begin(11, 10, 100000); // 10,11 also used by RTC and IMU
}

void THSensor::service()
{
  static uint8_t read_delay = 2;
  static uint8_t errCnt = 0;

  if(--read_delay)
    return;

  float temp;
  float rh;
  if(am.measure(temp, rh))
  {
    if(!ee.bCF)
      temp = temp * 9 / 5 + 32;
    tempMedian.add(temp * 10);
    if (tempMedian.getAverage(2, temp) == tempMedian.OK)
    {
      m_temp = temp;
      m_rh = rh * 10;
    }
    errCnt = 0;
  }
  else
  {
    if(errCnt < 5)
      errCnt++;
    else
    {
      IPAddress ip;
      display.Notify("AM2320 Error", ip); // be annoying
    }
  }

  read_delay = 5; // update every 5 seconds
}
