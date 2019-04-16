// Written by Michele <o-zone@zerozone.it> Pinassi
// Released under GPLv3 - No any warranty

#include "SD_MMC.h" //sd card esp32

bool initSDCard() {
  if(!SD_MMC.begin("/sdcard",true)){
    DEBUG_PRINT("Card Mount Failed");
    return false;
  }
  uint8_t cardType = SD_MMC.cardType();

  if(cardType == CARD_NONE){
    DEBUG_PRINT("No SD card attached");
    return false;
  }

  if(cardType == CARD_MMC){
    DEBUG_PRINT("Card type MMC");
  } else if(cardType == CARD_SD){
    DEBUG_PRINT("Card type SDSC");
  } else if(cardType == CARD_SDHC){
    DEBUG_PRINT("Card type SDHC");
  } else {
    DEBUG_PRINT("Card type UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  
  return true;
}

bool writeFile(const char *path, const char *file_data) {
  File file = SD_MMC.open(path, FILE_WRITE);
  if(!file){
    DEBUG_PRINT("Failed to open file for writing");
    return false;
  }
  if(!file.print(file_data)){
    DEBUG_PRINT("Error while writing to file!");
    return false;
  }
  file.close();
  return true;
}
