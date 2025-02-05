
#ifndef MEDIA_H
#define MEDIA_H

#include <Arduino.h>
#include <cstring>
#include "FS.h"
#include "SD_MMC.h"
#include <Audio.h>  // ESP32-audioI2S-master (ESP32 2.0.14 compatible version from WaveShare example) 

#define INTERNAL_FS FFat // Make it the same as the partition scheme uploaded (SPIFFS, LittleFS, FFat)

#define SD_CLK_PIN  14
#define SD_CMD_PIN  17 
#define SD_D0_PIN   16 
#define SD_D1_PIN   -1 
#define SD_D2_PIN   -1 
#define SD_D3_PIN   21 

#define I2S_DOUT    47
#define I2S_BCLK    48  
#define I2S_LRC     38      // I2S_WS

#define Volume_MAX  21

enum _sounds
{
  SND_CLICK,
  SND_ALERT,
};

struct FileEntry
{
  char Name[100];
  uint32_t Size;
  time_t Date;
};

class Media
{
public:
  Media(void);
  void init(void);
  void service(void);

  void Sound(uint8_t n);
  void setVolume(uint8_t volume);
  void Play(const char* directory, const char* fileName);
  void Pause(void);
  void Resume(void);
  uint32_t Music_Duration(void);
  uint32_t Music_Elapsed(void);
  uint16_t Music_Energy(void);
  String fileListJson(bool bInternal);
  void deleteIFile(char *pszFilename);
  void deleteSDFile(char *pszFilename);

  bool m_bCardIn;
  bool m_bCardInit;
  bool m_bPlaying;
  uint8_t m_volume = 100;

private:
  uint16_t Folder_retrieval(fs::FS &fs, const char* directory, FileEntry list[],uint16_t maxFiles);
  void Audio_Init(void);

#define FILELIST_CNT 32
  FileEntry SDList[FILELIST_CNT];
  FileEntry InternalList[FILELIST_CNT];
};

extern Media media;

#endif // MEDIA_H
