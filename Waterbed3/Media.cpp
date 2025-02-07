#include "Media.h"
#include "eeMem.h"
#include "jsonString.h"

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

  // async dir list because this can take a while
  if(m_bCardIn)
  {
    if(m_bDirty)
    {
      m_bDirty = false;
      Path = SD_MMC.open("/");
      file = Path.openNextFile();
      fileCount = 0;
    }
  
    if(Path && file)
    {
      if (!file.isDirectory())
      {
        strncpy(SDList[fileCount].Name, file.name(), maxPathLength - 1);
        SDList[fileCount].Size = file.size();
        SDList[fileCount].Date = file.getLastWrite();
        fileCount++;
      }
      file = Path.openNextFile();
      if(!file || fileCount >= FILELIST_CNT - 1 )
      {
        file.close();
        Path.close();
      }
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
  }

  if(INTERNAL_FS.begin(true))
  {
    Folder_retrieval(INTERNAL_FS, "/", InternalList, FILELIST_CNT - 2);
  }

  setDirty();
}

void Media::fillFileBButtons(Tile& pTile)
{
  uint8_t i;

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

uint16_t Media::Folder_retrieval(fs::FS &fs, const char* directory, FileEntry list[], uint16_t maxFiles)
{
  File Path = fs.open(directory);
  if (!Path)
  {
//    ets_printf("Path: <%s> does not exist\r\n", directory);
    return 0;
  }
  
  uint16_t fileCount = 0;
  File file = Path.openNextFile();

  while (file && fileCount < maxFiles)
  {
    if (!file.isDirectory())
    {
      strncpy(list[fileCount].Name, file.name(), maxPathLength - 1);
      list[fileCount].Size = file.size();
      list[fileCount].Date = file.getLastWrite();
      fileCount++;
    }
    file = Path.openNextFile();
  }
  Path.close();                                                         
  return fileCount;                                                 
}

String Media::internalFileListJson()
{
  // refresh the list before sending
  memset(InternalList, 0, sizeof(InternalList));
  Folder_retrieval(INTERNAL_FS, "/", InternalList, FILELIST_CNT-1);

  jsonString js("files");
  js.Array("list", InternalList);
  return js.Close();
}

String Media::sdFileListJson()
{
  if(m_bDirty)
  {
    memset(SDList, 0, sizeof(SDList)); // warning: use same sizes
    Folder_retrieval(SD_MMC, "/", SDList, FILELIST_CNT-1);
    m_bDirty = false;
  }

  jsonString js("filesSD");
  js.Array("list", SDList );
  return js.Close();
}


void Media::deleteIFile(char *pszFilename)
{
  String sRemoveFile = "/";
  sRemoveFile += pszFilename;
  INTERNAL_FS.remove(sRemoveFile);
}

void Media::deleteSDFile(char *pszFilename)
{
  String sRemoveFile = "/";
  sRemoveFile += pszFilename;
  SD_MMC.remove(sRemoveFile);
  m_bDirty = true;
}

void Media::setDirty()
{
  m_bDirty = true;
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
  if(!m_bCardIn)
    return;

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
  if(!m_bCardIn || !audio.isRunning())
    return;
  audio.pauseResume();
}

void Media::Resume()
{
  if(!m_bCardIn || audio.isRunning())
    return;
  audio.pauseResume();
}

uint32_t Media::Music_Duration()
{
  if(!m_bCardIn)
    return 0;

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
  if(!m_bCardIn)
    return 0;
  return audio.getAudioCurrentTime();
}

uint16_t Media::Music_Energy()
{
  if(!m_bCardIn)
    return 0;
  return audio.getVUlevel();
}
