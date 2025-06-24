#include "SDInterface.h"

SDInterface::SDInterface(SPIClass* spi, int cs)
  : _spi(spi), _cs(cs) {}

bool SDInterface::initSD() {
  #ifdef HAS_SD
    String display_string = "";

    pinMode(SD_CS, OUTPUT);

    delay(10);
    //enum { SPI_SCK = 0, SPI_MISO = 36, SPI_MOSI = 26 };
    //this->spiExt = new SPIClass();
    //this->spiExt->begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
    //SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS); // Don't need this if init display first and using same SPI pins
    if (!SD.begin(SD_CS, *_spi)) {
      Logger::log(WARN_MSG, "Failed to mount SD Card");
      this->supported = false;
      return false;
    }
    else {
      Logger::log(GUD_MSG, "SD mounted successfully!");
      this->supported = true;
      this->cardType = SD.cardType();

      this->cardSizeMB = SD.cardSize() / (1024 * 1024);
    
      if (this->supported) {
        const int NUM_DIGITS = log10(this->cardSizeMB) + 1;

        char sz[NUM_DIGITS + 1];

        sz[NUM_DIGITS] =  0;
        for ( size_t i = NUM_DIGITS; i--; this->cardSizeMB /= 10)
        {
            sz[i] = '0' + (this->cardSizeMB % 10);
            display_string.concat((String)sz[i]);
        }
  
        this->card_sz = sz;
      }

      if (!SD.exists("/SCRIPTS")) {
        Logger::log(STD_MSG, "/SCRIPTS does not exist. Creating...");

        SD.mkdir("/SCRIPTS");
        Logger::log(STD_MSG, "/SCRIPTS created");
      }

      this->sd_files = new LinkedList<String>();

      //this->sd_files->add("Back");

      this->update_bin_file = settings.loadSetting<String>("UpdateFile");
    
      return true;
  }

  #else
    Logger::log(WARN_MSG, "SD support disabled, skipping init");
    return false;
  #endif
}

File SDInterface::getFile(String path) {
  if (this->supported) {
    File file = SD.open(path, FILE_READ);

    //if (file)
    return file;
  }
}

bool SDInterface::removeFile(String file_path) {
  if (SD.remove(file_path))
    return true;
  else
    return false;
}

void SDInterface::listDirToLinkedList(LinkedList<String>* file_names, String str_dir, String ext) {
  if (this->supported) {
    File dir = SD.open(str_dir);
    while (true)
    {
      File entry = dir.openNextFile();
      if (!entry)
      {
        break;
      }

      if (entry.isDirectory())
        continue;

      String file_name = entry.name();
      if (ext != "") {
        if (file_name.endsWith(ext)) {
          file_names->add(file_name);
        }
      }
      else
        file_names->add(file_name);
    }
  }
}

void SDInterface::listDir(String str_dir){
  if (this->supported) {
    File dir = SD.open(str_dir);
    while (true)
    {
      File entry = dir.openNextFile();
      if (! entry)
      {
        break;
      }
      //for (uint8_t i = 0; i < numTabs; i++)
      //{
      //  Serial.print('\t');
      //}
      Serial.print(entry.name());
      Serial.print("\t");
      Serial.println(entry.size());
      entry.close();
    }
  }
}

String SDInterface::findFirstBinFile(const String& dirPath) {
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return "";  // Not a valid directory
  }

  File entry;
  while ((entry = dir.openNextFile())) {
    if (!entry.isDirectory()) {
      String filename = entry.name();
      // If the file is in a subfolder, strip the path
      int lastSlash = filename.lastIndexOf('/');
      if (lastSlash != -1) {
        filename = filename.substring(lastSlash + 1);
      }
      if (filename.endsWith(".bin")) {
        entry.close();
        dir.close();
        return filename;
      }
    }
    entry.close();
  }

  dir.close();
  return "";  // No .bin file found
}

void SDInterface::runUpdate() {
  String bin_name = this->findFirstBinFile("/");

  if (bin_name == "") {
    Logger::log(STD_MSG, "No bin file found");
    return;
  }

  if (bin_name == this->update_bin_file) {
    Logger::log(STD_MSG, "No new bin file found");
    return;
  }
  else {
    display.clearScreen();
    display.tft->setCursor(0, 0);
    display.tft->print("Updating Firmware:\n");
    display.tft->println(bin_name);
    Logger::log(WARN_MSG, "Found new bin file (" + bin_name + "). Updating...");
    this->update_bin_file = bin_name;
    settings.saveSetting<bool>(UPDATE_KEY, this->update_bin_file);
  }

  File updateBin = SD.open("/" + bin_name);

  // Check if file is good
  if (updateBin) {
    // Check if it is a directory first
    if(updateBin.isDirectory()){
      Logger::log(WARN_MSG, "Error, could not find update file");
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    // Check file size
    if (updateSize > 0) {
      Logger::log(STD_MSG, "Starting update over SD. Please wait...");
      this->performUpdate(updateBin, updateSize);
    }
    // File is empty
    else {
      Logger::log(WARN_MSG, "Error, file is empty");
      display.tft->println("File is empty. Exiting...");
      return;
    }

    updateBin.close();
    
    // whe finished remove the binary from sd card to indicate end of the process
    Logger::log(STD_MSG, "rebooting...");
    display.tft->println("Complete. Rebooting...");
    delay(1000);
    ESP.restart();
  }
  // File was not good
  else {
    display.tft->println("Could not load bin");
    display.tft->println("Exiting...");
    Logger::log(WARN_MSG, "Could not load .bin from sd root");
  }
}

void SDInterface::performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Logger::log(STD_MSG, "Written : " + String(written) + " successfully");
    }
    else {
      Logger::log(WARN_MSG, "Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end()) {
      Logger::log(STD_MSG, "OTA done!");
      if (Update.isFinished()) {
        Logger::log(STD_MSG, "Update successfully completed. Rebooting.");
      }
      else {
        Logger::log(WARN_MSG, "Update not finished? Something went wrong!");
        display.tft->println("Update error occurred");
      }
    }
    else {
      Logger::log(WARN_MSG, "Error Occurred. Error #: " + String(Update.getError()));
      display.tft->println("Update error occurred");
    }

  }
  else
  {
    Logger::log(WARN_MSG, "Not enough space to begin OTA");
    display.tft->println("Update error occurred");
  }
}

bool SDInterface::checkDetectPin() {
  #ifdef KIT
    if (digitalRead(SD_DET) == LOW)
      return true;
    else
      return false;
  #endif

  return false;
}

void SDInterface::main() {
  if (!this->supported) {
    if (checkDetectPin()) {
      delay(100);
      this->initSD();
    }
  }
}
