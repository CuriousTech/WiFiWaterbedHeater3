/*
Display and UI code
*/

#include <WiFi.h>
#include <time.h>
#include "eeMem.h"
#include "Lights.h"
#include <FunctionalInterrupt.h>
#include "display.h"
#include <math.h>
#include "WB.h"
#include "TempArray.h"
#include "THSensor.h"
#include "Media.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite popup = TFT_eSprite(&tft);

// URemote functions
extern int WsPcClientID;
extern void stopWiFi();
extern void startWiFi();
extern bool sendStatCmd( uint16_t *pCode);
extern void consolePrint(String s); // for browser debug
extern void remoteNotifCancelled(void);

extern tm gLTime;

static_assert(USER_SETUP_ID==303, "User setup incorrect in TFT_eSPI, use 303");

#define I2C_SDA  11
#define I2C_SCL  10
#define TP_SDA    1
#define TP_SCL    3
#define IMU_INT  13 // INT1
#define TP_RST    2
#define TP_INT    4 // normally high
#define TFT_BL    5
#define PWR_CTRL  7
#define PWR_BTN   6

#include "CST328.h"
CST328 touch(TP_SDA, TP_SCL, TP_RST, TP_INT);

const int16_t bgColor = TFT_BLACK;

#define TITLE_HEIGHT 22
#define BORDER_SPACE 3 // H and V spacing + font size for buttons and space between buttons

void Display::init(void)
{
  pinMode(TFT_BL, OUTPUT);

  pinMode(PWR_CTRL, OUTPUT);
  digitalWrite(PWR_CTRL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.setTextPadding(8);

  sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT); // This uses over 115K RAM for 240x240
  popup.setTextDatum(TL_DATUM);
  popup.setTextColor(TFT_WHITE, TFT_DARKGREY );
  popup.setFreeFont(&FreeSans9pt7b);

  // count tiles, index rows, count columns, format the buttons
  uint8_t nRowIdx = 0;

  for(m_nTileCnt = 0; m_nTileCnt < TILES && (m_tile[m_nTileCnt].Extras || m_tile[m_nTileCnt].pszTitle || m_tile[m_nTileCnt].pBGImage); m_nTileCnt++) // count screens
  {
    if(m_tile[m_nTileCnt].nRow != nRowIdx)
      m_nRowStart[++nRowIdx] = m_nTileCnt;
    m_nColCnt[nRowIdx]++;
    formatButtons(m_tile[m_nTileCnt]);
  }

  m_nCurrTile = 2; // start on WB tile
  m_nCurrRow = m_tile[m_nCurrTile].nRow;
  drawTile(m_nCurrTile, true, 0, 0);
  sprite.pushSprite(0, 0); // draw starting tile

  touch.begin();
}

