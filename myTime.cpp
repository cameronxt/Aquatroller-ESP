#include "myTime.h"

MyTime::MyTime() {

}

void MyTime::init() {
  configTime(timeData.gmtOffset_sec, timeData.daylightOffset_sec, timeData.ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Function to get current NTP time
tm MyTime::getNtpTime() {
  tm ntpTime;
  getLocalTime( &ntpTime ); // TESTING: need to make actual call
  Serial.println(&ntpTime, "NTP has synced the time to %H:%M:%S" );
  return ntpTime;
}

// Returns time in seconds since previous midnight
// Takes int for hour and minute and seconds, returns UL seconds. Zero is valid input
unsigned long MyTime::getTimeInSeconds(int hours, int minutes, int seconds) {
    return ((hours * SecondsPerHour) + (minutes * SecondsPerMinute) + seconds);
}

// Calculate seconds since midnight for given tm
unsigned long MyTime::getTimeInSeconds (tm* getTime) {         
  return ((getTime->tm_hour * SecondsPerHour) + (getTime->tm_min * 60) + getTime->tm_sec );
}

unsigned long MyTime::getTimeInSeconds () {         // Calculate seconds since midnight as of now
tm currentTime;
getLocalTime( &currentTime );
  return ((currentTime.tm_hour * SecondsPerHour) + (currentTime.tm_min * SecondsPerMinute) + currentTime.tm_sec );
}