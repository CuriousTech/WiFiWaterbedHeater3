#ifndef LIGHTS_H
#define LIGHTS_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include "eeMem.h"

enum LI_Status
{
  LI_Idle,
  LI_Busy,
  LI_Done,
  LI_ConnectError,
  LI_Fail,
  LI_MemoryError,
};

#define LBUF_SIZE 1024

class Lights
{
public:
  Lights(void);
  void init(void);
  void start(void);
  bool setSwitch(char *pName, int8_t bPwr, uint8_t nLevel);
  bool getSwitch(uint8_t n);
  bool send(IPAddress serverIP, uint16_t port, const char *pURI);
  int checkStatus();

private:
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  void _onData(AsyncClient* client, char* data, size_t len);
  void processJson(char *p, const char **jsonList);
  char *skipwhite(char *p);
  void callback(int8_t iName, char *pName, int32_t iValue, char *psValue);

  AsyncClient m_ac;
  char *m_pBuffer = NULL;
  char *m_pURI;
  uint8_t m_status;
  uint8_t m_nSwitch;
  bool m_bQuery;
  int m_bufIdx;
  bool m_bOn[EE_LIGHT_CNT][2];
  uint8_t m_nLevel[EE_LIGHT_CNT];
};

extern Lights lights;

#endif // LIGHTS_H