void Display::service(void)
{
  dimmer();

  if(m_nDispFreeze)
  {
    if( millis() < m_nDispFreeze) // freeze display for notification
      return;
    m_nDispFreeze = 0;
  }

  static bool bScrolling; // scrolling active
  static bool bSliderHit;
  static int16_t touchXstart, touchYstart; // first touch pos for scolling and non-moving detect
  static uint32_t touchStartMS; // timing for detecting swipe vs press

  if(touch.available() )
  {
    if(touch.event == 0) // initial touch. Just setup for gesture check
    {
      touchXstart = touch.x;
      touchYstart = touch.y;
      touch.gestureID = 0;
      bSliderHit = false;
      bScrolling = false;
      touchStartMS = millis();

      m_backlightTimer = ee.sleepTime; // reset timer for any touch
      m_brightness = ee.brightLevel;
    }
    else if(touch.event == 1) // release
    {
      if(touch.gestureID)
      {
        snapTile(); // finish the swipe (only forward for now)
        m_bSwipeReady = false;
        touch.gestureID = 0; // unlock the idle display updates
      }
      else if(m_pCurrBtn)
      {
        drawButton(m_tile[m_nCurrTile], m_pCurrBtn, false, 0, 0, false); // draw released button
        updateTile();

        if(m_pCurrBtn && (m_pCurrBtn->flags & (BF_SLIDER_H|BF_SLIDER_V)) )
        {
          buttonCmd(m_pCurrBtn, true);
        }
      }
      else if(bScrolling == false) // check for a quick button hit if nothing else appeared to happen
      {
        checkButtonHit(touchXstart, touchYstart);
      }

      touch.gestureID = 0;
      m_pCurrBtn = NULL;
    }
    else // 2=active
    {
      if( m_brightness < ee.brightLevel ) // ignore input until brightness is full
        return;

      Tile& pTile = m_tile[m_nCurrTile];

      if(bSliderHit);
      else if(touch.gestureID) // swiping mode
      {
        swipeTile(touch.x - touchXstart, touch.y - touchYstart);
        touchXstart = touch.x;
        touchYstart = touch.y;
      }
      else if(bScrolling) // scrolling detected
      {
        if(touch.y - touchYstart)
          bScrolling = scrollPage(m_nCurrTile, touch.y - touchYstart);
        touchYstart = touch.y;
      }
      else // normal press or hold
      {
        if( !m_bPopupActive && pTile.Extras & EX_SCROLL ) // give this a bit more priority
        {
          if( touchYstart > 40 && touchYstart < DISPLAY_HEIGHT - 40 && touch.y != touchYstart && (touch.y - touchYstart < 10 || touchYstart - touch.y < 10)) // slow/short flick
          {
            if( touchXstart > 40 && touchXstart < DISPLAY_WIDTH - 40) // start needs to be in the middle somewhere
            {
              bScrolling = scrollPage(m_nCurrTile, touch.y - touchYstart); // test it
              touchYstart = touch.y;
            }
          }
        }

        if(millis() - touchStartMS < 150 && touch.gestureID == 0 && bScrolling == false) // delay a bit
        {
          if(m_bPopupActive && touch.x >= m_popupLeft && touch.x < m_popupLeft + m_popupWidth && touch.y >= m_popupTop && touch.y < m_popupTop + m_popupHeight)
          {
            m_bPopupActive = false;
            if(m_notifyIp)
              remoteNotifCancelled();
            updateTile();
          }
          else
          { // detect swipes
            swipeCheck((int16_t)touch.x - (int16_t)touchXstart, (int16_t)touch.y - (int16_t)touchYstart);

            if(touch.gestureID)
              startSwipe();
          }
        }
        else if(m_pCurrBtn == NULL && touch.gestureID == 0 && bScrolling == false) // check button hit
        {
          if(m_bPopupActive && touch.x >= m_popupLeft && touch.x < m_popupLeft + m_popupWidth && touch.y >= m_popupTop && touch.y < m_popupTop + m_popupHeight)
          {
            m_bPopupActive = false;
            if(m_notifyIp)
              remoteNotifCancelled();
            updateTile(); // erase popup
          }
          else
          {
            checkButtonHit(touchXstart, touchYstart);
          }
        }
        else if(m_pCurrBtn && (m_pCurrBtn->flags & (BF_SLIDER_H|BF_SLIDER_V)) )
        {
          if(m_pCurrBtn->flags & BF_SLIDER_H)
          {
            uint16_t off = constrain(touch.x - m_pCurrBtn->r.x, 0, m_pCurrBtn->r.w);

            m_pCurrBtn->data[1] = off * 100 / m_pCurrBtn->r.w;
            drawButton(pTile, m_pCurrBtn, true, 0, 0, false);
            updateTile();
          }
          else if(m_pCurrBtn->flags & BF_SLIDER_V)
          {
            uint16_t off = constrain(touch.y - m_pCurrBtn->r.y, 0, m_pCurrBtn->r.h);
            m_pCurrBtn->data[1] = 100 - (off * 100 / m_pCurrBtn->r.h);
            drawButton(pTile, m_pCurrBtn, true, 0, 0, false);
            updateTile();
          }
        }
        else if(millis() - m_lastms > 300) // repeat speed
        {
          if(m_pCurrBtn && (m_pCurrBtn->flags & (BF_REPEAT|BF_SLIDER_H|BF_SLIDER_V)) )
          {
            buttonCmd(m_pCurrBtn, true);
            if( !(m_pCurrBtn->flags & (BF_SLIDER_H|BF_SLIDER_V)) )
              media.Sound(SND_CLICK);
            if(m_pCurrBtn->flags & (BF_SLIDER_H|BF_SLIDER_V)) 
              bSliderHit = true;
          }
          m_lastms = millis();
        }
      }
    }
  }

  static uint16_t lastHiTemp;

  if(lastHiTemp != wb.m_hiTemp) // faster response
  {
    lastHiTemp = wb.m_hiTemp;
    drawTile(m_nCurrTile, false, 0, 0);
    updateTile();
  }

  // WiFi connect effect
  if( gLTime.tm_year < 124 || WiFi.status() != WL_CONNECTED) // Do a connecting effect
  {
    static uint8_t rgb[3] = {0, 0, 0};
    static uint8_t cnt;
    if(++cnt > 2)
    {
      cnt = 0;
      if(WiFi.status() != WL_CONNECTED)
        rgb[0] ++;
      else
        rgb[2] ++;
      tft.drawSmoothRoundRect(0, 0, 13, 14, DISPLAY_WIDTH-1, DISPLAY_HEIGHT - 2, rgb16(rgb[0], rgb[1], rgb[2]), bgColor);
    }
  }
}

void Display::updateTile()
{
  if(m_bPopupActive)
    popup.pushToSprite(&sprite, m_popupLeft, m_popupTop);
  sprite.pushSprite(0, 0);
}

void Display::swipeCheck(int16_t dX, int16_t dY)
{
  if(dX == 0 && dY == 0)
    return;

  if(dX == 0)
      touch.gestureID = (dY > 0) ? SWIPE_DOWN : SWIPE_UP;
  else if(dY == 0)
      touch.gestureID = (dX > 0) ? SWIPE_RIGHT : SWIPE_LEFT;
  else if(dX > 0 && dY > 0)
      touch.gestureID = (dX > dY) ? SWIPE_DOWN : SWIPE_RIGHT;
  else if(dX > 0 && dY < 0)
      touch.gestureID = (dX > dY) ? SWIPE_RIGHT : SWIPE_DOWN;
  else if(dX < 0 && dY < 0)
      touch.gestureID = (dX > dY) ? SWIPE_UP : SWIPE_LEFT;
  else  // -dX +dY
      touch.gestureID = (-dX > dY) ? SWIPE_LEFT : SWIPE_DOWN;
}

