#include "ato.h"

// Set all pin constants
AutoTopOff::AutoTopOff (byte opticalSwitchPin, byte alarmSwitchPin, byte waterValvePin)
  : _opticalSwitchPin(opticalSwitchPin), _alarmSwitchPin(alarmSwitchPin), _waterValvePin(waterValvePin), _bufferSize (DEBOUNCE_BUFFER)  // assign control pins
{
}

void AutoTopOff::init() {  // Setup control pins
  // set correct pinModes
  pinMode(_opticalSwitchPin, INPUT);
  pinMode(_alarmSwitchPin, INPUT);
  pinMode(_waterValvePin, OUTPUT);

  digitalWrite(_waterValvePin, LOW);  // Set water valve to off on startup
}


// Check mechanical float sensor for an alrarm (overfull) condition
void AutoTopOff::alarmLoop() {
  ESP_LOGV("ATO", "Checking for alarms");
  unsigned long delay;    //required delay in millis

  if (checkForAlarm()) {   // if alarm switch has been triggered
    disableATOValve();  // turn of the valve so it doesnt overflow
  } else {  // otherwise we can reset the alarm
    resetAlarm();
  }
  
  if ( isFilling() ) {    // Set appropriate delay based on whether filling
    delay = _data.AlarmCheckDelayWhileFilling;  
  } else {
    delay = _data.AlarmCheckDelay;
  }
  ESP_LOGD("ATO", "Alarm check delay is %i", delay);
  vTaskDelay(delay / portTICK_PERIOD_MS);
}


void AutoTopOff::valveLoop() {
  ESP_LOGD("ATO", "Checking Level via Optical sensor");
  unsigned long delayValve = 0;
  if ( isFilling() ) {   // if valve is open
    delayValve =_data.LevelCheckDelayWhileFilling; // timer for delay
    if (checkLevelSensor()) {  // if true, water level is full
      disableATOValve();  //So lets turn it off
    }
  } else {  // otherwise valve is closed
    delayValve = _data.LevelCheckDelay;  // timer for delay
    enableATOValve();   // lets fill it up
  }
  vTaskDelay(delayValve / portTICK_PERIOD_MS);  // time to delay in ms
}

// Returns true if there is an active alarm
bool AutoTopOff::isAlarmActive() {
  if (_overfillAlarm) {
  ESP_LOGV("ATO", "Valve: ATO Filling");
  } else { 
    ESP_LOGV("ATO", "Valve: idle");
  }
  return _overfillAlarm;
}

// Resets alarm if it was triggered
void AutoTopOff::resetAlarm() {
  if (isAlarmActive()) { 
    ESP_LOGD("ATO", "Resetting Alarm");
    _overfillAlarm = false;
  }
}


bool AutoTopOff::isFilling() { 
  if ( _isValveOpen ) {
    ESP_LOGD("ATO", "Valve is open");
  } else {
    ESP_LOGD("ATO", "Valve is closed");
  }
  return _isValveOpen;
}

// Check optical level sensor and then returns
//  true when full and false when low
bool AutoTopOff::checkLevelSensor() {
  unsigned long delayLevelSensor;
  int sum = 0;
  ESP_LOGV("ATO", "Checking optical level sensor");
  for (byte i = 0; i < DEBOUNCE_BUFFER; i++) {  // cycle through buffer
    _levelBuffer[i] = analogRead(_opticalSwitchPin);  // store optical switch value into buffer
    sum += _levelBuffer[i];
    ESP_LOGD("ATO", "Adding %imv to buffer in position %i", _levelBuffer[i], i);
    vTaskDelay(50);
  }
  if (isFilling()) {    // Set required delay depending on if we are filling or not
    delayLevelSensor = getOpticalCheckFillingDelay();
  } else {
    delayLevelSensor = getOpticalCheckDelay();
  }
  ESP_LOGD("ATO", "Total Sum: %i", sum);
  ESP_LOGD("ATO", "Reported Value: %i", sum/sizeof(_levelBuffer));

  if ((sum / DEBOUNCE_BUFFER) > _data.OptoThreshold) {  // if the average of the buffer readings above threshold
    ESP_LOGD("ATO", "Tank is full");
    return true;   // above threshold = full
  } else {
    ESP_LOGD("ATO", "Tank needs filled");
    return false;  // below threshold = needs filled
  }
  // vTaskDelay(1);
}

// Check for an alarm condition returns true when
//  all buffer poitions return an alarm
bool AutoTopOff::checkForAlarm() {
  bool alarm = digitalRead(_alarmSwitchPin);
  _alarmBuffer[_alarmBufferIndex] = alarm;   // Store reading into buffer
  int sum = 0;  // for storing the sum of al the buffer positions to average
  if (_alarmBufferIndex < sizeof(_alarmBuffer) -1 ) {  // increase index buffer if there is room
    _alarmBufferIndex++;
  } else {  // otherwise reset it
    _alarmBufferIndex = 0;
  }
  for (int index = 0; index < sizeof(_alarmBuffer); index++) {  // Sum all positions of alarm switch from buffer
    if (_alarmBuffer[index]) {
      sum++;
    }
  }
  if ((sum >= sizeof(_alarmBuffer))) {  // if the buffer is full of alarms
    _overfillAlarm = true;  // set overfill alarm
    return true;   // true = alarm
  }
  _overfillAlarm = false;
  return false;  // false = no alarm
}

// Turns on ATO fill valve if its off and has been longer than required delay
void AutoTopOff::enableATOValve() {
  if (!_isValveOpen) {   // If valve is not open
    if (millis() - _prevValveTime > _data.WaterValveDelay) {  // valve cycle timer
    ESP_LOGI("ATO", "Opening Valve");
      digitalWrite(_waterValvePin, HIGH);  // Set valve open
      _isValveOpen = true;  // set flag
      _prevValveTime = millis();  // Set last valve cycle time
    }
  }
}

void AutoTopOff::disableATOValve() {
  if (_isValveOpen) {  // if valve open
    if (millis() - _prevValveTime > _data.WaterValveDelay) {  // valve cycle timer
    ESP_LOGI("ATO", "Closing Valve");
      digitalWrite(_waterValvePin, LOW);  // Set valve closed
      _isValveOpen = false; // reset flag
      _prevValveTime = millis();  // Set last valve cycle time
    }
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