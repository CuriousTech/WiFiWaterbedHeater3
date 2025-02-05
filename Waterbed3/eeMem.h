#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

struct Sched
{
  uint16_t setTemp;
  uint16_t timeSch;
  uint8_t thresh;
  uint8_t wday;  // Todo: weekday 0=any, 1-7 = day of week
};

struct Alarm
{
  uint16_t ms;  // 0 = inactive
  uint16_t freq;
  uint16_t timeSch;
  uint8_t  wday;  // Todo: weekday 0=any, bits 0-6 = day of week
};

#define MAX_SCHED 8

#define EESIZE (offsetof(eeMem, end) - offsetof(eeMem, size) )

struct iotLight
{
  char szName[16];
  uint8_t ip[4];
};

class eeMem
{
public:
  eeMem(){};
  void init(void);
  bool check(void);
  bool update(bool bForce);
  bool verify(bool bComp);
  uint16_t Fletcher16( uint8_t* data, int count);

  uint16_t size = EESIZE;            // if size changes, use defaults
  uint16_t sum = 0xAAAA;             // if sum is different from memory struct, write
  uint16_t nWriteCnt;               // just a counter for later checking
  char     szSSID[24] = "";  // Enter you WiFi router SSID
  char     szSSIDPassword[24] = ""; // and password
  char     iotPassword[24] = "password"; // password for controlling dimmers and switches
  uint8_t  hostIp[4] = {192,168,31,100};
  uint8_t  hostPort = 85;
  uint8_t  hvacIP[4] = {192,168,31,46};
  uint16_t hvacPort = 80;
  uint8_t  brightLevel = 180; // brightness
  uint16_t sleepTime = 60; // timer for sleep mode

  uint16_t vacaTemp = 700;     // vacation temp
  uint8_t schedCnt[4] = {5,5,5,5};   // number of active scedules
  int8_t  tz = -5;
  bool    bVaca = false;         // vacation enabled
  bool    bAvg = true;          // average target between schedules
  bool    bEco = false;          // eco mode
  bool    bCF = false;  // true = celcius
  uint16_t scheduleDays[4] = {77, 155, 171, 355}; // Spring, Summer, Fall, Winter

  uint32_t nOvershootTime;
  int16_t  nOvershootTempDiff;
  Alarm   alarm[MAX_SCHED] = 
  { // alarms
    {0, 1000, 8*60, 0x3E},
    {0},
  };

#define EE_LIGHT_CNT 20
  iotLight lights[EE_LIGHT_CNT] =
  {
    {"BRFan", {192,168,31,57}}, // Device to query for the rest
    {},
  };

  Sched   schedule[4][MAX_SCHED] =  // 2x22x8 bytes
  {
    {
      {831,  3*60, 3, 0},
      {824,  6*60, 2, 0},  // temp, time, thresh, wday
      {819,  8*60, 3, 0},
      {819, 16*60, 3, 0},
      {834, 21*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0}
    },
    {
      {831,  3*60, 3, 0},
      {824,  6*60, 2, 0},  // temp, time, thresh, wday
      {819,  8*60, 3, 0},
      {819, 16*60, 3, 0},
      {834, 21*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0}
    },
    {
      {831,  3*60, 3, 0},
      {824,  6*60, 2, 0},  // temp, time, thresh, wday
      {819,  8*60, 3, 0},
      {819, 16*60, 3, 0},
      {834, 21*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0}
    },
    {
      {831,  3*60, 3, 0},
      {824,  6*60, 2, 0},  // temp, time, thresh, wday
      {819,  8*60, 3, 0},
      {819, 16*60, 3, 0},
      {834, 21*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0}
    },
  };
  uint16_t ppkwh = 154; // $0.154 / KWH
  uint16_t rate = 50; // seconds
  uint16_t watts = 290; // Heating pad
  uint32_t tSecsMon[12] = {1254411,1067144,916519,850686,122453,268488,302535,396531,501161,552347,427980,883172}; // total secwatt hours per month (copied from page)
  int16_t  tAdj[2] = {0,0};
  int16_t  pids[3] = {60*3,60*1, 5}; // Todo: real PID

  uint8_t  reserved[52];           // Note: To force an EEPROM update, just subtract 1 byte and flash again
  uint8_t  end;
};

extern eeMem ee;

#endif // EEMEM_H
