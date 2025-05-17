#include "Media.h"
#include "eeMem.h"
#include "jsonString.h"

extern void WsSend(String s);

Audio audio;

Media::Media()
{ 
}

void Media::service()
{
  static File Path;
  static File file;
  static uint16_t fileCount;

  audio.loop();

  // non-blocking dir list because this can take a while
  if(!m_bCardIn)
    return;

  if(m_bDirty)
  {
    m_bDirty = false;
    if(Path = SD_MMC.open(m_sdPath[0] ? m_sdPath : "/"))
    {
      memset(SDList, 0, sizeof(SDList));
      file = Path.openNextFile();
    }
    fileCount = 0;
  }

  if(Path && file)
  {
    strncpy(SDList[fileCount].Name, file.name(), maxPathLength - 1);
    SDList[fileCount].Size = file.size();
    SDList[fileCount].bDir = file.isDirectory();
    SDList[fileCount].Date = file.getLastWrite();
    fileCount++;
    file = Path.openNextFile();
    if(!file || fileCount >= FILELIST_CNT - 1 )
    {
      file.close();
      Path.close();

      jsonString js("files");
      js.Array("list", SDList, m_sdPath[1] ? true:false );
      WsSend(js.Close());
    }
  }
}

void Media::init()
{
  pinMode(SD_D3_PIN, OUTPUT);

  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN,-1,-1,-1);

  Audio_Init();

  digitalWrite(SD_D3_PIN, HIGH); //enable
  vTaskDelay(pdMS_TO_TICKS(10));

  if (SD_MMC.begin("/sdcard", true, true) )
  {
    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE || cardType == CARD_UNKNOWN)
    {
//      ets_printf("No SD card\r\n");
      return;
    }
    m_bCardIn = true;
    strcpy(m_sdPath, "/"); // start with root
    setDirty();
  }

  INTERNAL_FS.begin(true); // mount FFat after SD for correct free space
}

void Media::fillFileBButtons(Tile& pTile)
{
  uint8_t i;

  if(!m_bCardIn)
    return;

  for(i = 0; i < BUTTON_CNT - 2 && SDList[i].Name[0]; i++)
  {
    pTile.button[i+1].pszText = SDList[i].Name;
    pTile.button[i+1].row = i;
    pTile.button[i+1].r.w = 260;
    pTile.button[i+1].r.h = 20;
    pTile.button[i+1].nFunction = BTF_TrackBtn;
  }

  pTile.button[i].row = 0xFF; // end row
}

void Media::Audio_Init()
{
  // Audio
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  setVolume(m_volume);
}

void Media::setVolume(uint8_t volume)
{
  if(volume > Volume_MAX )
    return;
  m_volume = volume;
  audio.setVolume(volume * 21 / 100); // 0...21
}

const char *pSounds[] = {
  "/click.wav",
  "/alert.wav",
};

