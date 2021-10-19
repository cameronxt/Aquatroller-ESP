// TODO: SD Card: Store Settings & Save Logs

// Optional Modules. comment to disable
#define LIGHT_CONTROL
#define TEMP_CONTROL
#define PH_CONTROL
#define C02_CONTROL
#define ATO_CONTROL

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

// Standard includes
#include <WiFi.h> // WiFi Driver
#include <ArduinoOTA.h> // OTA Library
#include <Wire.h> // Talks to the ds18b20
#include <esp_log.h>
#include <Preferences.h>
#include "include/time.h"

// My Libraries
//#include "eepromaccess.h"
#include "bluetooth.h"
#include "temp.h"
#include "lights.h"
#include "ph.h"
#include "ato.h"
#include "secrets.h"
#include "PrefAccess.h"

// Pin Definitions
#define PH_SENSOR_PIN 39
#define C02_VALVE_PIN 2
#define OPTICAL_SWITCH_PIN 36
#define ALARM_SWITCH_PIN 34
#define WATER_VALVE_PIN 0
#define ONE_WIRE_PIN 4

// TODO: Move into myTime and make changable/savable
// NFS server
const char  ntpServer[32] = "pool.ntp.org";
const long  gmtOffset_sec = -21400;
const int   daylightOffset_sec = 3600;

// constants for number of seconds in standard units of time
const unsigned long SecondsPerHour = 60UL * 60;
const unsigned long SecondsPerMinute = 60UL;




////////////// instance declaration //////////////////////
Preferences preferences;
// PrefAccess prefs; // Object for retrieving and saving preferences

BluetoothModule bt;     // Create bt instance
//SDAccess sd;            // create SD card instance

PH ph(PH_SENSOR_PIN, C02_VALVE_PIN);     // Create Ph Controller - Inputs = (PH Input Pin, C02 Relay Trigger Pin, Pointer to RTC class)
AutoTopOff ato(OPTICAL_SWITCH_PIN, ALARM_SWITCH_PIN, WATER_VALVE_PIN);                // Create ATO instance

// PWM and LEDS
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();  // Setup PWM Driver
//Light light(&pwm);  // Setup my light, needs the driver and the time

// Temp Sensors
OneWire oneWire(ONE_WIRE_PIN);   // Setup onewire connection for comms with temp sensor(s)
DallasTemperature tempSensors(&oneWire);    // tell temp sensor library to use oneWire to talk to sensors
Temp temp(&tempSensors);                    // Tell the temp control library which sensors to use

//EepromAccess eeprom(ph.getDataAddress(), light.getDataAddress(), temp.getDataAddress());   // Create eepromAccess class, send it a reference of everything that needs saved

////////////// instance declaration //////////////////////




////////////// RTOS task functions /////////////

// Monitor ATO overfill alarm switch
void TaskATOAlarm( void *pvParameters ) {
  static UBaseType_t prevMarkATOAlarm = NULL;
  vTaskDelay(ato.getAlarmCheckDelay() / portTICK_PERIOD_MS);
  for(;;) {
    ato.alarmLoop();
    UBaseType_t highMark = uxTaskGetStackHighWaterMark( NULL );
    if(highMark != prevMarkATOAlarm) {
      ESP_LOGD("ATOAlarm", "ATO Alarm Task Highwater: %u", highMark);
      prevMarkATOAlarm = highMark;
    }
  }
  vTaskDelete( NULL );
}

// Control ATO valve based on result from optical level sensor
void TaskATOValve( void *pvParameters ) {
  static UBaseType_t prevMarkATOValve = NULL;
  vTaskDelay(ato.getValveCycleDelay() / portTICK_PERIOD_MS);
  for(;;) {
    ato.valveLoop();
    UBaseType_t highMark = uxTaskGetStackHighWaterMark( NULL );
    if(highMark != prevMarkATOValve) {
      ESP_LOGD("ATOValve", "ATO Valve Task Highwater: %u", highMark);
      prevMarkATOValve = highMark;
    }
  }
  vTaskDelete( NULL );
}

// Monitor ph from analog ph sensor
void TaskPH( void *pvParameters ) {
  static UBaseType_t prevMarkPh = NULL;
  vTaskDelay(ph.getPhDelay() / portTICK_PERIOD_MS);
  for(;;) {
    ph.phLoop();
    UBaseType_t highMark = uxTaskGetStackHighWaterMark( NULL );
    if(highMark != prevMarkPh) {
      ESP_LOGD("PH", "PH Task Highwater: %u", highMark);
      prevMarkPh = highMark;
    }
    vTaskDelay(ph._phData.checkPhDelay / portTICK_PERIOD_MS);
  }
  vTaskDelete( NULL );
}

