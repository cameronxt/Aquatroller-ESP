#include "lights.h"

Light::Light(Adafruit_PWMServoDriver *pwm, DS3232RTC *rtc) {
  _pwm = pwm;
  _rtc = rtc;
  _data.normalFadeDelay = 5;
  _data.testFadeDelay = 5;
  _targetBright = _normalMaxBright;
  for (byte chIndex; chIndex < _numChannels; chIndex++) {
    _data.ch[chIndex].prevFadeTime = 0;
  }
}

void Light::init() {
  Wire.setClock(400000);
  _pwm->begin();
  // _pwm.setOutputMode(false);           // Set PWM Driver to open Drain for LEDs
  _pwm->setPWMFreq(1000);                // Set PWM Hz to reduce buzzing sound and Meanwell LLD's have a max of 1kHz
  for (int chIndex = 0; chIndex < _numChannels; chIndex++) {
    _pwm->setPin(chIndex, 0, 0); // turn all channels off
  }

  // Initialize class variables
  _currentTime = now();
  _prevDay = day();   // Set daily timer

  _onTime = intTimesToTime_t(_data.onTimeHours, _data.onTimeMinutes); // Convert on time from to time_t from 2 bytes
  _offTime =  intTimesToTime_t(_data.offTimeHours, _data.offTimeMinutes);  // Convert off time from to time_t from 2 bytes

  calcStartStep(); // Find start step ??? TODO: Do chIndex need this here?
  calcFadeMap();  // calculate the fade map

  if ( isLightTime() ) {
    Serial.println();
    // Serial.println(F("Light Should be on..."));
    // Serial.println(F("Setting fadeDelay to bring light to proper brightness"));
    // TODO: Maybe calcStartStep should be here
    // Serial.println();

    for (byte chIndex = 0; chIndex < _numChannels; chIndex++) {

       unsigned long timeDiff = 1000ul * abs( (  _currentTime - intTimesToTime_t(_data.ch[chIndex].stepTimeHours[_data.ch[chIndex].fadeIndex], _data.ch[chIndex].stepTimeMinutes[_data.ch[chIndex].fadeIndex])));  // Calculate time difference in millis
       unsigned long brightDiff = abs(_data.ch[chIndex].brightness[_data.ch[chIndex].fadeIndex] - _data.ch[chIndex].currentBright); // calculate total amount of change in brightness

      _data.ch[chIndex].fadeDelay[_data.ch[chIndex].fadeIndex] = timeDiff / brightDiff;

      // Serial.print(F("Ch #"));
      // Serial.print(chIndex);
      // Serial.print(F(" set to "));
      // Serial.println(_data.ch[chIndex].fadeDelay[_data.ch[chIndex].fadeIndex]);
      // Serial.println();

    }
  }


}

// Main loop function.
// Calls light control based on time and _data.mode
void Light::loop(time_t currentTime) {

  _currentTime = currentTime;  // Update time in Library

  if (isLightTime()) {
    switch (_data.mode) {
      case NORMAL:
        normalLights();
        break;
      case SUNLIGHT:
        break;
      case CUSTOM:
        customLights( millis() );
        //Serial.println(F("Custom Lights"));
        break;
      case TEST:
        testLights();
        break;
    }
  } else {  // Run the moonlights during off times
    
    moonLights();

    // Reset fadeMap at midnight
    if (day() != _prevDay)  { // Once a day timer
      //Serial.println(F("Performing daily map reset"));
      for(byte ch = 0; ch < _numChannels; ch++) {
        _data.ch[ch].stepCount = 0;
        _data.ch[ch].fadeIndex = 0;
      }
      calcFadeMap();
      _prevDay = day(); // Reset timer for next run
    }
  }
  vTaskDelay(1); // delay number of ticks
}
void Light::moonLights () {
  //Serial.println(F("Its moonlight time"));
    for (int chIndex = 0; chIndex < _numChannels; chIndex++) {  // Cycle through all channels for moonlights
      if (millis() - _data.ch[chIndex].prevFadeTime > _data.ch[chIndex].moonlightFadeDelay ) {  // Only update after minimum fade time per channel
        bool valChanged = false;
        if (_data.ch[chIndex].currentBright < _data.ch[chIndex].moonlightBright) {
          _data.ch[chIndex].currentBright++;
          valChanged = true;
          _pwm->setPin(chIndex, _data.ch[chIndex].currentBright, 0);

          
        } else if (_data.ch[chIndex].currentBright > _data.ch[chIndex].moonlightBright) { 
          _data.ch[chIndex].currentBright--;
          valChanged = true;
          _pwm->setPin(chIndex, _data.ch[chIndex].currentBright, 0);
        }
        _data.ch[chIndex].prevFadeTime = _currentTime;
        if (valChanged) {
          // Serial.print(F("Moonligthts Ch#"));
          // Serial.print(chIndex);
          // Serial.print(F(" set brightness to: "));
          // Serial.println(_data.ch[chIndex].currentBright);
          valChanged = false;
        }
      }
    }
}
// Turns 2 ints (one for hours one for minutes) into a time_t of today
// Need to remove in favor of byte storage
time_t Light::intTimesToTime_t (int hoursNow, int minutesNow) {
  return ((hoursNow * SECS_PER_HOUR) + (minutesNow * SECS_PER_MIN) + (previousMidnight(_currentTime)));
}