void Display::checkButtonHit(int16_t touchXstart, int16_t touchYstart)
{
  Tile& pTile = m_tile[m_nCurrTile];
  Button *pBtn = &pTile.button[0];

  for(uint16_t i = 0; pBtn[i].row != 0xFF; i++) // check for press in button bounds, rejects swiping
  {
    int16_t y = pBtn[i].r.y;

    if( !(pBtn[i].flags & BF_FIXED_POS) ) // adjust for scroll offset
      y -= pTile.nScrollIndex;

    if ( (touch.x >= pBtn[i].r.x-1 && touch.x <= pBtn[i].r.x + pBtn[i].r.w+1 && touch.y >= y-1 && touch.y <= y + pBtn[i].r.h+1)
       && (touchXstart >= pBtn[i].r.x-1 && touchXstart <= pBtn[i].r.x + pBtn[i].r.w+1 && touchYstart >= y-1 && touchYstart <= y + pBtn[i].r.h+1) ) // make sure not swiping
    {
      m_pCurrBtn = &pBtn[i];

      if(m_pCurrBtn->flags & BF_SLIDER_H)
      {
        uint16_t off = touch.x - m_pCurrBtn->r.x;
        m_pCurrBtn->data[1] = off * 100 / m_pCurrBtn->r.w;
      }
      else if(m_pCurrBtn->flags & BF_SLIDER_V)
      {
        uint16_t off = touch.y - m_pCurrBtn->r.y;
        m_pCurrBtn->data[1] = 100 - (off * 100 / m_pCurrBtn->r.h);
      }
      drawButton(pTile, m_pCurrBtn, true, 0, 0, false); // draw pressed state
      updateTile();
      buttonCmd(m_pCurrBtn, false);
      if( !(m_pCurrBtn->flags & (BF_SLIDER_H|BF_SLIDER_V)) )
        media.Sound(SND_CLICK);
      m_lastms = millis() - 600; // slow first repeat (600+300ms)
      break;
    }
  }
}

void Display::startSwipe()
{
  uint8_t nRowOffset;

  m_bSwipeReady = false;
  m_swipePos = 0;

  switch(touch.gestureID)
  {
    case SWIPE_RIGHT:
      if( (m_nCurrTile <= 0) // don't screoll if button pressed or screen 0
        || (m_tile[m_nCurrTile].nRow != m_tile[m_nCurrTile-1].nRow) ) // don't scroll left if leftmost
      {
        swipeBump();
        break;
      }

      m_nLastRow = m_nCurrRow;
      m_nNextTile = m_nCurrTile - 1;
      m_bSwipeReady = true;
      break;
    case SWIPE_LEFT:
      if( (m_nCurrTile >= m_nTileCnt - 1) // don't scroll if last screen
        || (m_tile[m_nCurrTile].nRow != m_tile[m_nCurrTile+1].nRow) ) // don't scroll right if rightmost
      {
        swipeBump();
        break;
      }
  
      m_nLastRow = m_tile[m_nCurrTile].nRow;
      m_nNextTile = m_nCurrTile + 1; // save for snap
      m_bSwipeReady = true;
      break;
    case SWIPE_DOWN:
      if(m_tile[m_nCurrTile].nRow == 0) // don't slide up from top
      {
        swipeBump();
        break;
      }
  
      nRowOffset = m_nCurrTile - m_nRowStart[m_nCurrRow];
      m_nLastRow = m_nCurrRow;
      m_nCurrRow--;
      m_nNextTile = m_nRowStart[m_nCurrRow] + min((int)nRowOffset, m_nColCnt[m_nCurrRow]-1 ); // get close to the adjacent screen

      if(m_nCurrRow == 1) // slide up/swipe down to WB tile
        m_nNextTile = 2;

      m_bSwipeReady = true;
      break;
    case SWIPE_UP:
      if(m_nCurrRow == m_tile[m_nTileCnt-1].nRow)
      {
        swipeBump();
        break;
      }

      nRowOffset = m_nCurrTile - m_nRowStart[m_nCurrRow];
      m_nLastRow = m_nCurrRow;
      m_nNextTile = m_nRowStart[m_nCurrRow+1] + min((int)nRowOffset, m_nColCnt[m_nCurrRow+1]-1 );

      if(m_nNextTile < m_nTileCnt)
      {
        m_bSwipeReady = true;
        m_nCurrRow++;
      }

      if(m_nCurrRow == 1) // slide down/swipe up to WB tile
        m_nNextTile = 2;

      break;
  }
}

// swipe effect
void Display::swipeTile(int16_t dX, int16_t dY)
{
  if(m_bSwipeReady == false)
    return;

  switch(touch.gestureID)
  {
    case SWIPE_RIGHT: // >>>
      m_swipePos = constrain(m_swipePos + dX, 0, DISPLAY_WIDTH);
      break;
    case SWIPE_LEFT: // <<<
      m_swipePos = constrain(m_swipePos - dX, 0, DISPLAY_WIDTH);
      break;
    case SWIPE_DOWN:
      m_swipePos = constrain(m_swipePos + dY, 0, DISPLAY_HEIGHT);
      break;
    case SWIPE_UP:
      m_swipePos = constrain(m_swipePos - dY, 0, DISPLAY_HEIGHT);
      break;
  }

  drawSwipeTiles();
}

// do some kind of visual effect
void Display::swipeBump()
{
  switch(touch.gestureID)
  {
    case SWIPE_RIGHT: // >>>
      tft.fillRect(0, 0, 5, DISPLAY_HEIGHT, TFT_BLUE);
      sprite.pushSprite(5, 0);
      break;
    case SWIPE_LEFT: // <<<
      tft.fillRect(DISPLAY_WIDTH-5, 0, 5, DISPLAY_HEIGHT, TFT_BLUE);
      sprite.pushSprite(-5, 0);
      break;
    case SWIPE_DOWN:
      tft.fillRect(0, 0, DISPLAY_WIDTH, 5, TFT_BLUE);
      sprite.pushSprite(0, 5);
      break;
    case SWIPE_UP:
      tft.fillRect(0, DISPLAY_HEIGHT-5, DISPLAY_WIDTH, 5, TFT_BLUE);
      sprite.pushSprite(0, -5);
      break;
  }
  delay(50);
  updateTile();
}

