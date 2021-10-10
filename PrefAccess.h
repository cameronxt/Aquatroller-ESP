// TODO: Figure out how im going to store data
// TODO: Figure out data layout
// TODO:? backup/restore eeprom?

#ifndef PrefAccess_h
#define PrefAccess_h
#include "Arduino.h"
#include "FS.h"
#include <SPIFFS.h>


struct Prefs {
  int Version = 1;
  int test = 7;
  bool test1 = true;
  char test2 = 'b';
};



class PrefAccess {
    //Prefs _prefTemp;
    struct Prefs _prefs, _tempPrefs;
    int pinCS = 10;
    bool _enableLogging = true;
  public:
    void init();
    void loop();
    bool loadPrefs();
    void applyPrefs();
    void savePrefs();

    // From SPIFFS_Test example
    void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
    void readFile(fs::FS &fs, const char * path);
    void writeFile(fs::FS &fs, const char * path, const char * message);
    void appendFile(fs::FS &fs, const char * path, const char * message);
    void renameFile(fs::FS &fs, const char * path1, const char * path2);
    void deleteFile(fs::FS &fs, const char * path);
    void testFileIO(fs::FS &fs, const char * path);

  private:
};


#endif
