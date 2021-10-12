#include "ph.h"

// PH Constructor
PH::PH(byte phInputPin, byte c02OutputPin)
  : _phPin(phInputPin),     // PH Sensor Analog pin
    _c02Pin(c02OutputPin)  // C02 relay pin
     {
  // _phData.targetPh = 7.0;
}

// Initialization function
void PH::init() {
  // Serial.print(F("Initializing PH..."));
  pinMode(_c02Pin, OUTPUT);     // Setup heater pin as output
  digitalWrite(_c02Pin, HIGH);  // Turn it off
  // Serial.println(F("Done"));
}

/* Loop function, place in your main loop() of your sketch.
   This is non blocking, using timers and flags. All it needs from
   your main sketch is the current time from millis() to run the
   timer. This loop will check the ph every so often (configurable)
   and add it to a buffer (size configurable). Once the buffer is
   full we will process the data in the buffer. We sort all the
   values from lowest to highest, then drop the highest and lowest
   values. Then we average the remaining values to come to our
   final output PH. Based on this PH, we decide if we need to turn
   on c02 or turn it off.
*/
// C02 Control && Getting resting PH
void PH::phLoop() {
  ESP_LOGV("PH", "Time to check PH");
  if (!_calibrationMode) {  // Dont read ph during calibration so probe stabilize without interrupt
    // Serial.println(F("PH Monitor Mode"));
   readPhRawToBuffer();  // read raw data and store it to an array
   processPhBuffer();    // Average and calculate new PH value

    if (_currentPh != _previousPh) {
      _previousPh = _currentPh;
      ESP_LOGI("PH", "Current PH: %f", _currentPh);
    }
  } else {
    ESP_LOGI("PH", "Calibration Mode");
    calibratePh();
  }

  ESP_LOGV("PH", "PH Check delay: %u", _phData.checkPhDelay);  
  // Timer so we only read ph every so often
  vTaskDelay(_phData.checkPhDelay / portTICK_PERIOD_MS);
}

void PH::c02Loop() {
  //static bool prevC02Position = false;
  //if ((ssm > _phData.c02OnTime) && (ssm < _phData.c02OffTime)) {   // Is it time for the c02 to be on
  // Serial.println(F("C02 Time"));
  ESP_LOGV("C02", "Time to check C02");
  if (_newPh) {
    //Serial.println("Checking C02 Levels");
    if (_currentPh > _phData.targetPh) {  // PH is higher than target
      //turnOnC02();                        // turn on c02
      if (_prevC02Position != isC02On()) {
        ESP_LOGI("C02", "Turning C02: ON");
        _prevC02Position = isC02On();
      }
    } else {
     // turnOffC02();  // otherwise turn it off
      if (_prevC02Position != isC02On()) {
        ESP_LOGI("C02", "Turning C02: OFF");
        _prevC02Position = isC02On();
      }
    }
    _newPh = false;
  }
  ESP_LOGV("C02", "C02 Check Delay: %u", _phData.checkC02Delay);
  vTaskDelay(_phData.checkC02Delay /portTICK_PERIOD_MS);
}

// Reads the raw sensor value and stores it in the buffer, then advance buffer for next reading
void PH::readPhRawToBuffer() {
  //vTaskDelay(10);
  ESP_LOGV("PH", "Reading Raw Value for PH");
  if (_phIndex < _bufSize) {              // Bounds checking
    _buf[_phIndex] = analogRead(_phPin);  // store anolg value into buffer position
    _phIndex++;                           // Advance buffer to next position
  } else {
    _bufferFull = true;
    _phIndex = 0;
  }
  
}