// finish unfinished swipe
void Display::snapTile()
{
  if(m_bSwipeReady == false)
    return;

  uint8_t nSpeed = 50;
  uint16_t space = DISPLAY_HEIGHT;

  if( touch.gestureID == SWIPE_LEFT || touch.gestureID == SWIPE_RIGHT)
    space = DISPLAY_WIDTH;

  if(m_swipePos < space/2)
  {
    m_nCurrRow = m_nLastRow;

    while(m_swipePos > 0)
    {
      if(m_swipePos > nSpeed)
        m_swipePos -= nSpeed;
      else
        m_swipePos = 0;
      drawSwipeTiles();
      nSpeed -= 5;
    }
    return;
  }

  while( m_swipePos < space)
  {
    if(m_swipePos < space - nSpeed)
      m_swipePos += nSpeed;
    else
      m_swipePos = space;
    drawSwipeTiles();
    nSpeed -= 5;
  }

  m_nCurrTile = m_nNextTile;
}

void Display::drawSwipeTiles()
{
  switch(touch.gestureID)
  {
    case SWIPE_RIGHT: // >>>
      drawTile(m_nCurrTile, true, m_swipePos, 0);
      drawTile(m_nNextTile, true, m_swipePos - DISPLAY_WIDTH, 0);
      break;
    case SWIPE_LEFT: // <<<
      drawTile(m_nCurrTile, true, -m_swipePos, 0);
      drawTile(m_nNextTile, true, DISPLAY_WIDTH - m_swipePos, 0);
      break;
    case SWIPE_DOWN:
      drawTile(m_nCurrTile, true, 0, m_swipePos);
      drawTile(m_nNextTile, true, 0, m_swipePos - DISPLAY_HEIGHT);
      break;
    case SWIPE_UP:
      drawTile(m_nCurrTile, true, 0, -m_swipePos);
      drawTile(m_nNextTile, true, 0, DISPLAY_HEIGHT - m_swipePos);
      break;
  }
  updateTile();
}

// called each second
void Display::oneSec()
{
  if(m_nDispFreeze || touch.gestureID) // don't overwrite the popup or while swiping
    return;

  if( m_backlightTimer ) // the dimmer thing
  {
    if(--m_backlightTimer == 0)
    {
      m_brightness = 0;
      m_bSwitchToHome = true;
    }
  }

  if(m_brightness) // anything but off
  {
    drawTile(m_nCurrTile, false, 0, 0);
    updateTile();
  }

  static bool bQDone = false;
  // A query or command is done
  if(lights.checkStatus() == LI_Done)
  {
    if(bQDone == false) // first response is list from a lightswitch
    {
      bQDone = true;
      // Find Lights tile
      uint8_t nLi = 0;
      while(m_tile[nLi].pszTitle || m_tile[nLi].Extras )
      {
        if( !strcmp(m_tile[nLi].pszTitle, "Lights") )
          break;
        nLi++;
      }

      Tile& pTile = m_tile[nLi];

      if(pTile.Extras)
      {
        uint8_t i;

        for(i = 0; i < BUTTON_CNT - 2 && ee.lights[i].szName[0]; i++)
        {
          pTile.button[i+1].pszText = ee.lights[i].szName;
          pTile.button[i+1].row = i >> 1; // 2 per row
          pTile.button[i+1].r.w = 100;
          pTile.button[i+1].r.h = 28;
          pTile.button[i+1].nFunction = BTF_Lights;
        }

        pTile.button[i].row = 0xFF; // end row
      }
      formatButtons(pTile);
    }
  }
}

// scrolling oversized tiles and notification list
bool Display::scrollPage(uint8_t nTile, int16_t nDelta)
{
  Tile& pTile = m_tile[nTile];
  uint16_t nViewSize = DISPLAY_HEIGHT;

  if(pTile.pszTitle[0])
    nViewSize -= TITLE_HEIGHT;

  if(pTile.Extras & EX_NOTIF )
  {
    uint8_t nCnt;
    for(nCnt = 0; nCnt < NOTE_CNT && m_pszNotifs[nCnt]; nCnt++);

    if(nCnt) nCnt--; // fudge
    pTile.nPageHeight = nCnt * sprite.fontHeight();
    nViewSize -= 28; // clear button
  }

  if(pTile.nPageHeight <= nViewSize) // not large enough to scroll
    return false;

  pTile.nScrollIndex = constrain(pTile.nScrollIndex - nDelta, -TITLE_HEIGHT, pTile.nPageHeight - nViewSize);

  drawTile(nTile, false, 0, 0);
  updateTile();
  return true; // scrollable
}

