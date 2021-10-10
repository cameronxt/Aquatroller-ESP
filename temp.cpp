#include "temp.h"

Temp::Temp(DallasTemperature* tempSensors) {
  _temp = tempSensors;    // Pointer to our temp sensors
}

void Temp::init() {
  ESP_LOGV("Temp", "Initializing Temperature Controls... ");
  _temp->begin();                           // Initialize comms for sensors
  _temp->setWaitForConversion("FALSE");     // We dont want to wait, it blocks us, so we will time it ourselves
  pinMode(HEATER_PIN, OUTPUT);               // Setup heater pin as output
  digitalWrite(HEATER_PIN, LOW);            // Turn it off
  ESP_LOGV("Temp", "Temperature Controls Ready");
}

void Temp::loop() {
  updateTemps();
  getCurrentTemp();
                                                  
  vTaskDelay(pdMS_TO_TICKS(getTempDelay()));  // Pause task for temp delay
}

void Temp::heaterLoop() {
  if ((_temps[0] <= getTargetTemp()- 0.5) && (_temps[1] <= getTargetTemp() - 0.5)) {       // if temp is low
    turnHeaterOn();                                                                             // then turn it on
  } else if ((_temps[0] > getTargetTemp()) || (_temps[1] > getTargetTemp())) {            // if temp is too high
    turnHeaterOff();                                                                           // Then Turn it off
  }
  vTaskDelay(pdMS_TO_TICKS(getHeaterDelay()));
}

 // returns the most recent average of all active senors
float Temp::getCurrentTemp() { 
  float averageTemp = 0;  // accumulates temps for average
  int activeSensorCount = 0;    // Number of sensors that are functioning with no errors
  static bool prevError[NUM_OF_SENSORS];  // array to hold previous error flags so we only print one error
  for(int index = 0; index < NUM_OF_SENSORS; index++ ) {    // Cycle through all sensors
    if (_tempError[index]) {  // if this sensor has an error flag
      if (prevError[index] != _tempError[index]) {  // if this is the first time for this flag
        ESP_LOGD("Temp", "Sensor %i Error. Ignoring Value", index);
        prevError[index] = _tempError[index]; // update previous error so it doesnt print again till reset
      }
    } else {
      averageTemp += _temps[index];
      activeSensorCount++;
      ESP_LOGD("Temp", "Sensor %i Connected. Using Value", index);
    }
  }

  static int prevSensorCount = 0;
  if (activeSensorCount > 0) {
    averageTemp /= activeSensorCount;
    ESP_LOGI("Temp", "Current Temperature: %fC", averageTemp);
    return ( averageTemp ); 
  } else {
    if (prevSensorCount != activeSensorCount) {
      ESP_LOGW("Temp", "Current Temperature unavailble");
      ESP_LOGW("Temp", "Please Connect a sensor...");
      prevSensorCount = activeSensorCount;
    }
    return 0;
  }
}

float Temp::getCurrentTempBySensor() {
  
}

void Temp::updateTemps() {
  static bool prevTempError[NUM_OF_SENSORS];
  
  ESP_LOGV("Temp", "Requesting Temperature Conversion...");
  _temp->requestTemperatures();   //  Start temp conversion
  vTaskDelay(pdMS_TO_TICKS(_tempData.conversionDelayTime));   // Delay for the sesors conversion time

  for (int sensor = 0; sensor < NUM_OF_SENSORS; sensor++) {   // Cycle through all available sensors
    _temps[sensor] = _temp->getTempFByIndex(sensor);    // Read the sensosr on the Wire bus
    if (_temps[sensor] <= TEMP_ERROR_DISCONNECTED) {   // Check for errors
      _tempError[sensor] = true;
      if(prevTempError[sensor] != _tempError[sensor]) {
        ESP_LOGE("Temp", "Temp Sensor %i: ERROR #196: Disconnected", sensor);
        prevTempError[sensor] = _tempError[sensor];
      }
    } else {
      _tempError[sensor] = false;
      ESP_LOGI("Temp", "Current Temp Ch %u: %f", sensor, _temps[sensor]);
    }
  }
}

void Temp::turnHeaterOn() {
  if (!isHeaterOn()) {                                  // Only Turn it on if its off
    ESP_LOGI("Temp", "Turning heater ON");
    digitalWrite(HEATER_PIN, HIGH);                     // Turn on heater
    _isHeaterOn = true;
  }
}

void Temp::turnHeaterOff() {
  if (isHeaterOn()) {                                   // Only turn it off if its on
    ESP_LOGI("Temp", "Turning heater OFF");
    digitalWrite(HEATER_PIN, LOW);                    // Turn off heater
    _isHeaterOn = false;
  }
}

// Getter for current heater change delay (heaterDelayTime)
unsigned int Temp::getHeaterDelay() {
  return _tempData.heaterDelayTime;
}

// Setter for current heater change delay (heaterDelayTime)
void Temp::setHeaterDelay(unsigned int newDelay) {
  _tempData.heaterDelayTime = newDelay;
}

// Getter for current temp check delay (tempDelayTime)
unsigned int Temp::getTempDelay() {
  return _tempData.tempDelayTime;
}

// Setter for current temp delay (tempDelayTime)
void Temp::setTempDelay(unsigned int newDelay) {
  _tempData.tempDelayTime = newDelay;
}

// Getter for current target temperature (targetTemp)
float Temp::getTargetTemp() {
  return _tempData.targetTemp;
}

// Setter for current target temperature (targetTemp)
void Temp::setTargetTemp(float newTemp) {
  _tempData.targetTemp = newTemp;
}

bool Temp::isHeaterOn() {
  ESP_LOGV("Temp", "Checking heater state...");
  if ( _isHeaterOn ) {
    ESP_LOGV("Temp", "Heater is: ON");
  } else {
    ESP_LOGV("Temp", "Heater is: OFF");
  }
  return _isHeaterOn;
}
