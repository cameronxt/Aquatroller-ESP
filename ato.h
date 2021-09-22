#ifndef ato_h
#define ato_h
#include "Arduino.h"

////////////////////////////////////////////////////////////
///////////////////TEST THIS!!!!!!!!!!//////////////////////
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
  bool isFilling() { return _isValveOpen;};

  // Check Sensors
  bool checkLevelSensor();
  bool checkForAlarm();

  // Turns ATO on/off respectively
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
  ATOData _data;
  const byte _opticalSwitchPin;
  const byte _alarmSwitchPin;
  const byte _waterValvePin;
  const byte _bufferSize;
  
  bool _isValveOpen = false;
  bool _overfillAlarm = false;
  unsigned long _prevLevelTime = 0;
  unsigned long _prevAlarmCheckTime = 0;
  unsigned long _prevValveTime = 0;
  unsigned long _currentTime = 0;
  int _levelBuffer[DEBOUNCE_BUFFER] = {0,0,0}; 
  bool _alarmBuffer[DEBOUNCE_BUFFER] = {false,false,false};
  byte _levelBufferIndex = 0;
  byte _alarmBufferIndex = 0;
  
};
#endif
