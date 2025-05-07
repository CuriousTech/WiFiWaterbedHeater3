#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h> // TFT_eSPI library from library manager?
#include "WB.h"
#include "digitalFont.h"

// from 5-6-5 to 16 bit value (max 31, 63, 31)
#define rgb16(r,g,b) ( ((uint16_t)r << 11) | ((uint16_t)g << 5) | (uint16_t)b )

#define DISPLAY_WIDTH TFT_HEIGHT
#define DISPLAY_HEIGHT TFT_WIDTH

enum Button_Function
{
  BTF_None,
  BTF_LightsDimmer,
  BTF_Brightness,
  BTF_Sleep,
  BTF_SleepInc,
  BTF_SleepDec,
  BTF_Volume,

  BTF_Restart,
  BTF_WbTempInc,
  BTF_WbTempDec,
  BTF_History,

  BTF_Lights,
  BTF_Clear,
  BTF_RSSI,
  BTF_Time,
  BTF_AMPM,
  BTF_Date,

  BTF_SetTemp,
  BTF_WBTemp,
  BTF_RoomTemp,
  BTF_OutTemp,

  BTF_StatCmd, // stat commands
  BTF_Stat_Temp,
  BTF_Stat_SetTemp,
  BTF_Stat_Fan,

  BTF_TrackBtn,

  BTF_ProgressBar,
  BTF_RadarDist,
};

// Button flags
#define BF_BORDER     (1 << 0) // Just a text time, not button
#define BF_REPEAT     (1 << 1) // repeatable (hold)
#define BF_STATE_2    (1 << 2) // tri-state
#define BF_FIXED_POS  (1 << 3)
#define BF_TEXT       (1<< 4)
#define BF_SLIDER_H   (1<< 5)
#define BF_SLIDER_V   (1<< 6)
#define BF_ARROW_UP   (1<< 7)
#define BF_ARROW_DOWN (1<< 8)
#define BF_ARROW_LEFT (1<< 9)
#define BF_ARROW_RIGHT (1<< 10)

// Tile extras
#define EX_NONE  (0)
#define EX_NOTIF  (1 << 2)
#define EX_SCROLL (1 << 3)