// Turns 2 byte (one for hours one for minutes) into a time_t of today
time_t Light::byteTimesToTime_t (byte hoursNow, byte minutesNow) {
  return ((hoursNow * SECS_PER_HOUR) + (minutesNow * SECS_PER_MIN) + (previousMidnight(_currentTime)));
}

// TODO: Make work even if time passes midnight
bool Light::isLightTime() {
  time_t onTime = byteTimesToTime_t(_data.onTimeHours, _data.onTimeMinutes);
  time_t offTime = byteTimesToTime_t(_data.offTimeHours, _data.offTimeMinutes);
  //    Serial.print ("Current Time: ");
  //    Serial.print (hour(_currentTime));
  //    Serial.print (F(":"));
  //    Serial.println(minute(_currentTime));
  //    Serial.print ("On Time: ");
  //    Serial.print (hour(onTime));
  //    Serial.print(F(":"));
  //    Serial.println(minute(onTime));
  //    Serial.print ("Off Time: ");
  //    Serial.print (hour(offTime));
  //    Serial.print(F(":"));
  //    Serial.println(minute(offTime));
  if (onTime > offTime) {
    //     Serial.println(F("On Time is MORE than Off Time"));
    time_t modOffTime = offTime + SECS_PER_DAY;
    if ((_currentTime < offTime) || (_currentTime >= onTime)) {
      //Serial.println(F("Light should be: ON"));
      return true;
    } else {
      //Serial.println(F("Light should be: OFF"));
      return false;
    }
  } else {
    //Serial.println(F("On Time is LESS than Off Time"));
    if ((_currentTime >= onTime) && (_currentTime < offTime)) {
      //Serial.println(F("Light should be: ON"));
      return true;
    } else {
      //Serial.println(F("Light should be: OFF"));
      return false;
    }
  }
  //    Serial.println(_currentTime);
  //    Serial.println(onTime);
  //    Serial.println(offTime);
}

// Light mode = Normal - Main loop method
void Light::normalLights() {
  static unsigned long prevMillis;

  if (_currentTime - prevMillis > _data.normalFadeDelay) {     // Wait for delay
    //Serial.println("Light Delay Exceeded");
    if (isLightTime()) {                           // if light is supposed to be on
      _targetBright = _normalMaxBright; // Automatically goes to max -
      if ((_targetBright > _normalCurrentBright) && (_normalCurrentBright < _normalMaxBright)) {             // and target bright is brighter than current

        _normalCurrentBright++;                                 // make one step brighter
        for (int chIndex = 0; chIndex < _numChannels; chIndex++) {                           // set all cahnnels to new value
          _pwm->setPin(chIndex, _normalCurrentBright, 0);
        }
      }
    } else {  // If light is supposed to be off
      _targetBright = 0; // Change Our target
      if ((_targetBright < _normalCurrentBright) && (_normalCurrentBright > 0)) { // and target brightness is lower than current bright
        _normalCurrentBright--; // make one step dimmer
        for (int chIndex = 0; chIndex < _numChannels; chIndex++) {  // Set all channels to new value
          _pwm->setPin(chIndex, _normalCurrentBright, 0);
        }
      }
    }

    prevMillis += _data.normalFadeDelay;  // reset delay timer
  }
}

