#include "ato.h"

AutoTopOff::AutoTopOff (byte opticalSwitchPin, byte alarmSwitchPin, byte waterValvePin)
  : _opticalSwitchPin(opticalSwitchPin), _alarmSwitchPin(alarmSwitchPin), _waterValvePin(waterValvePin), _bufferSize (DEBOUNCE_BUFFER)  // assign control pins
{
}

void AutoTopOff::init() {  // Setup control pins
  pinMode(_opticalSwitchPin, INPUT);
  pinMode(_alarmSwitchPin, INPUT);
  pinMode(_waterValvePin, OUTPUT);
}

void AutoTopOff::alarmLoop() {
  unsigned long delay;
  if ( isFilling() ) {
    delay = _data.AlarmCheckDelayWhileFilling;  // timer for delay
  } else {
    delay = _data.AlarmCheckDelay;
  }
  if (checkForAlarm()) {                                                       // if alarm switch has been triggered
    disableATOValve();                                                         // turn of the valve so it doesnt overflow
  } else {                                                                     // if alarm is no longer triggered we can reset the alarm
    resetAlarm();
  }
  vTaskDelay(delay / portTICK_PERIOD_MS);
}


void AutoTopOff::valveLoop() {
  unsigned long delayValve = 0;
  if ( isFilling() ) {                                    // if valve is open
    delayValve =_data.LevelCheckDelayWhileFilling;        // timer for delay
    if (checkLevelSensor()) {                             // if true, water level is full
      disableATOValve();                                  //So lets turn it off
    }
  } else {                                                // otherwise valve is closed
    delayValve = _data.LevelCheckDelay;                   // timer for delay
    enableATOValve();                                     // lets fill it up
  }
  vTaskDelay(delayValve / portTICK_PERIOD_MS);            // time to delay in ms
}


bool AutoTopOff::isAlarmActive() {
  return _overfillAlarm;
}


void AutoTopOff::resetAlarm() {
  if (isAlarmActive()) {  // Only reset if its been set
    _overfillAlarm = false;
  }
}

// true is full and false is low
bool AutoTopOff::checkLevelSensor() {
  unsigned long delayLevelSensor;
  for (byte i; i < sizeof(_levelBufferIndex); i++) {
    _levelBuffer[i] = analogRead(_opticalSwitchPin);
    if (isFilling()) {
      delayLevelSensor = getOpticalCheckFillingDelay();
    } else {
      delayLevelSensor = getOpticalCheckDelay();
    }
    vTaskDelay(delayLevelSensor / portTICK_PERIOD_MS);
  }

  int sum = 0;
  for (int index = 0; index < (sizeof(_levelBuffer)); index++) {  // add buffer readings together
    sum += _levelBuffer[index];
  }
  if ((sum / sizeof(_levelBuffer)) > _data.OptoThreshold) {  // if the average of the buffer readings above threshold
    return true;                                             // above threshold = full
  } else {
    return false;  // below threshold = needs filled
  }
  vTaskDelay(1);
}

bool AutoTopOff::checkForAlarm() {
  bool alarm = digitalRead(_alarmSwitchPin);
  _prevLevelTime = _currentTime;                         // Check alarm float switch
  _prevAlarmCheckTime = _currentTime;                    // Update time last checked
  _alarmBuffer[_alarmBufferIndex] = alarm;               // Store reading into buffer
  if (_alarmBufferIndex < (sizeof(_alarmBuffer) - 1)) {  // increase index buffer if there is room
    _alarmBufferIndex++;
  } else {  // otherwise reset it
    _alarmBufferIndex = 0;
  }
  int sum = 0;
  for (int index = 0; index < (sizeof(_alarmBuffer) - 1); index++) {  // Count all positions of alarm switch
    if (_alarmBuffer[index]) {
      sum++;
    }
  }
  if ((sum >= sizeof(_alarmBuffer))) {  // if the buffer is full of alarms
    _overfillAlarm = true;              // set overfill alarm
    return true;                        // true = alarm
  }
  return false;  // false = normal
}


void AutoTopOff::enableATOValve() {
  if (!_isValveOpen) {                                            // If valve is not open
    if (_currentTime - _prevValveTime > _data.WaterValveDelay) {  // valve cycle timer
      digitalWrite(_waterValvePin, HIGH);                         // Set valve open
      _isValveOpen = true;  // set flag
      _data.WaterValveDelay = _currentTime;  // Set last valve cycle time
    }
  }
}

void AutoTopOff::disableATOValve() {
  if (_isValveOpen) {                   // if valve open
    digitalWrite(_waterValvePin, LOW);  // Set valve closed
    _isValveOpen = false; // reset flag
    _data.WaterValveDelay = _currentTime;  // Set last valve cycle time
  }
}


/////////////////////////////////////////////////////////////////////
////////////////////////Setters and getters//////////////////////////
/////////////////////////////////////////////////////////////////////
void AutoTopOff::setOpticalCheckDelay(unsigned long lvlCheckDelay) {
  _data.LevelCheckDelay = lvlCheckDelay;
}


void AutoTopOff::setOpticalCheckFillingDelay(unsigned long lvlCheckDelay) {
  _data.LevelCheckDelayWhileFilling = lvlCheckDelay;
}


void AutoTopOff::setAlarmCheckDelay(unsigned long alarmCheckDelay) {
  _data.AlarmCheckDelay = alarmCheckDelay;
}


void AutoTopOff::setAlarmCheckFillingDelay(unsigned long alarmCheckDelay) {
  _data.AlarmCheckDelayWhileFilling = alarmCheckDelay;
}


void AutoTopOff::setValveCycleDelay(unsigned long valveCycleDelay) {
  _data.AlarmCheckDelayWhileFilling = valveCycleDelay;
}


void AutoTopOff::setOptoThreshold(int threshold) {
  _data.OptoThreshold = threshold;
}

unsigned long AutoTopOff::getOpticalCheckDelay() {
  return _data.LevelCheckDelay;
}


unsigned long AutoTopOff::getOpticalCheckFillingDelay() {
  return _data.LevelCheckDelayWhileFilling;
}


unsigned long AutoTopOff::getAlarmCheckDelay() {
  return _data.AlarmCheckDelay;
}


unsigned long AutoTopOff::getAlarmCheckFillingDelay() {
  return _data.AlarmCheckDelayWhileFilling;
}


int AutoTopOff::getOptoThreshold() {
  return _data.OptoThreshold;
}


unsigned long AutoTopOff::getValveCycleDelay() {
  return _data.WaterValveDelay;
  }
/////////////////////////////////////////////////////////////////////
///////////////////////End Seters and getters////////////////////////
/////////////////////////////////////////////////////////////////////