struct Rect{
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct Button
{
  uint16_t row;         // Used with w to calculate x positions
  uint16_t flags;       // see BF_ flags
  uint16_t nFunction;   // see enum Button_Function
  const char *pszText;  // Button text
  const GFXfont *pFont; // button text font (NULL = default)
  uint16_t textColor;   // text color (0 = default for now)
  const unsigned short *pIcon[2]; // Normal, pressed icons
  Rect r;             // Leave x,y 0 if not fixed, w,h calculated if 0, or set icon size here
  uint16_t data[4];   // data[1]=slider value
};

struct Tile
{
  const char *pszTitle;    // Top text
  uint8_t nRow;
  uint8_t Extras;           // See EX_ flags
  int16_t nScrollIndex;     // page scrolling top offset
  uint16_t nPageHeight;     // height for page scrolling
  const unsigned short *pBGImage; // background image for screen
#define BUTTON_CNT 30
  Button button[BUTTON_CNT]; // all the buttons
};

class Display
{
public:
  Display()
  {
  }
  void init(void);
  void oneSec(void);
  void service(void);
  void toast(const char *pszNote);
  void Notify(char *pszNote, IPAddress ip);
  void NotificationCancel(IPAddress ip);
  void setSliderValue(uint8_t st, int8_t iValue);
  void RingIndicator(uint8_t n);
  void updateTile(void);
  void checkNotif(void);

private:
  void swipeCheck(int16_t dX, int16_t dY);
  void checkButtonHit(int16_t touchXstart, int16_t touchYstart);
  void startSwipe(void);
  void swipeTile(int16_t dX, int16_t dY);
  void drawSwipeTiles(void);
  void snapTile(void);
  void swipeBump(void);
  void drawTile(int8_t nTile, bool bFirst, int16_t x, int16_t y);
  bool scrollPage(uint8_t nTile, int16_t nDelta);
  void drawButton(Tile& pTile, Button *pBtn, bool bPressed, int16_t x, int16_t y, bool bInit);
  void buttonCmd(Button *pBtn, bool bRepeat);
  void dimmer(void);
  void btnRSSI(Button *pBtn, int16_t x, int16_t y);
  void drawNotifs(Tile& pTile, int16_t x, int16_t y);
  void drawText(String s, int16_t x, int16_t y, int16_t angle, uint16_t cFg, uint16_t cBg, const GFXfont *pFont );

public:
  void formatButtons(Tile& pTile);
#define TILES 9
/*
Tile layout
   [0]
[1][WB][L][M]
   [2]
   [3]
*/
  Tile m_tile[TILES] =
  {
    // Pull-down tile (first)
    {
      "Settings",
      0, // row 0
      EX_NONE,
      0,
      0,
      NULL,
      {
        { 0, BF_TEXT, 0, " Sleep:", NULL, 0,  {0}, {0,0, 50, 32}},
        { 0, BF_BORDER|BF_TEXT, BTF_Sleep, "90",  NULL, 0, {0}, {0, 0, 60, 32}},
        { 0, BF_REPEAT|BF_ARROW_LEFT, BTF_SleepDec, NULL, NULL, 0, {0}, {0, 0, 32, 32}},
        { 0, BF_REPEAT|BF_ARROW_RIGHT, BTF_SleepInc, NULL, NULL, 0, {0}, {0, 0, 32, 32}},
        { 1, 0, BTF_Restart, "Restart", NULL, 0, {0},  {0, 0, 112, 28}},
        { 1, BF_FIXED_POS, BTF_RSSI, "", NULL, 0, {0}, {(DISPLAY_WIDTH/2 - 26/2), 180 + 26, 26, 26} },
        { 2, BF_SLIDER_V|BF_FIXED_POS|BF_TEXT, BTF_Brightness, "Brightness", &FreeSans7pt7b, TFT_CYAN, {0}, {10, 40, 20, DISPLAY_HEIGHT - 80} },
        {0xFF}
      }
    },
    //
    {
      "Thermostat",
      1, // row 1
      EX_NONE,
      0,
      0,
      NULL,
      {
        { 0, BF_TEXT, 0, "Out:",  NULL, 0, {0}, {0, 0, 0, 32}},
        { 0, BF_BORDER|BF_TEXT, BTF_OutTemp, "",  NULL, 0, {0}, {0, 0, 60, 32}},
        { 0, BF_TEXT, 0, "", NULL, 0,  {0}, {0, 0, 32, 32}}, // spacer
        { 1, BF_TEXT, 0, " In:", NULL, 0,  {0}, {0, 0, 38, 32}},
        { 1, BF_BORDER|BF_TEXT, BTF_Stat_Temp, "",  NULL, 0, {0}, {0, 0, 60, 32}},
        { 1, BF_REPEAT|BF_ARROW_UP, BTF_StatCmd, NULL, NULL, 0, {0}, {0, 0, 32, 32}, {3}},
        { 2, BF_TEXT, 0, "Set:", NULL, 0,  {0}, {0, 0, 0, 32}},
        { 2, BF_BORDER|BF_TEXT, BTF_Stat_SetTemp, "",  NULL, 0, {0}, {0, 0, 60, 32}},
        { 2, BF_REPEAT|BF_ARROW_DOWN, BTF_StatCmd, NULL, NULL, 0, {0}, {0, 0, 32, 32}, {2}},
        { 3, 0, BTF_Stat_Fan, "Fan", NULL, 0, {0, 0}, {0, 0, 0, 32},(1)}, // sends 1 to PC script
        {0xFF}
      }
    },
    // tile 2 (waterbed)
    {
      "",
      1, // row 1
      EX_NONE,
      0,
      0,
      0,
      {
        { 0, BF_FIXED_POS|BF_TEXT, BTF_Time,  "12:00",    &DIGIRT_40pt7b, TFT_CYAN,  {0}, {  3,  2, 160, 50} },
        { 0, BF_FIXED_POS|BF_TEXT, BTF_AMPM, "00 AM SUN", &DIGIRT_18pt7b, TFT_CYAN,  {0}, {180,  2, 120, 32} },
        { 0, BF_FIXED_POS|BF_TEXT, BTF_Date,  "Jan 01",   &DIGIRT_18pt7b, TFT_CYAN,  {0}, {180, 33,  80, 32} },

        { 1, BF_FIXED_POS|BF_TEXT, BTF_None, "Room Temp", &FreeSans7pt7b, TFT_WHITE, {0}, {40, 125, 80, 32} },
        { 1, BF_FIXED_POS|BF_TEXT, BTF_RoomTemp,  "0.0",  &DIGIRT_18pt7b, TFT_CYAN,  {0}, {40, 148, 80, 32} },
        { 1, BF_FIXED_POS|BF_TEXT, BTF_None, "Out Temp",  &FreeSans7pt7b, TFT_WHITE, {0}, {40, 175, 80, 32} },
        { 1, BF_FIXED_POS|BF_TEXT, BTF_OutTemp,   "0.0",  &DIGIRT_18pt7b, TFT_CYAN,  {0}, {40, 198, 80, 32} },

#ifdef RADAR_H
        { 1, BF_FIXED_POS|BF_TEXT, BTF_RadarDist,   "0.0",  &FreeSans9pt7b, TFT_CYAN,  {0}, {40, 100, 200, 32}},
#endif
        { 1, BF_FIXED_POS|BF_TEXT, BTF_None, "Set Temp",  &FreeSans7pt7b, TFT_WHITE, {0}, {DISPLAY_WIDTH-140, 125, 80, 32}},
        { 1, BF_FIXED_POS|BF_TEXT, BTF_SetTemp,   "0.0",  &DIGIRT_18pt7b, TFT_CYAN,  {0}, {DISPLAY_WIDTH-140, 148, 80, 32}},
        { 1, BF_FIXED_POS|BF_TEXT, BTF_None, "Bed Temp",  &FreeSans7pt7b, TFT_WHITE, {0}, {DISPLAY_WIDTH-140, 175, 80, 32}},
        { 1, BF_FIXED_POS|BF_TEXT, BTF_WBTemp,    "0.0",  &DIGIRT_18pt7b, TFT_CYAN,  {0}, {DISPLAY_WIDTH-140, 198, 80, 32}},

        { 1, BF_FIXED_POS|BF_REPEAT|BF_ARROW_UP,   BTF_WbTempInc, NULL, NULL, 0, {0}, {DISPLAY_WIDTH-40, 140,    40, 44}},
        { 1, BF_FIXED_POS|BF_REPEAT|BF_ARROW_DOWN, BTF_WbTempDec, NULL, NULL, 0, {0}, {DISPLAY_WIDTH-40, 140+46, 40, 44}},

        { 0, BF_FIXED_POS, BTF_RSSI,        "", NULL, 0, {0}, {6, DISPLAY_HEIGHT - 6, 26, 26}  },
        {0xFF}
      }
    },
    // tile 3 (right horizontal tile)
    {
      "Music",
      1, // row 1
      EX_SCROLL,
      0,
      0,
      NULL,
      {
        { 0, BF_SLIDER_V|BF_FIXED_POS|BF_TEXT, BTF_Volume, "", &FreeSans7pt7b, 0,  {0}, {DISPLAY_WIDTH-26, 40, 20, DISPLAY_HEIGHT - 80}},
        {0xFF}
      }
    },
    // tile 4 (rightmost horizontal tile)
    {
      "Lights",
      1, // row 1
      EX_SCROLL,
      0,
      0,
      NULL,
      {
        { 0, BF_SLIDER_V|BF_FIXED_POS, BTF_Lights, "", NULL, 0,  {0}, {10, 40, 20, DISPLAY_HEIGHT - 80} },
        {0xFF}
      }
    },
    //
    {
      "History",
      2, // row 2
      EX_NONE,
      0,
      0,
      NULL,
      {
        { 0, BF_BORDER|BF_FIXED_POS, BTF_History, "", NULL, 0, {0}, {10, 40, 250, 150} },
        {0xFF}
      }
    },
    // Notification tile (very last)
    {
      "Notifications",
      3,
      EX_NOTIF | EX_SCROLL,
      0,
      0,
      NULL,
      {
        { 0, BF_FIXED_POS, BTF_Clear, "Clear", NULL, 0, {0}, {0, DISPLAY_HEIGHT-24, 180, 24} }, // force y
        {0xFF}
      }
    },
  };

private:
  IPAddress  m_notifyIp;
  uint8_t    m_bright; // current brightness
  uint8_t    m_nTileCnt;
  int8_t     m_nCurrTile; // start screen
  Button    *m_pCurrBtn; // for button repeats and release
  uint32_t   m_lastms; // for button repeats
  uint32_t   m_nDispFreeze;
  uint8_t    m_nRowStart[TILES]; // index of screen at row
  uint8_t    m_nColCnt[TILES]; // column count for each row of screens
  uint16_t   m_sleepTimer;
  int16_t    m_swipePos;
  bool       m_bSwipeReady;
  uint8_t    m_nNextTile;
  uint8_t    m_nCurrRow;
  uint8_t    m_nLastRow;
  bool       m_bPopupActive;
  uint16_t   m_popupLeft;
  uint16_t   m_popupTop;
  uint16_t   m_popupHeight;
  uint16_t   m_popupWidth;
  bool       m_bSwitchToHome;

#define NOTE_CNT 20
  const char *m_pszNotifs[NOTE_CNT + 1];

public:
  uint8_t  m_brightness = 200; // initial brightness
  uint16_t m_backlightTimer = 90; // backlight/screensaver timer, seconds
  uint8_t  m_nWsConnected; // websockets connected (to stay awake)

  uint16_t m_statTemp;
  uint16_t m_statSetTemp;
  bool     m_bStatFan;
  uint16_t m_outTemp;
  uint8_t  m_outRh;
};

extern Display display;

#endif // DISPLAY_H