// Light mode = Custom - Main loop method
void Light::customLights (unsigned long currentMillis) {
  //Serial.println(F("Custom Light Loop"));
  //    Serial.println(F("Light Time: true"));

  // Cycle through the channels
  for (int chIndex = 0; chIndex < _numChannels; chIndex++ ) {

    byte fadeIndex = _data.ch[chIndex].fadeIndex;
    //
    //              Serial.print(F("currentMillis: "));
    //              Serial.print(currentMillis);
    //              Serial.print(F(" - prevFadeTime: "));
    //              Serial.print( _data.ch[chIndex].prevFadeTime );
    //              Serial.print(F(" - Fade Delay "));
    //              Serial.println( _data.ch[chIndex].fadeDelay[fadeIndex] );

    if (currentMillis - _data.ch[chIndex].prevFadeTime > _data.ch[chIndex].fadeDelay[fadeIndex]) {  // Channel timers
      // Serial.println();
      // Serial.print(F("Channel #"));
      // Serial.print(chIndex);
      // Serial.print(F(" - Index: "));
      // Serial.println(fadeIndex );
      // Serial.print ("Current Time: ");
      // Serial.print (hour(_currentTime));
      // Serial.print (F(":"));
      // Serial.println(minute(_currentTime));

      _data.ch[chIndex].prevFadeTime += _data.ch[chIndex].fadeDelay[fadeIndex]; // Reset timer

      if (_data.ch[chIndex].currentBright < _data.ch[chIndex].brightness[fadeIndex]) { // if brightness needs to increase
        _data.ch[chIndex].currentBright++;
        // Serial.print(F("Brightness increased to: "));
        // Serial.println(_data.ch[chIndex].currentBright);
      } else if (_data.ch[chIndex].currentBright > _data.ch[chIndex].brightness[fadeIndex]) {   // if brightness needs to decrease
        _data.ch[chIndex].currentBright--;
        // Serial.print(F("Brightness decreased to: "));
        // Serial.println(_data.ch[chIndex].currentBright);

        // otherwise check to see if its time to move to the next fade cycle
      } else if (_currentTime >= byteTimesToTime_t( _data.ch[chIndex].stepTimeHours[fadeIndex], _data.ch[chIndex].stepTimeMinutes[fadeIndex] )) {
        if ( _data.ch[chIndex].fadeIndex <= _mapSize ) {   // Advance and reset fade index
          // Serial.println(F("Incresing fadeIndex to "));
          _data.ch[chIndex].fadeIndex++;
          // Serial.println(_data.ch[chIndex].fadeIndex);
        } else if (_currentTime >= byteTimesToTime_t( _data.offTimeHours, _data.offTimeMinutes )) {
          _data.ch[chIndex].fadeIndex = 0;
        }
      }

      _pwm->setPin(chIndex, _data.ch[chIndex].currentBright, 0);  // Set brightness on PWM Driver

      // Serial.print(F("Number of steps: "));
      // Serial.println(_data.ch[chIndex].numberOfSteps[fadeIndex]);

      // Serial.print(F("Target Brightness: "));
      // Serial.println(_data.ch[chIndex].brightness[fadeIndex]);

      // Serial.print(F("fadeDelay: "));
      // Serial.println(_data.ch[chIndex].fadeDelay[fadeIndex]);

      // Serial.print(F("Channel "));
      // Serial.print( chIndex );

      // Serial.print(F(" Set to "));
      // Serial.println(_data.ch[chIndex].currentBright);
      // Serial.println();

    }
  }
}

