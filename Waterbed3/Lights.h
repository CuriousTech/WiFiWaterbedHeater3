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

struct cQ
{
  String sName;
  IPAddress ip;
  String sUri;
  uint16_t port;
};

#define LBUF_SIZE 1024

class Lights
{
public:
  Lights(void);
  void init(void);
  void service(void);
  void setSwitch(char *pName, int8_t bPwr, uint16_t nLevel);
  bool getSwitch(const char *pName);
  int checkStatus();
  void clearQueue(void);
  void callQueue(String sName, uint16_t port, String sUri);
  void callQueue(IPAddress ip, uint16_t port, String sUri);

private:
  bool send(String sName, uint16_t port, const char *pURI);
  bool send(IPAddress ip, uint16_t port, const char *pURI);
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  void _onTimeout(AsyncClient* client, uint32_t time);
  void _onData(AsyncClient* client, char* data, size_t len);
  void processJson(char *p, const char **jsonList);
  char *skipwhite(char *p);
  void callback(int8_t iName, char *pName, int32_t iValue, char *psValue);
  void checkQueue(void);
  int16_t outTemp();
  int16_t outRh();
  AsyncClient m_ac;
  char *m_pBuffer = NULL;
  char *m_pURI;
  uint8_t m_status = LI_Done;
  uint8_t m_nSwitch;
  bool m_bQuery;
  int m_bufIdx;
  bool m_bOn[EE_LIGHT_CNT][2];
  uint16_t m_nLevel[EE_LIGHT_CNT];
  uint8_t _error;
#define CQ_CNT 16
  cQ queue[CQ_CNT];
  uint8_t qIdxIn;
  uint8_t qIdxOut;

  int16_t m_outTemp;
  int16_t m_outRh;
};

extern Lights lights;

#endif // LIGHTS_H