// calculate positions of non-fixed buttons
void Display::formatButtons(Tile& pTile)
{
  sprite.setTextDatum(MC_DATUM); // center text on button
  sprite.setFreeFont(&FreeSans9pt7b);

  uint8_t btnCnt;
  for( btnCnt = 0; btnCnt < BUTTON_CNT && pTile.button[btnCnt].row != 0xFF; btnCnt++) // count buttons
    if( pTile.button[btnCnt].r.w == 0 && pTile.button[btnCnt].pszText && pTile.button[btnCnt].pszText[0] ) // if no width
    {
      if(pTile.button[btnCnt].pFont)
        sprite.setFreeFont(pTile.button[btnCnt].pFont);
      else
        sprite.setFreeFont(&FreeSans9pt7b);
      pTile.button[btnCnt].r.w = sprite.textWidth(pTile.button[btnCnt].pszText) + (BORDER_SPACE*2);   // calc width based on text width
    }

  if(btnCnt == 0)
    return;

  uint8_t btnIdx = 0;
  uint8_t nRows = pTile.button[btnCnt - 1].row + 1; // use last button for row total
  uint8_t nRow = 0;
  uint16_t nCenter;

  // Calc X positions
  while( nRow < nRows )
  {
    uint8_t rBtns = 0;
    uint16_t rWidth = 0;

    for(uint8_t i = btnIdx; i < btnCnt && pTile.button[i].row == nRow; i++, rBtns++)  // count buttons for current row
    {
      if( pTile.button[i].r.x == 0)// && (!pTile.button[ i ].flags & BF_FIXED_POS))
      {
        rWidth += pTile.button[i].r.w + BORDER_SPACE; // total up width of row
      // todo: find row height here, max up
      }
    }

    nCenter = DISPLAY_WIDTH/2;

    uint16_t cx = nCenter - (rWidth >> 1);

    for(uint8_t i = btnIdx; i < btnIdx+rBtns; i++)
    {
      if(pTile.button[i].r.x == 0)
      {
        pTile.button[i].r.x = cx;
        cx += pTile.button[i].r.w + BORDER_SPACE;
      }
    }

    btnIdx += rBtns;
    nRow++;
  }

  // Y positions
  uint8_t nBtnH = pTile.button[0].r.h;
  if(pTile.button[0].flags & BF_FIXED_POS) // fix: kludge for light slider
    nBtnH = pTile.button[1].r.h;

  if(nRows) nRows--; // todo: something is amiss here
  pTile.nPageHeight = nRows * (nBtnH + BORDER_SPACE) - BORDER_SPACE;

  uint16_t nViewSize = DISPLAY_HEIGHT;
  if(pTile.pszTitle[0])                           // shrink view by title height
    nViewSize -= TITLE_HEIGHT;

  pTile.nScrollIndex = 0;

  // ScrollIndex is added to button Y on draw and detect
  if(!(pTile.Extras & EX_NOTIF) && pTile.nPageHeight <= nViewSize) // Center smaller page
    pTile.nScrollIndex = -(nViewSize - pTile.nPageHeight) >> 1;
  else if(pTile.pszTitle[0])                       // Just offset by title height if too large
    pTile.nScrollIndex -= (TITLE_HEIGHT + BORDER_SPACE);             // down past title

  for(btnIdx = 0; btnIdx < btnCnt; btnIdx++ )
    if( !(pTile.button[ btnIdx ].flags & BF_FIXED_POS) )
    {
      pTile.button[ btnIdx ].r.y = pTile.button[ btnIdx ].row * (nBtnH + BORDER_SPACE);
    }
}

void Display::drawTile(int8_t nTile, bool bFirst, int16_t x, int16_t y)
{
  Tile& pTile = m_tile[nTile];

  if(pTile.pBGImage)
    sprite.pushImage(x, y, DISPLAY_WIDTH, DISPLAY_HEIGHT, pTile.pBGImage);
  else if(x == 0 && y == 0)
    sprite.fillSprite(bgColor);
  else
    sprite.fillRect(x, y, DISPLAY_WIDTH, DISPLAY_HEIGHT, bgColor);

  if(pTile.Extras & EX_NOTIF)
    drawNotifs(pTile, x, y);

  sprite.setTextDatum(MC_DATUM);
  sprite.setFreeFont(&FreeSans9pt7b);

  if(bFirst && !strcmp(pTile.pszTitle, "Music")) // fill in the mp3 list
  {
    media.fillFileBButtons(pTile);
    formatButtons(pTile);
  }

  uint8_t btnCnt;
  for( btnCnt = 0; btnCnt < BUTTON_CNT && pTile.button[btnCnt].row != 0xFF; btnCnt++); // count buttons

  if( btnCnt )
  {
    for(uint8_t btnIdx = 0; btnIdx < btnCnt; btnIdx++ )
      drawButton( pTile, &pTile.button[ btnIdx ], false, x, y, bFirst);
  }

  if(pTile.pszTitle[0])
  {
    sprite.setFreeFont(&FreeSans9pt7b);
    sprite.fillRect(x, y, DISPLAY_WIDTH, TITLE_HEIGHT, bgColor); // cover any scrolled buttons/text
    sprite.setTextColor(rgb16(16,63,0), bgColor );
    sprite.drawString(pTile.pszTitle, x + DISPLAY_WIDTH/2, y + TITLE_HEIGHT/2 - 3);
  }
}

uint8_t Display::hourFormat12(uint8_t h)
{
  if(h > 12) return h - 12;
  if(h == 0) h = 12;
  return h;
}

