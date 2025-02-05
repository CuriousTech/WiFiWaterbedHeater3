#ifndef RADAR_H
#define RADAR_H

#include <Arduino.h>
#include "eeMem.h"
#include <ld2410.h> // from Library Manager in Arduino IDE

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

private:
  ld2410 m_ld;
  bool radarConnected = false;

};

extern Radar radar;

#endif // RADAR_H
