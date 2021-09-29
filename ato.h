#ifndef ato_h
#define ato_h
#include "Arduino.h"

////////////////////////////////////////////////////////////
///////////////////TEST THIS ALL!!!!!!//////////////////////
////////////////////////////////////////////////////////////

const int DEBOUNCE_BUFFER = 3;
// Data structure for random access to stored eeprom values
struct ATOData {
  // Delays are all milliseconds
  unsigned long LevelCheckDelay = 10000;
  unsigned long LevelCheckDelayWhileFilling = 5000;

  unsigned long AlarmCheckDelay = 10000;
  unsigned long AlarmCheckDelayWhileFilling = 1000;

  unsigned long WaterValveDelay = 2UL * 1000;

  int OptoThreshold = 750;  // mV that indicate sensor triggered
};


class AutoTopOff{

  public:
    AutoTopOff(byte opticalSwitchPin, byte alarmSwitchPin, byte waterValvePin);
    void init();
    void alarmLoop();   // Main Loop
    void valveLoop();
    
    bool needsFilled();   // Check fluid level, returns true when below fill level
    bool isAlarmActive();   // Checks for alarm condiion, if alram return true
    void resetAlarm();
    bool isFilling();

    // Sensor Monitoring
    bool checkLevelSensor();
    bool checkForAlarm();

    // Valve control
    void enableATOValve();
    void disableATOValve();

    // Setters
    void setOpticalCheckDelay(unsigned long lvlCheckDelay);
    void setOpticalCheckFillingDelay(unsigned long lvlCheckDelay);
    void setAlarmCheckDelay(unsigned long alarmCheckDelay);
    void setAlarmCheckFillingDelay(unsigned long alarmCheckDelay);
    void setValveCycleDelay(unsigned long valveCycleDelay);
    void setOptoThreshold(int threshold);

    // Getters
    unsigned long getOpticalCheckDelay();
    unsigned long getOpticalCheckFillingDelay();
    unsigned long getAlarmCheckDelay();
    unsigned long getAlarmCheckFillingDelay();
    unsigned long getValveCycleDelay();
    int getOptoThreshold();
  // Get valve position function

  private:
    ATOData _data;    // User editable values

    /////// constants declared in constructor ////////
    const byte _opticalSwitchPin;
    const byte _alarmSwitchPin;
    const byte _waterValvePin;
    const byte _bufferSize;
    
    //Buffers
    bool _alarmBuffer[DEBOUNCE_BUFFER]; // alarm state buffer
    int _levelBuffer[DEBOUNCE_BUFFER];  // level state buffer

    // Indices for buffers
    byte _levelBufferIndex = 0;   // Current index for opto level sensor buffer
    byte _alarmBufferIndex = 0;   // current index for alarm switch buffer

    // Class use variables
    bool _isValveOpen = false;  // Valve state
    bool _overfillAlarm = false;  // Alarm state
    unsigned long _prevLevelTime = 0;   // Last time the level was checked with opto
    unsigned long _prevAlarmCheckTime = 0;   // Last time we checked for an alarm
    unsigned long _prevValveTime = 0;   // Last time the valve was cycled
  
};
#endif
