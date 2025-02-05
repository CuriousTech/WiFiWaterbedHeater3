#ifndef THSENSOR_H
#define THSENSOR_H

#include <Arduino.h>
#include <AM2320.h> // https://github.com/CuriousTech/ESP-HVAC/blob/master/Libraries/AM2320/AM2320.h
#include "eeMem.h"
#include "RunningMedian.h"

class THSensor
{
public:
  THSensor(void);
  void init(void);
  void service(void);
  uint16_t m_temp;
  uint16_t m_rh;

private:
  RunningMedian<int16_t,25> tempMedian; //median of 25 samples
  AM2320 am;
};

extern THSensor ths;

#endif // THSENSOR_H