// Determines length of delay between each fade step
// Depends on calcFadeStep() to have been run at least once to set
//   us to the correct indices for each channel
// TODO: TEST!!! Fade from off to current values when turned on during photo cycle
void Light::calcFadeMap() {

  // These are to test the speed of the function
  unsigned long startTime = micros();
  unsigned long endTime;


  // Debug output
  // Serial.println(F("Calculating Fade Map..."));
  // Serial.println();
  // Serial.print(F("Current Time: "));
  // Serial.println(_currentTime);
  // Serial.print(F("On Time: "));
  // Serial.println( byteTimesToTime_t(_data.onTimeHours, _data.onTimeMinutes) );
  // Serial.print(F("Off Time: "));
  // Serial.println( byteTimesToTime_t(_data.offTimeHours, _data.offTimeMinutes) );
  // Serial.println();

  for (byte chIndex = 0; chIndex < _numChannels ; chIndex++) {  // iterate through channels

    // Serial.println();
    // Serial.print(F("Channel #"));
    // Serial.println(chIndex);


    for (byte stepIndex = _data.ch[chIndex].fadeIndex; stepIndex <= _mapSize; stepIndex++) {  // iterate through step times

      // Serial.print(F("Map Step #"));
      // Serial.println(stepIndex);

      unsigned long timeDiff;  // Difference between times in millis
      unsigned int brightDiff; // Difference in brightness

      if ((stepIndex == _data.ch[chIndex].fadeIndex) && isLightTime()) {  // If we are in this step on resume

        //Serial.println(F("J >= fadeIndex && isLightTime() "));

        timeDiff = 1000ul * abs( (  _currentTime - intTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex])));  // Calculate time difference in millis
        brightDiff = abs(_data.ch[chIndex].brightness[stepIndex] - _data.ch[chIndex].currentBright); // calculate total amount of change in brightness

      }
      else if (stepIndex == 0) {     // If index is on its first pass

        //Serial.println(F("J == 0"));
        timeDiff = 1000ul * abs( (  _onTime - intTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex])));  // Calculate time difference in millis
        brightDiff = abs(_data.ch[chIndex].brightness[stepIndex] - _data.ch[chIndex].currentBright); // calculate total amount of change in brightness

        //        Serial.print(F("Next requested brightness: "));
        //        Serial.println(_data.ch[chIndex].brightness[stepIndex]);
        //        Serial.print(F("Previous requested brightness: "));
        //        Serial.println(_data.ch[chIndex].moonlightBright);
        //        Serial.print(F("Step Time: "));
        //        Serial.println(intTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex]));
        //        Serial.print(F("OnTime: "));
        //        Serial.println(_onTime);

      } else if (( stepIndex > 0 ) && (stepIndex < _mapSize  )) {   // If index is all but first or last
        //Serial.println(F("J > 0 && stepIndex < mapSize"));
        timeDiff = 1000ul * abs(( intTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex])  - intTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex - 1], _data.ch[chIndex].stepTimeMinutes[stepIndex - 1])));
        brightDiff = abs((_data.ch[chIndex].brightness[stepIndex]) - (_data.ch[chIndex].brightness[stepIndex - 1]));

        //        Serial.print(F("Current requested brightness: "));
        //        Serial.println(_data.ch[chIndex].brightness[stepIndex]);
        //        Serial.print(F("Previous requested brightness: "));
        //        Serial.println(_data.ch[chIndex].brightness[stepIndex - 1]);

      } else {
        //Serial.println(F("J >= mapSize"));
        timeDiff =  1000ul * abs( (intTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex - 1], _data.ch[chIndex].stepTimeMinutes[stepIndex - 1] )) -  _offTime );
        brightDiff = _data.ch[chIndex].brightness[stepIndex - 1 ] - _data.ch[chIndex].moonlightBright;

        //        Serial.print(F("Current requested brightness: "));
        //        Serial.println(_data.ch[chIndex].moonlightBright);
        //        Serial.print(F("Previous requested brightness: "));
        //        Serial.println(_data.ch[chIndex].brightness[stepIndex - 1]);

      }
      if (timeDiff >= brightDiff) {
      _data.ch[chIndex].fadeDelay[stepIndex] = timeDiff / brightDiff;
      } else {
        
      }
      // Serial.print(F("fadeDelay: "));
      // Serial.print(_data.ch[chIndex].fadeDelay[stepIndex]);
      // Serial.println();
    }
  }
  endTime = micros();
  // Serial.println(F("Done Calculating Fade Map"));
  // Serial.print(F("Fade Map calculation took "));
  // Serial.print(endTime - startTime);
  // Serial.println(F(" microseconds"));
}

