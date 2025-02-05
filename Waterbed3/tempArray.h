#ifndef TEMPARRAY_H
#define TEMPARRAY_H

struct tempArr{
  uint16_t min;
  uint16_t temp;
  uint8_t state;
  uint16_t rm;
  uint16_t rh;
};

#define LOG_CNT (288+2)

class TempArray
{
public:
  TempArray(){}
  void add(void);
  String get(void);
  void draw(int16_t xPos, int16_t yPos, uint16_t w, uint16_t h);
protected:
  int16_t t2y(uint16_t t, uint16_t h);
  int16_t tm2x(uint16_t t, uint16_t w);
  uint16_t tween(uint16_t t1, uint16_t t2, int m, int r);
  tempArr m_log[LOG_CNT];
  uint16_t mn, mx;
};

extern TempArray ta;

#endif
