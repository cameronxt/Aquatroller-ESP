#ifndef temp_h
#define temp_h
#include "Arduino.h"
#include <OneWire.h>
#include <DallasTemperature.h>

const int TEMP_PIN = 2;
const int HEATER_PIN = 4;
const int NUM_OF_SENSORS = 2;

const int TEMP_ERROR_DISCONNECTED = -196;

struct TemperatureData {
  
  float targetTemp = 82.0;             // (fahrenheit)Our desired temperature
  unsigned long heaterDelayTime = 60000;   // (seconds) How long to wait before allowing heater to cycle again
  unsigned long tempDelayTime = 10000;     // (seconds) How long to wait before getting temp again in seconds
  unsigned long conversionDelayTime = 750; // 750ms for most accurate reading

};

class Temp {
    TemperatureData _tempData;
    unsigned long _prevTempTime;               // Timer to only check temp every so often
    unsigned long _prevConversionTime;         // Timer for conversion times
    unsigned long _prevHeaterTime;             // Timer to control how often the heater can be cycled
    float _temps[NUM_OF_SENSORS];              // Most recent available temps
    bool _waitingToCheck = false;              // Flag for when we are waiting to check after conversion
    bool _isHeaterOn = false;                  // Flag to track state of heater
    bool _tempError[NUM_OF_SENSORS];
  public:
    Temp(DallasTemperature* _temp);
    void init();
    void loop();
    void heaterLoop();
    
    float getCurrentTemp();
    float getCurrentTempBySensor();

    float getTargetTemp();
    void setTargetTemp(float newTemp);
    
    unsigned int getHeaterDelay();
    void setHeaterDelay(unsigned int newDelay);
    
    unsigned int getTempDelay();
    void setTempDelay(unsigned int newDelay);

    bool isHeaterOn();
    
    TemperatureData* getDataAddress() { return &_tempData; };

  private:
    OneWire* _wire; // our onewire object to communicate with sensors
    DallasTemperature* _temp; // dallas temp object to access ds18b20
    void updateTemps();
    void turnHeaterOn();
    void turnHeaterOff();

};
#endif