// "inject" us into the correct part of fade if we turn on during On Period
void Light::calcStartStep() {

  if ( isLightTime() ) {

    bool positionFound[_numChannels] = {false, false, false, false, false, false}; // So we know when to stop searching for a channel

    // DEBUG
    // Serial.println();
    // Serial.println(F("Calculating Start steps..."));
    //    Serial.println();
    //    Serial.print(F("Current Time: "));
    //    Serial.println(_currentTime);
    //    Serial.print(F("On Time: "));
    //    Serial.println( byteTimesToTime_t(_data.onTimeHours, _data.onTimeMinutes) );
    //    Serial.print(F("Off Time: "));
    //    Serial.println( byteTimesToTime_t(_data.onTimeHours, _data.onTimeMinutes) );
    //    Serial.println();

    for (byte chIndex = 0; chIndex < _numChannels ; chIndex++) {  // iterate through channels
      //      Serial.println();
      //      Serial.print(F("Channel #"));
      //      Serial.println(chIndex);

      // iterate through step times for each channel, there is an additional
      // fade period between array and off/nightlight time
      for (byte stepIndex = 0; stepIndex <= _mapSize; stepIndex++) {

        if (!positionFound[chIndex]) {

          //          Serial.print(F("Checking Step #"));
          //          Serial.println(stepIndex);

          if (stepIndex == 0) {     // If index is on its first pass
            //  DEBUG
            //                        Serial.println(F("Checking position (stepIndex == 0)"));
            //                        Serial.print(F("Index: "));
            //                        Serial.println(stepIndex);
            //                        Serial.print(F("Current Time: "));
            //                        Serial.println(_currentTime);
            //                        Serial.print(F("On Time:"));
            //                        Serial.println( byteTimesToTime_t(_data.onTimeHours, _data.onTimeMinutes) );
            //                        Serial.print(F("Step Time: "));
            //                        Serial.println( byteTimesToTime_t( _data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex] ) );

            // Check to see if this is the correct step to start on if we start in the middle of a fade cycle
            if (_currentTime > byteTimesToTime_t(_data.onTimeHours, _data.onTimeMinutes)) {
              if (_currentTime < byteTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex] )  ) {
                // If this is the correct index window for current time
                _data.ch[chIndex].fadeIndex = stepIndex;    // Save our index for this channel
                positionFound[chIndex] = true;

                // Serial.print(F("Found fade start point ch #"));
                // Serial.println(chIndex + 1);
                // Serial.print(F("Map Index: "));
                // Serial.println(_data.ch[chIndex].fadeIndex);
              }
            }
          }
          else if (( stepIndex > 0 ) && (stepIndex < _mapSize  )) {
            //Serial.println(F("J > 0 && stepIndex < mapSize"));

            //                        Serial.println(F("Checking position (stepIndex < _mapSize)"));
            //                        Serial.print(F("Index: "));
            //                        Serial.println(stepIndex);
            //                        Serial.print(F("Current Time: "));
            //                        Serial.println(_currentTime);
            //                        Serial.print(F("Current Step Time:"));
            //                        Serial.println( byteTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex] ));
            //                        Serial.print(F("Next Step Time: "));
            //                        Serial.println(byteTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex + 1], _data.ch[chIndex].stepTimeMinutes[stepIndex + 1]));

            // Check to see if this is the correct step to start on if we start in the middle of a fade cycle
            if (_currentTime > byteTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex - 1], _data.ch[chIndex].stepTimeMinutes[stepIndex - 1])) {
              if (_currentTime < byteTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex])) {
                _data.ch[chIndex].fadeIndex = stepIndex;
                _data.ch[chIndex].stepCount = map(_currentTime, byteTimesToTime_t( _data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex]), byteTimesToTime_t( _data.ch[chIndex].stepTimeHours[stepIndex - 1], _data.ch[chIndex].stepTimeMinutes[stepIndex]), _data.ch[chIndex].brightness[stepIndex - 1] , _data.ch[chIndex].brightness[stepIndex]);
                positionFound[chIndex] = true;


                //Serial.println();
                // Serial.print(F("Found fade start point ch #"));
                // Serial.println(chIndex + 1);
                // Serial.print(F("Map Index: "));
                // Serial.println(_data.ch[chIndex].fadeIndex);
              }

            }

          } else {
            //            Serial.println(F("stepIndex >= _mapSize"));
            //                        Serial.println(F("Checking position (else)"));
            //                        Serial.print(F("Index: "));
            //                        Serial.println(stepIndex);
            //                        Serial.print(F("Current Time: "));
            //                        Serial.println(_currentTime);
            //                        Serial.print(F("On Time:"));
            //                        Serial.println(byteTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex]));
            //                        Serial.print(F("Off Time: "));
            //                        Serial.println( byteTimesToTime_t(_data.offTimeHours, _data.onTimeMinutes ));

            // Check to see if this is the correct step to start on if we start in the middle of a fade cycle
            if (_currentTime > byteTimesToTime_t(_data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex])) {
              if (_currentTime < byteTimesToTime_t(_data.offTimeHours, _data.offTimeMinutes ) ) {
                _data.ch[chIndex].fadeIndex = stepIndex;
                _data.ch[chIndex].stepCount = map(_currentTime, byteTimesToTime_t( _data.ch[chIndex].stepTimeHours[stepIndex], _data.ch[chIndex].stepTimeMinutes[stepIndex]), byteTimesToTime_t( _data.offTimeHours, _data.offTimeMinutes) , _data.ch[chIndex].brightness[stepIndex], 0);
                positionFound[chIndex] = true;

                //Serial.println();
                // Serial.print(F("Found fade start point ch #"));
                // Serial.println(chIndex + 1);
                // Serial.print(F("Index: "));
                // Serial.println(_data.ch[chIndex].fadeIndex);


              }
            }
            // Serial.println();

          }

        }
      }
    }

    // Serial.println(F("Done calculating start steps"));
    // Serial.println("");
  }
}

