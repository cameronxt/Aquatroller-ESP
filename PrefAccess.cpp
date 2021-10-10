// save a copy of eeprom settings
// create generic log creator
// delete log entries after upload or after certain time has passed
#include "PrefAccess.h"

void PrefAccess::init() {

  if (!SPIFFS.begin( true )) {
    ESP_LOGE("Prefs", "An error has occured. Unable to mount filesystem");
  }

  loop();

  // Serial.print(F("Initializing SD Card..."));
  // // Initialize the SD card
  // if (SD.begin(5)) {
  //   _enableLogging = true;
  //   ESP_LOGD("SD", " SD card mounted, logging enabled "));
  // } else {
  //   _enableLogging = false;
  //   ESP_LOGD("SD", " No SD card found, logging disabled"));
  // }

  // uint8_t cardType = SD.cardType();

  // if(cardType == CARD_NONE){
  //   ESP_LOGD("SD", "No SD card attached");
  //   return;
  // }

  // Serial.print("SD Card Type: ");
  // if(cardType == CARD_MMC){
  //   ESP_LOGD("SD", "MMC");
  // } else if(cardType == CARD_SD){
  //   ESP_LOGD("SD", "SDSC");
  // } else if(cardType == CARD_SDHC){
  //   ESP_LOGD("SD", "SDHC");
  // } else {
  //   ESP_LOGD("SD", "UNKNOWN");
  // }

}

void PrefAccess::loop() {
  loadPrefs();
  applyPrefs();
}

// returns true on success
bool PrefAccess::loadPrefs() {
 File prefFile = SPIFFS.open("/settings.dat", FILE_READ);
  if( !prefFile ){
    ESP_LOGE("Prefs", "Failed to open file for reading");
    return false;
  }
  ESP_LOGD("Prefs", "Reading prefrences from disk");
  prefFile.read( (byte *)&_tempPrefs, sizeof(_tempPrefs) );
  prefFile.close();
  return true;
}

void PrefAccess::applyPrefs(){
  if (_prefs.Version == 1) {
    ESP_LOGD("Prefs", "Found previous preferences, loading...");
    // Apply all settings
  } else {
    struct Prefs defaultPrefs;
    _prefs = defaultPrefs;
  ESP_LOGD("Prefs", "Preferences applied");
  }
}

void PrefAccess::savePrefs() {
 File prefFile = SPIFFS.open("/settings.dat", FILE_WRITE);
  if( !prefFile ){
    ESP_LOGE("Prefs", "Failed to open file for write");
    return;
  }
  prefFile.write( (byte *)&_prefs, sizeof(_prefs) );
  prefFile.close();
  ESP_LOGI("Prefs", "Preferences saved");
}

void PrefAccess::listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void PrefAccess::readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void PrefAccess::writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

void PrefAccess::appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
    file.close();
}

void PrefAccess::renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("- file renamed");
    } else {
        Serial.println("- rename failed");
    }
}

void PrefAccess::deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

void PrefAccess::testFileIO(fs::FS &fs, const char * path){
    Serial.printf("Testing file I/O with %s\r\n", path);

    static uint8_t buf[512];
    size_t len = 0;
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }

    size_t i;
    Serial.print("- writing" );
    uint32_t start = millis();
    for(i=0; i<2048; i++){
        if ((i & 0x001F) == 0x001F){
          Serial.print(".");
        }
        file.write(buf, 512);
    }
    Serial.println("");
    uint32_t end = millis() - start;
    Serial.printf(" - %u bytes written in %u ms\r\n", 2048 * 512, end);
    file.close();

    file = fs.open(path);
    start = millis();
    end = start;
    i = 0;
    if(file && !file.isDirectory()){
        len = file.size();
        size_t flen = len;
        start = millis();
        Serial.print("- reading" );
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            if ((i++ & 0x001F) == 0x001F){
              Serial.print(".");
            }
            len -= toRead;
        }
        Serial.println("");
        end = millis() - start;
        Serial.printf("- %u bytes read in %u ms\r\n", flen, end);
        file.close();
    } else {
        Serial.println("- failed to open file for reading");
    }
}