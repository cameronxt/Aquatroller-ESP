// incase it gets added multiple times
#ifndef bluetooth_h
#define bluetooth_h
#include "Arduino.h"

//#include "eepromaccess.h"

const byte btBufferSize = 32;

struct BTParse {
  byte primary;
  byte option;
  byte subOption;
  byte value;

  union Values {      // A litle union magic so we can read incoming data more easily
    float fValue; // 4 Bytes
    unsigned long lValue; // 4 Bytes
    char cValue[4]; // 4 Bytes
    int iValue; // 2 Bytes
  } values;
};

// Maybe make const out of these
//struct BTSettings {
  const int txPin = 0;
  const int rxPin = 1;


//};

class BluetoothModule {
  public:
    BluetoothModule();
    void setup();
    void loop();

    bool newParse;              // True when data has been parsed, but not retrieved
    BTParse parsedData;         // Struct for holding parsed data

  private:
    void recievePacket();
    void parseData();

    bool newData;               // True when we have a full packet to parse

    char receivedChars[btBufferSize];       // Buffer for incoming data
    char tempChars[btBufferSize];           // Temp buffer for strtok

    char startMarker = '<';
    char endMarker = '>';
};

#endif