// Test function to make sure everything is working right
// Uses a set fade time and fades to full on (maxBright) @ on time and full off @ off time
void Light::testLights() {
  static int currentBright = 0;

  static unsigned long prevFadeTime = millis();
  static bool isRising;
  //Serial.println("------------------------------------------");
  //Serial.print("Current Millis: ");
  //Serial.println(currentMillis);
  //Serial.print("Previous Fade Time: ");
  //Serial.println(prevFadeTime);

  if (_currentTime - prevFadeTime >= _data.testFadeDelay) {
    //Serial.println("Time to adjust light");
    if (isRising) {
      if (currentBright < 4096) {
        currentBright++;
      } else {
        isRising = false;
      }
    } else {
      if (currentBright > 0) {
        currentBright--;
      } else {
        isRising = true;
      }
    }
    for (int chIndex = 0; chIndex < 6; chIndex++) {
      //Serial.println("Adjusting Light");
      _pwm->setPin(chIndex, currentBright, 0);
    }
    prevFadeTime = _currentTime;
  }
}

/////////////////////////////////////////////////////////////////////////
/////////////////////////Getters and Setters////////////////////////////
////////////////////////////////////////////////////////////////////////

// Set Mode - Enum = NORMAL, CUSTOM, TEST
void Light::setMode(byte mode) {
  _data.mode = mode;
}

// Returns Mode Enum
byte Light::getMode() {
  return _data.mode;
}

// set time with a time_t
void Light::setOnTime(time_t onTime) {
  _data.onTimeHours = hour(onTime);
  _data.onTimeMinutes = minute(onTime);

}

// returns on time as a time_t
time_t Light::getOnTime() {
  return intTimesToTime_t(_data.onTimeHours, _data.onTimeMinutes);
}

// set off time with a time_t
void Light::setOffTime(time_t offTime) {
  _data.offTimeHours = hour(offTime);
  _data.onTimeHours = minute(offTime);
}

time_t Light::getOffTime() {
  return intTimesToTime_t(_data.offTimeHours, _data.offTimeMinutes);
}