const char _monthShortStr[13][4] PROGMEM = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
const char _dayShortStr[8][4] PROGMEM = {"","Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

void Display::drawButton(Tile& pTile, Button *pBtn, bool bPressed, int16_t x, int16_t y, bool bInit)
{
  String s = pBtn->pszText;

  switch(pBtn->nFunction)
  {
    case BTF_RSSI:
      btnRSSI(pBtn, x, y);
      return;
    case BTF_Time:
      if(gLTime.tm_year < 124) // don't display invalid time
        return;
      s = String( hourFormat12(gLTime.tm_hour) );
      if(hourFormat12(gLTime.tm_hour) < 10)
        s = " " + s;
      s += ":";
      if(gLTime.tm_min < 10) s += "0";
      s += gLTime.tm_min;
      break;
    case BTF_AMPM:
      if(gLTime.tm_year < 124) // don't display invalid time
        return;
      s = "";
      if(gLTime.tm_sec < 10) s += "0";
      s += gLTime.tm_sec;
      s += " ";
      s += (gLTime.tm_hour >= 12) ? "PM":"AM";
      s += " ";
      s += _dayShortStr[gLTime.tm_wday];
      break;
    case BTF_Date:
      if(gLTime.tm_year < 124) // don't display invalid time
        return;
      s = _monthShortStr[gLTime.tm_mon];
      s += " ";
      s += String(gLTime.tm_mday);
      break;

    case BTF_SetTemp:
      s = String((float)wb.m_hiTemp/10, 1);
      s += "°";
      break;

    case BTF_WBTemp:
      s = String((float)wb.m_currentTemp/10, 1);
      s += "°";
      pBtn->textColor = wb.m_bHeater ? TFT_RED : TFT_CYAN;
      break;

    case BTF_RoomTemp:
      s = String((float)ths.m_temp/10, 1);
      break;
    
    case BTF_Stat_Fan:
      if(m_bStatFan)
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;
      break;
    case BTF_Stat_Temp:
      s = String((float)m_statTemp/10, 1);
      break;
    case BTF_Stat_SetTemp:
      s  = String((float)m_statSetTemp/10, 1);
      break;
    case BTF_OutTemp:
      s = String((float)m_outTemp/10, 1);
      break;
    case BTF_Lights:
      if(lights.getSwitch(pBtn->pszText)) // light is on
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;
      break;
    case BTF_Sleep:
      s = ee.sleepTime;
      break;
    case BTF_TrackBtn:
      s = pBtn->pszText;
      s.remove(29); // cut down if needed
      break;
#ifdef RADAR_H
    case BTF_RadarDist:
      s = "Radar ";
      s += radar.nZone;
      s += " ";
      s += radar.nDistance;
      s += " ";
      s += radar.nEnergy;
      break;
#endif
    case BTF_Brightness:
      if(bInit) // only set when drawing tile, not user interaction
        pBtn->data[1] = m_brightness * 100 / 255;
      s = "Brightness ";
      s += pBtn->data[1];
      break;
    case BTF_Volume:
      if(bInit)
        pBtn->data[1] = media.m_volume;
      s = "Volume ";
      s += pBtn->data[1];
      break;

  }

  int16_t yOffset = pBtn->r.y;

  if( !(pBtn->flags & BF_FIXED_POS) ) // don't scroll fixed pos buttons
    yOffset -= pTile.nScrollIndex;

  if(yOffset < (pTile.pszTitle[0] ? 10:0) || yOffset >= DISPLAY_HEIGHT) // cut off anything out of bounds (top is covered by title band)
    return;

  yOffset += y; // swipe offset

  const int radius = 5;
  uint8_t nState = (pBtn->flags & BF_STATE_2) ? 1:0;
  uint16_t colorBg = (nState) ? TFT_NAVY : TFT_DARKGREY;

  if(bPressed)
  {
    nState = 1;
    colorBg = TFT_DARKCYAN;
  }

  if(pBtn->flags & (BF_ARROW_UP|BF_ARROW_DOWN|BF_ARROW_LEFT|BF_ARROW_RIGHT))
  {
    sprite.fillRoundRect(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, radius, colorBg);
    sprite.drawSmoothRoundRect(x + pBtn->r.x, yOffset, 2, 2, pBtn->r.w, pBtn->r.h, radius, TFT_CYAN, bgColor);

    const uint8_t pad = 5;
    int16_t x2 = x + pBtn->r.x + (pBtn->r.w>>1);

    if(pBtn->flags & (BF_ARROW_UP|BF_ARROW_DOWN))
      sprite.drawWideLine(x2, yOffset + pad, x2, yOffset + pBtn->r.h - pad, 4, TFT_CYAN, colorBg);
    else
      sprite.drawWideLine(x + pBtn->r.x + pad, yOffset + (pBtn->r.h>>1), x + pBtn->r.x + pBtn->r.w - pad, yOffset + (pBtn->r.h>>1), 4, TFT_CYAN, colorBg);

    if(pBtn->flags & (BF_ARROW_UP|BF_ARROW_LEFT))
      sprite.drawWideLine(x2, yOffset + pad, x + pBtn->r.x + pad, yOffset + (pBtn->r.h>>1), 4, TFT_CYAN, colorBg);

    if(pBtn->flags & (BF_ARROW_UP|BF_ARROW_RIGHT))
      sprite.drawWideLine(x2, yOffset + pad, x + pBtn->r.x + pBtn->r.w - pad, yOffset + (pBtn->r.h>>1), 4, TFT_CYAN, colorBg);

    if(pBtn->flags & (BF_ARROW_DOWN|BF_ARROW_LEFT))
      sprite.drawWideLine(x + pBtn->r.x + pad, yOffset + (pBtn->r.h>>1), x2, yOffset + pBtn->r.h - pad, 4, TFT_CYAN, colorBg);

    if(pBtn->flags & (BF_ARROW_DOWN|BF_ARROW_RIGHT))
      sprite.drawWideLine(x + pBtn->r.x + pBtn->r.w - pad, yOffset + (pBtn->r.h>>1), x2, yOffset + pBtn->r.h - pad, 4, TFT_CYAN, colorBg);
  }
  else if(pBtn->nFunction == BTF_History)
  {
    ta.draw(x + pBtn->r.x + 1, yOffset + 1, pBtn->r.w - 2, pBtn->r.h - 2);
    sprite.drawRoundRect(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, radius, TFT_CYAN);
  }
  else if(pBtn->pIcon[nState]) // draw an image if given
    sprite.pushImage(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, pBtn->pIcon[nState]);
  else if(pBtn->flags & BF_BORDER) // bordered text item
    sprite.drawRoundRect(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, radius, colorBg);
  else if(!(pBtn->flags & BF_TEXT) ) // if no image, or no image for state 2, and not just text, fill with a color
    sprite.fillRoundRect(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, radius, colorBg);

  const uint8_t dotSz = 10;
  if(pBtn->flags & BF_SLIDER_H)
    sprite.drawWideLine(x + pBtn->r.x, yOffset + (pBtn->r.h>>1), x + pBtn->r.x + pBtn->r.w, yOffset + (pBtn->r.h>>1), dotSz<<1, bgColor, bgColor);
  else if(pBtn->flags & BF_SLIDER_V)
    sprite.drawWideLine(x + pBtn->r.x + (pBtn->r.w>>1), yOffset, x + pBtn->r.x + (pBtn->r.w>>1), yOffset + pBtn->r.h, dotSz<<1, bgColor, bgColor); // erase old spot

  if(s.length())
  {
    if(pBtn->pFont)
      sprite.setFreeFont(pBtn->pFont);
    else
      sprite.setFreeFont(&FreeSans9pt7b); // default font

    if(pBtn->flags & BF_TEXT) // text only
      sprite.setTextColor(pBtn->textColor ? pBtn->textColor : TFT_WHITE,  bgColor);
    else
      sprite.setTextColor(TFT_CYAN, colorBg);

    if(pBtn->flags & BF_SLIDER_V) // vertical slider text
      drawText(s, x + pBtn->r.x + 26, yOffset + (pBtn->r.h >> 1), 270, pBtn->textColor ? pBtn->textColor : TFT_WHITE,  bgColor, pBtn->pFont);
    else // normal text
      sprite.drawString( s, x + pBtn->r.x + (pBtn->r.w >> 1), yOffset + (pBtn->r.h >> 1) - 2 );
  }

  if(pBtn->flags & BF_SLIDER_H)
  {
    sprite.drawWideLine(x + pBtn->r.x, yOffset + (pBtn->r.h>>1), x + pBtn->r.x + pBtn->r.w, yOffset + (pBtn->r.h>>1), 5, TFT_BLUE, bgColor);
    sprite.drawSpot(x + pBtn->r.x + dotSz + pBtn->data[1] * (pBtn->r.w - dotSz*2) / 100, yOffset + 15, dotSz, TFT_YELLOW);
  }
  else if(pBtn->flags & BF_SLIDER_V)
  {
    sprite.drawWideLine(x + pBtn->r.x + (pBtn->r.w>>1), yOffset, x + pBtn->r.x + (pBtn->r.w>>1), yOffset + pBtn->r.h, 5, TFT_BLUE, bgColor);
    sprite.drawSpot(x + pBtn->r.x + (pBtn->r.w>>1), yOffset + dotSz + (100 - pBtn->data[1]) * (pBtn->r.h - dotSz*2) / 100, dotSz, TFT_YELLOW);
  }
}

// called when button is activated
void Display::buttonCmd(Button *pBtn, bool bRepeat)
{
  uint16_t code[4];
  IPAddress ip;

  switch(pBtn->nFunction)
  {
    case BTF_SleepDec:
      if(ee.sleepTime)
        ee.sleepTime --;
      break;
    case BTF_SleepInc:
      if(ee.sleepTime < 3600)
        ee.sleepTime ++;
      break;
    case BTF_WbTempInc:
      if(ee.bVaca) // disable vacation mode
        ee.bVaca = false;
      else
        wb.changeTemp(1, true);
      break;
    case BTF_WbTempDec:
      if(ee.bVaca) // disable vacation mode
        ee.bVaca = false;
      else
        wb.changeTemp(-1, true);
      break;
    case BTF_LightsDimmer: // light dimmer
      lights.setSwitch( NULL, true, pBtn->data[1]);
      break;
    case BTF_Brightness: // screen brightness
      m_brightness = ee.brightLevel = pBtn->data[1] * 255 / 100; // 100% = 255
      break;

    case BTF_Volume: // music volume
      media.setVolume( pBtn->data[1] );
      break;

    case BTF_Lights:
      lights.setSwitch((char *)pBtn->pszText, -1, 0 );
      break;

    case BTF_Restart:
      ESP.restart();
      break;

    case BTF_StatCmd:
    case BTF_Stat_Fan:
      if( !sendStatCmd(pBtn->data) )
        toast("Stat command failed");
      break;

    case BTF_Clear:
      memset(m_pszNotifs, 0, sizeof(m_pszNotifs) );
      break;

    case BTF_TrackBtn:
      media.Play("/", pBtn->pszText);
      break;
  }
}

// Pop uup a notification + add to notes list
void Display::toast(const char *pszNote)
{
  m_brightness = ee.brightLevel;

  tft.setFreeFont(&FreeSans9pt7b);

  uint16_t w = tft.textWidth(pszNote) + (BORDER_SPACE*2);
  if(w > DISPLAY_WIDTH - 10) w = DISPLAY_WIDTH - 10;
  int16_t x = (DISPLAY_WIDTH/2) - (w >> 1); // center on screen
  const int16_t y = 30; // kind of high up

  tft.fillRoundRect(x, y, w, tft.fontHeight() + 3, 5, TFT_ORANGE);
  tft.setTextColor(TFT_BLACK, TFT_ORANGE);
  tft.drawString( pszNote, x + BORDER_SPACE, y + BORDER_SPACE );

  for(int8_t i = NOTE_CNT - 2; i >= 0; i--) // move old notifications down
    m_pszNotifs[i+1] = m_pszNotifs[i];
  m_pszNotifs[0] = pszNote;
  m_nDispFreeze = millis() + 900; // pop the toast up for 900ms
  media.Sound(SND_ALERT);
}

void Display::Notify(char *pszNote, IPAddress ip)
{
  m_notifyIp = ip; // used for remote cancel
  m_brightness = ee.brightLevel;

  for(int8_t i = NOTE_CNT - 2; i >= 0; i--) // move old notifications down
    m_pszNotifs[i+1] = m_pszNotifs[i];
  m_pszNotifs[0] = pszNote;

  if(popup.created())
  {
    popup.deleteSprite();
  }
  uint8_t nLines = 1;
  for(uint8_t i = 0; pszNote[i]; i++) // count the lines, assume last has no LF
    if(pszNote[i]=='\n') nLines++;

  m_popupHeight = 10 + nLines * 14;
  m_popupWidth = 180;

  m_popupLeft = (DISPLAY_WIDTH - m_popupWidth) / 2;
  m_popupTop = (DISPLAY_HEIGHT - m_popupHeight) / 2;

  popup.createSprite(m_popupWidth, m_popupHeight);

  popup.fillSprite(bgColor);
  popup.fillRect(1, 1, m_popupWidth-3, m_popupHeight-3, TFT_DARKGREY);

  String s = pszNote;
  uint8_t y = 5;
  int16_t nxt = 0;
  int16_t idx;

  while((idx = s.indexOf('\n', nxt)) != -1)
  {
    popup.drawString(s.substring(nxt, idx), 4, y);
    nxt = idx + 1;
    y += 14;
  }
  popup.drawString(s.substring(nxt), 4, y);

  popup.drawSmoothRoundRect(0, 0, 3, 3, m_popupWidth-1, m_popupHeight-1, TFT_CYAN, bgColor);

  popup.pushSprite( m_popupLeft, m_popupTop);
  m_bPopupActive = true;

  media.Sound(SND_ALERT);
}

void Display::NotificationCancel(IPAddress ip) // cancel a remote popup
{
  if(ip == m_notifyIp && m_bPopupActive)
  {
    m_bPopupActive = false;
    updateTile();
  }
}

void Display::checkNotif()
{
  if(m_bPopupActive) // make a sound when popup is active (mostly radar, when someone enters the room)
    media.Sound(SND_ALERT);
}

// draw the notifs list
void Display::drawNotifs(Tile& pTile, int16_t x, int16_t y)
{
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_WHITE, bgColor );
  sprite.setFreeFont(&FreeSans9pt7b);

  x += 40; // left padding
  y -= pTile.nScrollIndex; // offset by the scroll pos

  for(uint8_t i = 0; i < NOTE_CNT && m_pszNotifs[i]; i++)
  {
    if(y > TITLE_HEIGHT-10) // somewhat covered by title
      sprite.drawString(m_pszNotifs[i], x, y);
    y += sprite.fontHeight();
    if(y > DISPLAY_HEIGHT + 4)
      break;
  }
}