// Actuate c02 valve based on current ph vs desired drop
void TaskC02( void *pvParameters ) {
  static UBaseType_t prevMarkC02 = NULL;
  vTaskDelay(ph.getC02Delay() / portTICK_PERIOD_MS);
  for(;;) {
    ph.c02Loop();
    UBaseType_t highMark = uxTaskGetStackHighWaterMark( NULL );
    if(highMark != prevMarkC02) {
      ESP_LOGD("C02", "C02 Task Highwater: %u", highMark);
      prevMarkC02 = highMark;
    }
  }
  vTaskDelete( NULL );
}

// TODO: backup settings to SD every so often ?
// TODO: generic log to file function (psuedo)
//          - create buffer to store log messages
//          - once buffer is full or after certain time period
//          - logToSd(String logCaller, String[] msgToLog);
//          - logCaller will have ".log" added to create file name
//          - msgToLog is a string array of the message from buffer to log  
//          - check if file exist
//          - if not, make one
//          - open the file
//          - append message(s) to log file
//          - close file
// TODO: Check if file exists
// TODO; Check if directory exist
// TODO: Make new directory -- logCaller will be directory name of log files
// TODO: Make new file -- logCaller + todaysDate + .log
// TODO: Rename file
// TODO: Delete file
// TODO: New file with new date name everyday. delete after n days
void TaskSD( void *pvParameters ) {
  for (;;) {
// Do SD card Things    
  }
  vTaskDelete( NULL );
}

