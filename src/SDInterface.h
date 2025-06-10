#pragma once

#ifndef SDInterface_h
#define SDInterface_h

#include "configs.h"

#include "settings.h"
#include "SD.h"
#include "Buffer.h"
#ifdef HAS_SCREEN
  #include "Display.h"
#endif
#include "utils.h"
#include <Update.h>

extern Buffer buffer;
extern Settings settings;
#ifdef HAS_SCREEN
  extern Display display_obj;
#endif

#ifdef KIT
  #define SD_DET 4
#endif

class SDInterface {

  private:
  #if (defined(MARAUDER_M5STICKC) || defined(HAS_CYD_TOUCH))
    SPIClass *spiExt;
  #endif
    bool checkDetectPin();

  public:
    uint8_t cardType;
    uint64_t cardSizeMB;
    bool supported = false;

    String card_sz;
  
    bool initSD();

    LinkedList<String>* sd_files;

    void listDir(String str_dir);
    void listDirToLinkedList(LinkedList<String>* file_names, String str_dir = "/", String ext = "");
    File getFile(String path);
    void runUpdate();
    void performUpdate(Stream &updateSource, size_t updateSize);
    void main();
    bool removeFile(String file_path);
};

#endif