// smooth adjust brigthness (0-255)
void Display::dimmer()
{
  static bool bDelay;

  bDelay = !bDelay;
  if(m_bright == m_brightness || bDelay)
    return;

  if(m_brightness > m_bright + 1 && ee.brightLevel > 50)
    m_bright += 2;
  else if(m_brightness > m_bright)
    m_bright ++;
  else
  {
    m_bright --;
    if(m_bSwitchToHome)
    {
      m_bSwitchToHome = false;
      m_nCurrTile = 2; // wake with WB tile
      m_nCurrRow = m_tile[m_nCurrTile].nRow;
      drawTile(m_nCurrTile, false, 0, 0); // update before next wake
    }
  }
  analogWrite(TFT_BL, m_bright);
}

// Draw RSSI bars
void Display::btnRSSI(Button *pBtn, int16_t x, int16_t y)
{
#define RSSI_CNT 8
  static int16_t rssi[RSSI_CNT];
  static uint8_t rssiIdx = 0;

  if(WiFi.status() != WL_CONNECTED)
    return;

  rssi[rssiIdx] = WiFi.RSSI();
  if(++rssiIdx >= RSSI_CNT) rssiIdx = 0;

  int16_t rssiAvg = 0;
  for(int i = 0; i < RSSI_CNT; i++)
    rssiAvg += rssi[i];

  rssiAvg /= RSSI_CNT;

  const uint8_t wh = pBtn->r.w; // width and height
  const uint8_t sect = 127 / 5; //
  const uint8_t dist = wh  / 5; // distance between blocks
  int16_t sigStrength = 127 + rssiAvg;

  for (uint8_t i = 1; i < 6; i++)
  {
    sprite.fillRect( x + pBtn->r.x + i*dist, y + pBtn->r.y - i*dist, dist-2, i*dist, (sigStrength > i * sect) ? TFT_CYAN : bgColor );
  }
}

