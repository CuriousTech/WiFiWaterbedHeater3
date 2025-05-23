
// lights/dimmers/switches control for CuriousTech/Dimmer repo

#include "Lights.h"
#include "Display.h"
#include "uriString.h"
#include <WiFi.h>

extern Display display;

extern void WsSend(String s);

Lights::Lights()
{
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<Lights*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<Lights*>(obj))->_onDisconnect(c); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<Lights*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);
  m_ac.setRxTimeout(3); // give it 3 seconds
}

void Lights::init()
{
  m_bQuery = true;
  IPAddress ip(ee.lights[0].ip);
  callQueue(ip, 80, "/mdns");
}

void Lights::service()
{
  checkQueue();
}

void Lights::checkQueue()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  if (qIdxOut == qIdxIn || queue[qIdxOut].port == 0) // Nothing to do
    return;

  if(m_status == LI_Busy)
    return;

  if ( send(queue[qIdxOut].ip, queue[qIdxOut].port, queue[qIdxOut].sUri.c_str()) )
  {
    queue[qIdxOut].port = 0;
    if (++qIdxOut >= CQ_CNT)
      qIdxOut = 0;
  }
}

void Lights::clearQueue()
{
  memset(queue, 0, sizeof(queue));
  qIdxIn = 0;
  qIdxOut = 0;
}

void Lights::callQueue(IPAddress ip, uint16_t port, String sUri )
{
  if (queue[qIdxIn].port == 0)
  {
    queue[qIdxIn].ip = ip;
    queue[qIdxIn].sUri = sUri;
    queue[qIdxIn].port = port;
  }

  if (++qIdxIn >= CQ_CNT)
    qIdxIn = 0;
}

void Lights::setSwitch(char *pName, int8_t bPwr, uint16_t nLevel)
{
  uint8_t nLight = m_nSwitch; // last if no name

  if(pName)
    for(nLight = 0; ee.lights[nLight].szName[0] && nLight < EE_LIGHT_CNT; nLight++)
      if(!strcmp((const char *)pName, (const char *)ee.lights[nLight].szName))
        break;

  if(nLight >= EE_LIGHT_CNT-1 || ee.lights[nLight].ip[0] == 0) // no IP
    return;
  
  m_bQuery = false;

  m_nSwitch = nLight; // for response
  IPAddress ip(ee.lights[nLight].ip);

  uriString uri("/wifi");
  uri.Param("key", ee.iotPassword);
  if(bPwr == -1) // toggle
    uri.Param("pwr0", !m_bOn[nLight][0]);
  else
    uri.Param("pwr0", bPwr);

  if(nLevel)
  {
    uri.Param("level0", nLevel * 10); // dimmers are 1-1000
  }
  callQueue(ip, 80, uri.string());
}

bool Lights::getSwitch(const char *pName)
{
  uint8_t nLight = m_nSwitch; // last if no name

  if(pName)
    for(nLight = 0; ee.lights[nLight].szName[0] && nLight < EE_LIGHT_CNT; nLight++)
      if(!strcmp(pName, (const char *)ee.lights[nLight].szName))
        break;

  if(nLight >= EE_LIGHT_CNT-1 || ee.lights[nLight].ip[0] == 0) // no IP
    return false;

  return m_bOn[nLight][0];
}

bool Lights::send(IPAddress serverIP, uint16_t port, const char *pURI)
{
  if(m_ac.connected() || m_ac.connecting())
    return false;

  if(m_pBuffer == NULL)
  {
    m_pBuffer = new char[LBUF_SIZE];
    if(m_pBuffer) m_pBuffer[0] = 0;
    else
    {
      m_status = LI_MemoryError;
      return false;
    }
  }
  m_status = LI_Busy;
  strcpy(m_pBuffer, pURI);

  if(!m_ac.connect(serverIP, port))
  {
    m_status = LI_ConnectError;
    return false;
  }
  return true;
}

int Lights::checkStatus()
{
  if(m_status == LI_Done)
  {
    m_status = LI_Idle;
    return LI_Done;
  }
  return m_status;
}

void Lights::_onConnect(AsyncClient* client)
{
  if(m_pBuffer == NULL)
    return;

  String path = "GET ";
  path += m_pBuffer;
  path += " HTTP/1.1\n"
    "Host: ";
  path += client->remoteIP().toString();
  path += "\n"
    "Connection: close\n"
    "Accept: */*\n\n";

  m_ac.add(path.c_str(), path.length());
  m_bufIdx = 0;
}

// build file in chunks
void Lights::_onData(AsyncClient* client, char* data, size_t len)
{
  if(m_pBuffer == NULL || m_bufIdx + len >= LBUF_SIZE)
    return;
  memcpy(m_pBuffer + m_bufIdx, data, len);
  m_bufIdx += len;
  m_pBuffer[m_bufIdx] = 0;
}