// RTOS task to handle updating rtc with ntp
void TaskRTC( void *pvParameters ) {
  static UBaseType_t prevMarkRTC = NULL;
  vTaskDelay(100);
  for(;;) {
    // Do RTC things here
    UBaseType_t highMark = uxTaskGetStackHighWaterMark( NULL );
    if(highMark != prevMarkRTC) {
      ESP_LOGD("RTC", "RTC Task Highwater: %u", highMark);
      prevMarkRTC = highMark;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete( NULL );
}

// RTOS Task to handle lights
void TaskLights( void *pvParameters ) {
  static UBaseType_t prevMarkLight = NULL;
  light.loop(now());  // Run light controls, it needs to know the current time
    light.init();       // set initial state and begin running routines
  vTaskDelay(100 / portTICK_PERIOD_MS);
  for (;;) {
    light.loop(now());  // Run light controls, it needs to know the current time
    //Serial.println("lights");
    UBaseType_t highMark = uxTaskGetStackHighWaterMark( NULL );
     if(highMark != prevMarkLight) {
      ESP_LOGD("Light", "Light Task Highwater: %u", highMark);
       prevMarkLight = highMark;
     }
  }
  vTaskDelete( NULL );
}

// RTOS function to run bluetoothfunctions
//    -- 
//    -- Connect/Disconnect
//    -- get/set settings (parsing data)
//    -- Transfer log file(s)
//    -- Time sync with phone (android app)
void TaskBluetooth( void *pvParameters ) {
  for (;;) {
    // Do Bluetooth Things
  }
  vTaskDelete( NULL );
}

// RTOS task to monitor and control temp
//   TODO: Split monitoring and control into independant tasks
void TaskTemp( void *pvParameters ) {
  static UBaseType_t prevMarkTemp = NULL;
  vTaskDelay(temp.getTempDelay() / portTICK_PERIOD_MS);
  for (;;) {
    temp.loop();

    UBaseType_t highMark = uxTaskGetStackHighWaterMark( NULL );
     if(highMark != prevMarkTemp) {
      ESP_LOGD("Temp", "Temperature Task Highwater: %u", highMark);
       prevMarkTemp = highMark;
     }
  }
  vTaskDelete( NULL );
}


// RTOS task to handle wifi comms and also maintain wifi connection
void TaskWifi( void *pvParameters ) {
  for (;;) {
    vTaskDelay( 10000 / portTICK_PERIOD_MS ); // 10 seconds
    // do wifi things
    reconnectToWifi();
  }
  vTaskDelete( NULL );
}

////////////////// End RTOS Functions //////////////////////////

void setup() {
  Serial.begin(115200);
  ESP_LOGI("SYSTEM","Welcome to Aquatroller!");
  delay(800);  // one of my esp32 devkit clones crashes without this

  connectToWifi();
  
  setupTime();    // setup routine, gets time from RTC and sets local time
  ESP_LOGI("SYSTEM", "%02u:%02u", hour(), minute());  

  // eeprom.setup();      // Check for existing save, load if found, else generate new save and populate with default values
  // sd.init();          // init sd card, TODO: if card not present dont try to log
  //connectToWifi();
  // bt.setup();          // init bluetooth comms
  // Serial.println(F("Bluetooth Enabled"));
  // temp.init();
  ph.init();
  ato.init();
  // Serial.print(F("Free Ram - Setup:"));
  // Serial.println( freeRam() );
  ESP_LOGD("SYSTEM","Loading Modules..");

  preferences.begin("MainApp", true); // true = read only, Setup NVS for main app preferences 

  int appVersion = preferences.getInt( "Version", 0);

  if ( appVersion == 0) {
    ESP_LOGE("Prefs", "Error: No previous data found, please initialize the memory space");
  } else {
    ESP_LOGD("Prefs", "Found save data for version #$i", appVersion );
  }

  preferences.end();
  //prefs.init();
  
  // xTaskCreatePinnedToCore( TaskSD, "TaskSD", 1024, NULL, 2, NULL,  1);

  // #ifdef LIGHT_CONTROL  
  //   if ( xTaskCreatePinnedToCore( TaskLights, "TaskLights", 2000, NULL, 2, NULL,  1) == pdPASS) {
  //     ESP_LOGI("light", "light task started");
  //   } else {
  //     ESP_LOGE("light", "light task failed to start");
  //   }
  // #endif
  
  // #ifdef PH_CONTROL
  //   if ( xTaskCreatePinnedToCore(TaskPH, "TaskPH", 2700, NULL, 2, NULL, 1) == pdPASS) {
  //     ESP_LOGI("PH", "PH task started");
  //   } else {
  //     ESP_LOGE("PH", "PH task failed to start");
  //   }
  // #endif

  // #ifdef C02_CONTROL
  //   if ( xTaskCreatePinnedToCore(TaskC02, "TaskC02", 1700, NULL, 2, NULL, 1) == pdPASS) {
  //     ESP_LOGI("C02", "C02 task started");
  //   } else {
  //     ESP_LOGE("C02", "C02 task failed to start");
  //   }
  // #endif

  // // xTaskCreatePinnedToCore( TaskBluetooth, "TaskBluetooth", 1024, NULL, 2, NULL,  1);
  // #ifdef TEMP_CONTROL
  //   if ( xTaskCreatePinnedToCore(TaskTemp, "TaskTemp", 2000, NULL, 2, NULL, 1) == pdPASS) {
  //     ESP_LOGI("Temperature", "Temperature task started");
  //   } else {
  //     ESP_LOGE("Temperature", "Temperature task failed to start");
  //   }
  // #endif

  #ifdef ATO_CONTROL
    if ( xTaskCreatePinnedToCore(TaskATOAlarm, "TaskATOAlarm", 1700, NULL, 2, NULL, 1) == pdPASS) {
      ESP_LOGI("ATOALARM", "ATO Alarm task started");
    } else {
      ESP_LOGE("ATOAlarm", "ATO Alarm task failed to start");
    }

    if ( xTaskCreatePinnedToCore(TaskATOValve, "TaskATOValve", 1700, NULL, 2, NULL, 1) == pdPASS) {
      ESP_LOGI("ATOValve", "ATO Valve task started");
    } else {
      ESP_LOGE("ATOValve", "ATO Valve task failed to start");
    }
  #endif

  OtaSetup(); // Setup ota over wifi
  ESP_LOGI("SYSTEM", "Aquatroller Ready");

}

void loop() {

  // unsigned long currentTime;
  // currentTime = millis();
  now();  // Process for keeping track of time
  ArduinoOTA.handle();  // Check for incoming OTA update
  // This is a non blocking bluetooth implementation. Thanks to Robin2's mega post for most of this code
  //bt.loop();          // Check for and save valid packets
 // if (bt.newParse) {              // If we have a new parsed packet
 //   decodePacket(bt.parsedData);  // Decode and perform correct call
 //   bt.newParse = false;          // Set to false so we can get a new packet
  //}

  //temp.loop(currentTime);
  //light.loop(now());  // Run light controls, it needs to know the current time
  //ph.loop(currentTime);
  // eeprom.loop();



  //    Serial.print(F("Free Ram - Loop:"));
  //    Serial.println(freeRam());
  // timeClient.update();
  delay(1000);
}

// TODO: Testing to verify it all work
void decodePacket(BTParse data) { // Decides which actions should be taken on input packet

  switch (data.primary) {
    ////////// EEPROM Actions //////////////////
    case 0: // EEPROM
      switch (data.option) {
        case 0: // Get Data from EEPROM
          Serial.println(F("Getting Data from EEPROM"));
          // eeprom.getSettings();
          break;
        case 1: // Save Data to EEPROM
          Serial.println(F("Saving Data to EEPROM"));
          // eeprom.updateSettings();
          break;
        case 2: // Reset EEPROM
          Serial.println(F("Ressetting Data on EEPROM"));
          // eeprom.resetEeprom();
          break;
        default:
          Serial.println(F("Invalid Option Argument"));
          break;
      }
      break;
    /////////////// LED Actions /////////////////
    case 1: // LED's
      switch (data.option) {
        case 0: // Get Target LED Brightness
          Serial.print(F("Target Brightness: "));                   //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
          //light.getBright(data.subOption);
          break;
        case 1: // Set target brightness                            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
          // light.setBright((data.value, bt.parsedData.subOption);
          break;
        case 2: // Get Light Mode
          Serial.print(F("Light Mode: "));
          //Serial.println(light.getMode());
          break;
        case 3: // Set Light Mode
          //light.setMode(data.value);
          break;
        case 4: // Get LED On Time
          Serial.print(F("Light on time: "));
          //Serial.println(light.getOnTime());
          break;
        case 5: // Set LED on time
          //light.setOnTime(data.value);
          break;
        case 6: // Get LED Off Time
          Serial.print(F("Light off time: "));
          //Serial.println(light.getOffTime());
          break;
        case 7: // Set LED Off time
          //light.setOffTime(data.value);
          break;

        default:
          Serial.println(F("Invalid Option Argument"));
          break;
      }
      break;
    /////////////// SD Card Actions ////////////////
    case 2:
      // TODO: Add SD cards actions
      // Save/Log data
      // -- Temperature
      // -- PH/C02
      // -- Lights
      // Settings Backup/Restore
      // -- Check for backup file
      // -- Restore backup file
      // -- Create backup file
      break;
    /////////////// Bluetooth Actions //////////////
    case 3:
      // TODO: Bluetooth actions
      // Connect/disconnect
      // Change Name
      // Send Logs
      // Live Data Stream
      break;
    /////////////// PH Actions /////////////////////
    case 4:
      switch (data.option) {
        case 0: // Get target PH
          Serial.print(F("Target PH: "));
          Serial.println(ph.getTargetPh());
          break;
        case 1: // Set Target PH
          ph.setTargetPh(data.values.fValue);
          break;
        case 2: // Get Heater delay time
          Serial.print(F("PH Reading Delay: "));
          Serial.println(ph.getPhDelay());
          break;
        case 3: // Set heater delay
          ph.setPhDelay(data.values.lValue);
          break;
        case 4: // Get C02 PH Target
          Serial.print(F("PH Target - C02: "));
          Serial.println(ph.getC02PhTarget());
          break;
        case 5: // Set C02 PH Target
          ph.setC02PhTarget(data.values.fValue);
          break;
        case 6: // Get target PH drop with c02
          Serial.print(F("Target PH Drop with C02: "));
          Serial.println(ph.getTargetPhDrop());
          break;
        case 7: // Set target PH drop with c02
          ph.setTargetPhDrop(data.values.fValue);
          break;
        case 8: // Get target PPM C02
          Serial.print(F("Target PPM C02: "));
          Serial.println(ph.getTargetPPMC02());
          break;
        case 9: // Set PPM C02
          ph.setTargetPPMC02(data.values.iValue);
          break;
        case 10: // Get Kh Hardness
          Serial.print(F("Kh Hardness: "));
          Serial.println(ph.getKhHardness());
          break;
        case 11: // Set Kh hardness
          ph.setKhHardness(data.values.fValue);
          break;
        case 12: // Get c02 on time
          Serial.print(F("C02 on time: "));
          Serial.println(ph.getC02OnTime());
          break;
        case 13: // Set c02 on time
          ph.setC02OnTime(data.values.lValue);
          break;
        default:
          Serial.println(F("Invalid Option Argument"));
          break;
      }
      break;
    //////////////// Temperature Actions ///////////////////////
    case 5: // Temperature
      switch (data.option) {

        case 0: // Get target temperature
          Serial.print(F("Target Temp: "));
          Serial.println(temp.getTargetTemp());
          break;
        case 1: // Set Target Temp
          temp.setTargetTemp(data.values.fValue);
          break;
        case 2: // Get Heater delay time
          Serial.print(F("Heater Delay: "));
          Serial.println(temp.getHeaterDelay());
          break;
        case 3: // Set heater delay time
          temp.setHeaterDelay(data.values.lValue);
          break;
        case 4:  // Get Temperature check delay
          Serial.print(F("Temperature Delay: "));
          Serial.println(temp.getTempDelay());
          break;
        case 5:  // Set temperature check delay
          temp.setTempDelay(data.values.lValue);
          break;
        case 6:
          Serial.print(F("Target Temperature: "));
          Serial.println(temp.getTargetTemp());
          break;
        case 7:
          temp.setTargetTemp(data.values.fValue);
          break;
        default:
          Serial.println(F("Invalid Option Argument"));
          break;
      }
      break;
    //////////// Time Actions ////////////////////////
    case 6:
      switch (data.option) {
        case 0: // Get Current Time in SSM TODO: Testing !!!!!!!!!!!!!!
          // Serial.print(F("Seconds Since Midnight: "));
          // Serial.println(mytime.getTimeInSeconds());
          // Serial.print(F("On Time: "));
          // Serial.println(light.getOnTime());
          // Serial.print(F("Off Time: "));
          // Serial.println(light.getOffTime());

          break;

          case 1: // Set Current Time

          break;
        default:
          Serial.println(F("Invalid Option Argument"));
          break;
      }
      break;
    //////////// Soft Reset /////////////////////////
    case 9:
      Serial.println(F("Reset Command Recieved"));
      if (data.option == 1 && data.subOption == 1) {
        Serial.println(); Serial.println(); Serial.println(); Serial.println(); Serial.println(); Serial.println();
        Serial.println(); Serial.println(); Serial.println(); Serial.println(); Serial.println(); Serial.println();
        Serial.println(F("!!!!!!!!!!!!!!!!"));
        Serial.println(F("Restarting Device"));
        Serial.println(); Serial.println();
        delay(2000);
        ESP.restart();

      }
      break;
    default:
      Serial.println(F("Invalid Primary Argument"));
      break;
  }
  // Serial.print(F("Free Ram - Command Parser:"));
  // Serial.println(freeRam());
}


int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

// Creates new connection to wifi network
void connectToWifi() { 
 WiFi.mode(WIFI_STA);
 WiFi.begin(PRIVATE_SSID, PRIVATE_PASSWORD);

  while ( WiFi.status() != WL_CONNECTED ) {
    vTaskDelay( 500 / portTICK_PERIOD_MS );
    Serial.print ( "." );
  }
  Serial.println("");
  Serial.println("Connected");
}

void reconnectToWifi() {
  if (WiFi.status() != WL_CONNECTED) {
    ESP_LOGW("WiFi", "Wifi disconnected, attempting to re-connect");
    WiFi.disconnect();
    WiFi.reconnect();
    while (WiFi.status() != WL_CONNECTED) { 
      vTaskDelay( 500 / portTICK_PERIOD_MS );
      Serial.println( "." );
    }
    ESP_LOGW("WiFi", "Wifi reconnected succesfully");
    } else {
    ESP_LOGV("System", "WiFi already connected");
  }
}

time_t getNtpTime() {
  //timeClient.update();
  time_t ntpTime = millis(); // TESTING: need to make actual call
  ESP_LOGI("NTP", "NTP has synced the time to %i", ntpTime );
  return ntpTime;
}


void setupTime() {

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    
}

void OtaSetup() {
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.setHostname("Aquatroller");
  ArduinoOTA.begin();
}

// Returns time in seconds since previous midnight
// Takes int for hour and minute and seconds, returns UL seconds. Zero is valid input
unsigned long getTimeInSeconds(int hours, int minutes, int seconds) {
    return ((hours * SecondsPerHour) + (minutes * SecondsPerMinute) + seconds);
}

unsigned long getTimeInSeconds (time_t getTime) {         // Calculate seconds since midnight for given time_t
  return ((hour(getTime) * SecondsPerHour) + (minute(getTime) * 60) + second(getTime));
}

unsigned long getTimeInSeconds () {         // Calculate seconds since midnight as of now
  return ((hour() * SecondsPerHour) + (minute() * SecondsPerMinute) + second());
}
