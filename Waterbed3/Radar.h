#ifndef RADAR_H
#define RADAR_H

#include <Arduino.h>
#include "eeMem.h"
#include <ld2410.h> // from Library Manager in Arduino IDE

#define LD2450
#define LD2450_BUFFER_SIZE (10*30)

struct RadarTarget
{
  int16_t x;           // X mm
  int16_t y;           // Y mm
  int16_t speed;       // cm/s
  uint16_t resolution; // mm
};

class Radar
{
public:
  Radar(void);
  void init(void);
  void service(void);
  bool read(void);

  bool m_bPresence;
  bool m_bLightOn;
  bool m_bInBed;
  uint8_t nZone = 2;
  uint8_t bits;
  int16_t nDistance = 400; // start out of zones
  uint16_t nEnergy;

private:
#ifdef LD2450
  RadarTarget radTgt[3];
  uint8_t circular_buffer[LD2450_BUFFER_SIZE];
  uint16_t buffer_head = 0;
  uint16_t buffer_tail = 0;
#else
  ld2410 m_ld;
#endif
  bool radarConnected = false;
};

extern Radar radar;

#endif // RADAR_H