void Lights::_onDisconnect(AsyncClient* client)
{
  (void)client;

  char *p = m_pBuffer;
  m_status = LI_Done;
  if(p == NULL)
    return;
  if(m_bufIdx == 0)
  {
    delete m_pBuffer;
    m_pBuffer = NULL;
    return;
  }

  const char *jsonList[] = { // switch control mode
    "ch",
    "on0",
    "on1",
    "level0",
    NULL
  };

  const char *jsonQ[] = { // query mode
    "*",     // 0
    NULL
  };

  if(p[0] != '{')
    while(p[4]) // skip all the header lines
    {
      if(p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n')
      {
        p += 4;
        break;
      }
      p++;
    }

  if(m_bQuery)
    processJson(p, jsonQ);
  else
    processJson(p, jsonList);
  delete m_pBuffer;
  m_pBuffer = NULL;
}

void Lights::callback(int8_t iName, char *pName, int32_t iValue, char *psValue)
{
  String s;

  switch(iName)
  {
    case -1: // wildcard
      {
        uint8_t nIdx;
        for(nIdx = 0; nIdx < EE_LIGHT_CNT; nIdx++)
        {
          if(ee.lights[nIdx].szName[0] == 0) // new slot
            break;
          else if(!strcmp(ee.lights[nIdx].szName, pName) ) // name already exists
            break;
        }
        if(nIdx == EE_LIGHT_CNT) // overflow
          break;
        strncpy(ee.lights[nIdx].szName, pName, sizeof(ee.lights[0].szName) );

        char *pIP = psValue + 1;
        char *pEnd = pIP;
        while(*pEnd && *pEnd != '\"') pEnd++;
        if(*pEnd == '\"'){*pEnd = 0; pEnd += 2;}
        IPAddress ip;
        ip.fromString(pIP);

        ee.lights[nIdx].ip[0] = ip[0];
        ee.lights[nIdx].ip[1] = ip[1];
        ee.lights[nIdx].ip[2] = ip[2];
        ee.lights[nIdx].ip[3] = ip[3];

        while(*pEnd && *pEnd != ',') pEnd++; // skip channel count
        m_bOn[nIdx][0] = atoi(pEnd); // channel 0 state
      }
      break;
    case 0: // ch
      break;
    case 1: // on0
      m_bOn[m_nSwitch][0] = iValue;
      break;
    case 2: // on1
      m_bOn[m_nSwitch][1] = iValue;
      break;
    case 3: // level0
      m_nLevel[m_nSwitch] = iValue;
      display.setSliderValue(BTF_LightsDimmer, m_nLevel[m_nSwitch] / 10 ); // dimmer is 1-1000, slider is 0-100
      break;
  }
}

void Lights::processJson(char *p, const char **jsonList)
{
  char *pPair[2]; // param:data pair
  int8_t brace = 0;
  int8_t bracket = 0;
  int8_t inBracket = 0;
  int8_t inBrace = 0;

  while(*p)
  {
    p = skipwhite(p);
    if(*p == '{'){p++; brace++;}
    if(*p == '['){p++; bracket++;}
    if(*p == ',') p++;
    p = skipwhite(p);

    bool bInQ = false;
    if(*p == '"'){p++; bInQ = true;}
    pPair[0] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else
    {
      while(*p && *p != ':') p++;
    }
    if(*p != ':')
      return;

    *p++ = 0;
    p = skipwhite(p);
    bInQ = false;
    if(*p == '{') inBrace = brace+1; // data: {
    else if(*p == '['){p++; inBracket = bracket+1;} // data: [
    else if(*p == '"'){p++; bInQ = true;}
    pPair[1] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else if(inBrace)
    {
      while(*p && inBrace != brace){
        p++;
        if(*p == '{') inBrace++;
        if(*p == '}') inBrace--;
      }
      if(*p=='}') p++;
    }else if(inBracket)
    {
      while(*p && inBracket != bracket){
        p++;
        if(*p == '[') inBracket++;
        if(*p == ']') inBracket--;
      }
      if(*p == ']') *p++ = 0;
    }else while(*p && *p != ',' && *p != '\r' && *p != '\n') p++;
    if(*p) *p++ = 0;
    p = skipwhite(p);
    if(*p == ',') *p++ = 0;

    inBracket = 0;
    inBrace = 0;
    p = skipwhite(p);

    if(pPair[0][0])
    {
      if(jsonList[0][0] == '*') // wildcard
      {
          int32_t n = atol(pPair[1]);
          if(!strcmp(pPair[1], "true")) n = 1; // bool case
          callback(-1, pPair[0], n, pPair[1]);
      }
      else for(int i = 0; jsonList[i]; i++)
      {
        if(!strcmp(pPair[0], jsonList[i]))
        {
          int32_t n = atol(pPair[1]);
          if(!strcmp(pPair[1], "true")) n = 1; // bool case
          callback(i, pPair[0], n, pPair[1]);
          break;
        }
      }
    }

  }
}

char *Lights::skipwhite(char *p)
{
  while(*p == ' ' || *p == '\t' || *p =='\r' || *p == '\n')
    p++;
  return p;
}