// Play a sound from the internal FS
void Media::Sound(uint8_t n)
{
  if( !INTERNAL_FS.exists(pSounds[n]) )
    return;

  if(audio.isRunning())
    return;

  if(audio.connecttoFS(INTERNAL_FS, pSounds[n]))
  {
    Pause();
    Resume();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void Media::Play(const char* directory, const char* fileName)
{
  static const char *pLast;

  if(fileName == pLast && audio.isRunning())
  {
    audio.pauseResume();
    pLast = NULL;
    return;
  }
  pLast = fileName;

  // SD Card
  char filePath[maxPathLength];
  if(!strcmp(directory, "/"))
    snprintf(filePath, maxPathLength, "%s%s", directory, fileName);   
  else
    snprintf(filePath, maxPathLength, "%s/%s", directory, fileName);

  if(m_bCardIn)
  {
    if ( SD_MMC.exists(filePath))
    {
      audio.pauseResume();
      vTaskDelay(pdMS_TO_TICKS(100));
      if( !audio.connecttoFS(SD_MMC,(char*)filePath) )
      {
  //      ets_printf("Music Read Failed\r\n");
        return;
      }
  //    ets_printf("playing\r\n");
      Pause();
      Resume();
      vTaskDelay(pdMS_TO_TICKS(100));
      return;
    }
  }

  if ( INTERNAL_FS.exists(filePath))
  {
    audio.pauseResume();
    if( !audio.connecttoFS(INTERNAL_FS, (char*)filePath) )
    {
//      ets_printf("Music Read Failed\r\n");
      return;
    }
    Pause();
    Resume();
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }
}

void Media::Pause()
{
  if(!audio.isRunning())
    return;
  audio.pauseResume();
}

void Media::Resume()
{
  if(audio.isRunning())
    return;
  audio.pauseResume();
}

uint32_t Media::Music_Duration()
{
  uint32_t Audio_duration = audio.getAudioFileDuration(); 
/*
  if(Audio_duration > 60)
    ets_printf("Audio duration is %d minutes and %d seconds\r\n", Audio_duration/60, Audio_duration%60);
  else{
    if(Audio_duration != 0)
      ets_printf("Audio duration is %d seconds\r\n", Audio_duration);
    else
      ets_printf("Fail : Failed to obtain the audio duration.\r\n");
  }*/
  vTaskDelay(pdMS_TO_TICKS(10));

  return Audio_duration;
}

uint32_t Media::Music_Elapsed()
{
  return audio.getAudioCurrentTime();
}

uint16_t Media::Music_Energy()
{
  return audio.getVUlevel();
}

bool Media::SDCardAvailable()
{
  return m_bCardIn;
}

const char *Media::currFS()
{
  return m_bSDActive ? "SDCard" : "Internal";
}

void Media::setFS(char *pszValue)
{
  if(m_bCardIn)
    m_bSDActive = !strcmp(pszValue, "SD"); // For now, just use SD
  setPath("/");
}

uint32_t Media::freeSpace()
{
  uint64_t nFree;

  if(m_bSDActive)
  {
    if(m_bCardIn)
      nFree = SD_MMC.totalBytes() - SD_MMC.usedBytes();
  }
  else
  {
    nFree = INTERNAL_FS.totalBytes() - INTERNAL_FS.usedBytes();
  }
  return (uint32_t)(nFree >> 10);
}

void Media::setPath(char *szPath)
{
  if(m_bSDActive)
  {
    strncpy(m_sdPath, szPath, sizeof(m_sdPath)-1);
    setDirty();

    String s = "{\"cmd\":\"files\",\"list\":[";
    if(szPath[1])
      s += "[\"..\",0,1,0]";
    s += "]}";
    WsSend(s); // fill in for an empty dir
  }
  else
  {
    File Path = INTERNAL_FS.open(szPath[0] ? szPath : "/");
    if (!Path)
      return;

    uint16_t fileCount = 0;
    File file = Path.openNextFile();
  
    String s = "{\"cmd\":\"files\",\"list\":[";
    if(szPath[1])
    {
      s += "[\"..\",0,1,0]";
      fileCount++;
    }

    while (file)
    {
      if(fileCount)
        s += ",";
      s += "[\"";
      s += file.name();
      s += "\",";
      s += file.size();
      s += ",";
      s += file.isDirectory();
      s += ",";
      s += file.getLastWrite();
      s += "]";
      fileCount++;
      file = Path.openNextFile();
    }
    Path.close();
    s += "]}";
    WsSend(s);
  }
}

void Media::deleteFile(char *pszFilename)
{
  String sRemoveFile = "/";
  sRemoveFile += pszFilename;

  if(m_bSDActive)
  {
    if(!m_bCardIn)
      return;
    SD_MMC.remove(sRemoveFile);
    m_bDirty = true;
  }
  else
  {
    INTERNAL_FS.remove(sRemoveFile);
  }
}

File Media::createFile(String sFilename)
{
  String sFile = "/";
  sFile += sFilename;

  if(m_bSDActive)
    return SD_MMC.open(sFile, "w");
  return INTERNAL_FS.open(sFile, "w");
}

void Media::createDir(char *pszName)
{
  if(m_bSDActive)
  {
   if( !SD_MMC.exists(pszName) )
    SD_MMC.mkdir(pszName);
  }
  else
  {
    if( !INTERNAL_FS.exists(pszName) )
      INTERNAL_FS.mkdir(pszName);
  }
}

void Media::setDirty()
{
  if(m_bCardIn)
    m_bDirty = true;
}