// Called when buffer is full.
void PH::processPhBuffer() {
  if ( _bufferFull ) {
    int temp = 0;
    ESP_LOGD("PH", "Processing PH Buffer");
    for (int i = 0; i < _bufSize; i++) {  // First lets sort our results
      for (int j = i + 2; j < _bufSize - 1; j++) {
        if (_buf[i] > _buf[j]) {
          temp = _buf[i];
          _buf[i] = _buf[j];
          _buf[j] = temp;
        }
      }
    }
    _avgValue = 0;                                        // Reset average value
    for (int i = _dropMe - 1; i < _bufSize - _dropMe; i++) {  // Drop highest and lowest value(s), add the rest together
      _avgValue += _buf[i];
    }
    // vTaskDelay(10);
    float phVol = (((float)_avgValue / (_bufSize - (_dropMe * 2))) * ((float)3.3 / (float)4096));  // average and convert to milli-volts
    ESP_LOGD("PH", "PH Voltage: %fV", phVol);
    _currentPh = (-5.70 * (float)phVol) + (21.34 + (float)_phData.phCalValue);     // convert millivolts to PH reading
    _newPh = true;
    _bufferFull = false;
  }
}


// TODO: RTOSify this function
// Enter probe calibration mode...
// Give this method the curent time in miilis and a float array with the 2 buffer solutions target values.
// Ph monitoring will be disabled during calibration to allow the probe to get the most accurate reading possible.
// After waiting for the probe to stabilize, a reading is taken and stored, then we do both again for the second solution.
// Once we have both values we convert them to a PH value and then store both sets of targets and actuals.

void PH::calibratePh() {

  static bool haveFirstPoint = false;  // flag so we know if we are on first or second cal point

  if (_calibrationMode) {

    Serial.println(F("PH Calibration Mode..."));

    if (millis() - _prevPhTime >= _phData.phStabilizeDelay) {  // Timer to wait for stabilization period before reading ph
      int phVol = 0;
      if (!haveFirstPoint) {  // Do we need the first actual cal point

        Serial.println(F("Reading Actual 1"));
        _phIndex = 0;

        readPhRawToBuffer();  // Read raw ph value and add to buffer

        phVol = (float)_buf[0] * 5.0 / 1024;              // Convert to mv and store
        setCalActual(0, ((float)-5.70 * phVol + 21.34));  // convert millivolts to PH reading without calibration

        _prevPhTime == millis();  // reset PH timer
        haveFirstPoint = true;    // Flag the first point as done

      } else {

        Serial.println(F("Reading Actual 2"));
        readPhRawToBuffer();                              // Reset buffer index
        phVol = (float)_buf[1] * 5.0 / 1024;              // Convert to mv and store
        setCalActual(1, ((float)-5.70 * phVol + 21.34));  // convert millivolts to PH reading without calibration

        _prevPhTime = millis();  // reset PH timer
        _phIndex = 0;            // Reset index so we can start getting ph readings again

        calculateCalibration();  // Calculate Calibration Offset

        haveFirstPoint = false;    // Reset flag so we grab first point next time
        _calibrationMode = false;  // Reset flag to exit Calibration Mode
      }
    }
  }
}


// Set resting PH as the current PH, only call once you know the C02 has offgassed, a
// couple hours after c02 off should be plenty but i havent done any real life testing yet
void PH::calculateRestingPh() {
  // Get the resting PH to know our drop in PH with c02 and reset our flag
  _phData.restingPh = _currentPh;
  _needRestingPh = false;
}

// TODO: Testing to decide which of the following methods is more reliable

// Calculate the c02 in PPM that is in the water based on current PH and KH hardness
int PH::calculateC02PPM() {
  // c02 PPM = 3*KH*10^(7-ph)
  return _currentC02PPM = 3 * _phData.khHardness * pow(10, (7 - _currentPh));
}

// Calculate what PH should be to achieve desired PPM of c02
void PH::calculateTargetPh() {
  // pH = 6.35 + log(12.839 * dKH / cO2Target)
  _phData.targetPhC02 = 6.35 + log(12.839 * _phData.khHardness / _phData.targetPPMC02);
}

void PH::turnOnC02() {
  if (!_co2On) {
    digitalWrite(_c02Pin, LOW);
    _c02On = true;
  }
}

void PH::turnOffC02() {
  if (_c02On) {
    digitalWrite(_c02Pin, HIGH);
    _c02On = false;
  }
}