// update a slider from external source
void Display::setSliderValue(uint8_t st, int8_t iValue)
{
  for(uint8_t i = 0; i < m_nTileCnt; i++)
  {
    Button *pBtn = &m_tile[i].button[0];
    for(uint8_t j = 0; pBtn[j].r.x && j < BUTTON_CNT; j++)
    {
      if(m_tile[i].button[j].nFunction == st)
        m_tile[i].button[j].data[1] = iValue;
    }
  }
}

void Display::drawText(String s, int16_t x, int16_t y, int16_t angle, uint16_t cFg, uint16_t cBg, const GFXfont *pFont)
{
  TFT_eSprite textSprite = TFT_eSprite(&tft);

  textSprite.setTextDatum(TL_DATUM);
  textSprite.setTextColor(cFg, cBg);
  textSprite.setFreeFont(pFont ?  pFont : &FreeSans9pt7b);
  int16_t w = textSprite.textWidth(s) + 2;
  int16_t h = textSprite.fontHeight() + 2;
  textSprite.createSprite(w, h);
  textSprite.fillSprite( bgColor );

  textSprite.drawString(s, 0, 0);
  sprite.setPivot(x, y);
  textSprite.pushRotated(&sprite, angle);
  sprite.setPivot(0, 0);
}

// Flash a red indicator for RX, TX, PC around the top 
void Display::RingIndicator(uint8_t n)
{
  uint16_t pos = 0;

  switch(n)
  {
    case 0: // IR TX
      pos = 10;
      break;
    case 1: // IR RX
      pos = DISPLAY_WIDTH/2 - 20;
      break;
    case 2: // PC TX
      pos = DISPLAY_WIDTH - 40;
      break;
  }

  tft.drawWideLine(pos, 5, pos + 20, 5, 4, TFT_RED, bgColor);
}
